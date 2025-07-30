from __future__ import annotations
"""auto-gtp.py
A small framework to run head‑to‑head Othello/Reversi matches between text‑mode
engines that speak (a subset of) the Go Text Protocol (GTP).

Requirements
------------
• Python ≥ 3.9
 Executable paths supplied via *config.py*::

    leelaz      = "C:/engines/leelaz.exe"
    edax        = "C:/engines/edax.exe"
    kalmia      = "C:/engines/kalmia.exe"
    egaroucid   = "C:/engines/egaroucid.exe"
    rundir       = "C:/tmp/engine_workdir"
    results_root = "C:/tmp/othello_results"
    leelaz_args  = ["-g", "-q"]
    match_args   = ["-g", "-q"]

Edit those paths and the default Leela arguments to suit your setup.
"""

from dataclasses import dataclass, field
from pathlib import Path, PureWindowsPath
from multiprocessing import Pool, cpu_count
from uuid import uuid4
import subprocess
import tarfile
import csv
import hashlib
import gzip
import os
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading
import itertools

import pyopencl as cl
from typing import List, Tuple, Dict, Optional

# ---------------------------------------------------------------------------
# Configuration helpers (imported from user's config.py)
# ---------------------------------------------------------------------------

try:
    from config import (
        leelaz,
        match_args,
        edax,
        kalmia,
        egaroucid,
        rundir,
        results_root,
        save_gen_dir_,
        network,
        dummy_network,
        adri_gen_dir_
    )
except ImportError as exc:
    raise RuntimeError("config.py with engine paths is required") from exc

LEELA_DEFAULT_VISITS = 10000  # default visits for Leela
SGF_HEADER = "(;GM[2]FF[4]SZ[8]"
SGF_FOOTER = ")\n"
NGPUS = sum(len(platform.get_devices()) for platform in cl.get_platforms())

Path(results_root).mkdir(parents=True, exist_ok=True)

# ---------------------------------------------------------------------------
# Miscellaneous helpers
# ---------------------------------------------------------------------------

def guess_path_flavor(path: str) -> str:
    """
    Heuristically determines if a string path looks like Windows or POSIX.

    Returns:
        "windows", "posix", or "unknown"
    """
    if "\\" in path:
        # Windows uses backslashes — POSIX never does
        return "windows"
    if "/" in path:
        # POSIX uses forward slashes — Windows can too, but usually doesn't
        return "posix"
    return "unknown"

def win_to_wsl_path(win_path: str | os.PathLike) -> str:
    """Convert an absolute Windows file‑system path to its WSL counterpart."""
    path_flavor = guess_path_flavor(win_path)
    if path_flavor == "posix":
        print(f"Warning: {win_path} looks like and will be used as a posix path.")
        return win_path
    elif path_flavor == "windows":
        p = PureWindowsPath(str(win_path))
        if not p.drive:
            raise ValueError(f"Cannot convert relative path '{win_path}'.")
        drive = p.drive.rstrip(":").lower()
        rest = "/".join(p.parts[1:])
        return f"/mnt/{drive}/{rest}"
    else:
        raise ValueError(f"{win_path} does not look like a windows or posix path.")


def send_cmd(proc: subprocess.Popen, cmd: str) -> Tuple[str, List[str]]:
    """Send *one* GTP command and wait for the reply (header + payload)."""
    proc.stdin.write(cmd + "\n")
    proc.stdin.flush()
    header = proc.stdout.readline()
    payload: List[str] = []
    while True:
        line = proc.stdout.readline()
        if line in ("\n", ""):
            break
        payload.append(line.rstrip("\n"))
    return header.lstrip("= ?").strip(), payload


def sha256_short(path: str | Path, n: int = 8) -> str:
    path = Path(path)
    sha_path = path.with_suffix('.sha256')
    if not sha_path.exists():
        print(f"{sha_path} not found. Will compute hash on the fly.")
        h = hashlib.sha256()
        with gzip.open(path, "rb") as f:
            for chunk in iter(lambda: f.read(8192), b""):
                h.update(chunk)
        sha256sum = h.hexdigest()
        sha_path.write_text(sha256sum+" "+path.name)
    else:
        sha256sum = sha_path.read_text().strip().split()[0]

    return sha256sum[:n]


# ---------------------------------------------------------------------------
# Engine wrapper
# ---------------------------------------------------------------------------

@dataclass
class Engine:
    name: str
    exe: str | Path
    category: str
    extra: List[str] = field(default_factory=list)
    proc: subprocess.Popen | None = field(init=False, default=None)
    cwd: str | Path | None = None


    # ---- lifecycle ---------------------------------------------------------
    def reset(self):
        send_cmd(self.proc, "boardsize 8")
        send_cmd(self.proc, "clear_board")
        if self.category == "leelaz":
            send_cmd(self.proc, "clear_cache")

    def start(self):
        if self.proc is not None:
            self.reset()
            return
        self.proc = subprocess.Popen(
            [self.exe, *self.extra],
            cwd=self.cwd,
            text=True,
            bufsize=1,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            # stderr=subprocess.STDOUT,
        )
        self.reset()
        
    def restart(self, soft: bool = False):
        if self.proc is None:
            self.start()
        elif soft:
            self.reset()
        else:
            self.close()
            self.start()

    def close(self, soft: bool = False):
        if self.proc is None:
            return
        if soft:
            self.reset()
            return
        try:
            send_cmd(self.proc, "quit")
        except Exception:
            pass
        try:
            self.proc.kill()
        finally:
            self.proc = None

    # ---- gameplay helpers -------------------------------------------------
    def genmove(self, color: str) -> str:
        header, _ = send_cmd(self.proc, f"genmove {color}")
        return header.strip()

    def play_move(self, color: str, move: str):
        send_cmd(self.proc, f"play {color} {move}")

    def final_score(self) -> str | None:
        if self.category != "leelaz":
            # Only leelaz has final_score command
            return None
        header, _ = send_cmd(self.proc, "final_score")
        return header

    def short(self) -> str:
        return self.name.replace(" ", "_")

    # ---- cloning -----------------------------------------------------------
    def clone(self) -> "Engine":
        """
        Return a *stopped* engine with the same config.
        Subclasses should override to reconstruct from their own ctor args.
        """
        return Engine(
            name=self.name,
            exe=self.exe,
            category=self.category,
            extra=list(self.extra),  # copy mutable list
            cwd=self.cwd,
        )


class LeelaEngine(Engine):
    def __init__(self,
                 net_path: str | Path = dummy_network,
                 visits: int = LEELA_DEFAULT_VISITS,
                 exe: str | Path = None,
                 cpu_only: bool = False,
                 name: str | None = None,
                 **kwargs):

        # --- persist ctor args for cloning ---
        self.net_path = Path(net_path)
        self.visits = int(visits)
        self.cpu_only = bool(cpu_only)
        self.gpu: Optional[int] = None

        if exe is None:
            exe = leelaz
            code = "default"
            self.std_othello = True
        else:
            code = Path(exe).parts[-2] # adri, flip, ...
            self.std_othello = not (code == "adri")
            if not self.std_othello:
                print("Warning: LZ-adri can play only with itself, because of its flipped starting position.")

        net_path_linux = self.net_path
        if Path(exe).suffix == ".exe":
            # if Windows executable
            assert guess_path_flavor(str(self.net_path)) == "windows"
            net_path_linux = Path(win_to_wsl_path(self.net_path))
        net_hash = sha256_short(net_path_linux)

        if name is None:
            name = "LZ-" + code + "-" + net_hash + "-" + str(visits)
        extra = match_args + ["-w", str(self.net_path), "-v", str(self.visits)]
        if self.std_othello:
            extra += ["--std-othello"]

        super().__init__(
            name=name,
            exe=exe,
            category="leelaz",
            extra=extra,
            cwd=rundir,
            **kwargs
        )

    def set_gpu(self, gpu: int) -> None:
        if self.gpu is not None:
            # Already pinned to a GPU; do not change implicitly
            return
        self.gpu = int(gpu)

        # If already started, we must restart with the new flag
        was_running = self.proc is not None
        if was_running:
            self.close()

        # add flag exactly once
        if "--gpu" not in self.extra:
            self.extra += ["--gpu", str(self.gpu)]

        if was_running:
            self.start()

    def clone(self) -> "LeelaEngine":
        """
        Rebuild from semantic args, then carry over GPU pin (if any).
        """
        clone = LeelaEngine(
            net_path=self.net_path,
            visits=self.visits,
            exe=self.exe,
            cpu_only=self.cpu_only,
            name=self.name,
        )
        if self.std_othello and "--std-othello" not in clone.extra:
            clone.extra += ["--std-othello"]
        if self.gpu is not None:
            clone.set_gpu(self.gpu)
        return clone


class EdaxEngine(Engine):
    def __init__(self,
                 level: int | None = None,
                 book: bool | None = None,
                 **kwargs):
        self.level = level
        self.book = book

        extra = ["--gtp", "-vv"]

        if level is not None:
            extra += ["-l", str(level)]

        code = "max" if level is None else str(level)

        if book is not None:
            bu_str = "on" if book else "off"
            extra += ["-book-usage", bu_str]
            code += "-bu_" + bu_str

        name = "Edax-" + code
        super().__init__(
            name=name,
            exe=edax,
            category="edax",
            extra=extra,
            cwd=Path(edax).parent,
            **kwargs
        )

    def clone(self) -> "EdaxEngine":
        return EdaxEngine(level=self.level, book=self.book)

class KalmiaEngine(Engine):
    def __init__(self, strength: str = "superhuman", **kwargs):
        self.strength = strength
        code = {"custom":       0,
                "easy":         1,
                "normal":       2,
                "proffesional": 3, # typo in Kalmia args
                "superhuman":   4}
        assert strength in code
        
        name = f"Kalmia-{code[strength]}"
        super().__init__(
            name=name,
            exe=kalmia,
            category="kalmia",
            extra=["--mode", "gtp", "--difficulty", strength],
            cwd=Path(kalmia).parent,
            **kwargs
        )

    def clone(self) -> "KalmiaEngine":
        return KalmiaEngine(strength=self.strength)


class EgaroucidEngine(Engine):
    def __init__(self):
        super().__init__("egaroucid", egaroucid, "egaroucid")

# ---------------------------------------------------------------------------
# SGF helpers
# ---------------------------------------------------------------------------

ALPHA = "abcdefgh"
BETA  = "12345678"

def coord_to_sgf(move: str) -> str | None:
    """Return SGF coordinate (e.g., 'd4') or None for a pass."""
    move = move.strip()
    if move.lower() == "pass":
        return None

    if len(move) != 2:
        raise ValueError("coordinate must be exactly two characters")

    x, y = move[0].lower(), move[1]
    if x not in ALPHA:
        raise ValueError(f"x‑coordinate {x!r} not in {ALPHA!r}")
    if y not in BETA:
        raise ValueError(f"y‑coordinate {y!r} not in {BETA!r}")

    return f"{x}{y}"


# ---------------------------------------------------------------------------
# Game driver
# ---------------------------------------------------------------------------




@dataclass
class GameResult:
    moves: int
    result: str
    sgf: Path
    point: List[bool] = field(default_factory=lambda: [False, False, False])

    def __post_init__(self):
        if self.result.startswith("B"):
            self.point[0] = True
        if self.result.startswith("0"):
            self.point[1] = True
        if self.result.startswith("W"):
            self.point[2] = True
        assert sum(self.point) == 1
            
    
class Game:
    def __init__(self,
                 black: Engine,
                 white: Engine,
                 no: int,
                 match_id: str,
                 out_dir: Path,
                 soft_restart: bool = False):
        self.B = black
        self.W = white
        self.no = no # progressive number within the match
        self.mid = match_id
        self.out = out_dir
        self.soft_restart = soft_restart

        if self.B.category == "leelaz":
            self.referee = self.B
            self.with_referee = False
        elif self.W.category == "leelaz":
            self.referee = self.W
            self.with_referee = False
        else:
            self.referee = LeelaEngine()
            self.with_referee = True

    def _sgf(self, seq, res_tag):
        parts = [SGF_HEADER, f"PB[{self.B.name}]", f"PW[{self.W.name}]", f"RE[{res_tag}]"]
        for col, mv in seq:
            if mv.lower() == "resign":
                continue
            sgf_mv = coord_to_sgf(mv)
            if sgf_mv is not None:
                # Skip passes
                parts.append(f";{col}[{sgf_mv}]")
        parts.append(SGF_FOOTER)
        return "".join(parts)

    def play(self) -> GameResult:
        # fresh engines
        self.B.restart(self.soft_restart)
        self.W.restart(self.soft_restart)
        if self.with_referee:
            self.referee.restart(self.soft_restart)

        history = []  # ("B"|"W", move)
        to_move = "black"
        passes = 0
        moves = 0
        discs = 4
        resigned = False

        while True:
            eng, opp = (self.B, self.W) if to_move == "black" else (self.W, self.B)

            mv = eng.genmove(to_move).lower()
            if mv.startswith("wrong"):
                break
            if mv == "resign":
                resigned = True
                break

            moves += 1
            # print(f"[{to_move} {mv}]")
            history.append(("b" if to_move == "black" else "w", mv))
            # print(history)

            # mirror the move to the opponent so its board state matches
            opp.play_move(to_move, mv)
            if self.with_referee:
                self.referee.play_move(to_move, mv)

            if mv == "pass":
                passes += 1
                if passes == 2:
                    break
            else:
                passes = 0
                discs += 1
                if discs == 64:
                    break

            to_move = "white" if to_move == "black" else "black"

        if resigned:
            result = "W+R" if to_move == "black" else "B+R"
        else:
            result = self.referee.final_score()

        sgf_txt = self._sgf(history, result)
        fn = f"{self.mid}_g{self.no:03d}_{self.B.short()}-B_{self.W.short()}-W.sgf"
        fp = self.out / fn
        with open(fp, "w", encoding="utf-8") as f:
            f.write(sgf_txt)

        self.B.close(self.soft_restart)
        self.W.close(self.soft_restart)
        if self.with_referee:
            self.referee.close(self.soft_restart)

        return GameResult(moves, result, fp)


# ---------------------------------------------------------------------------
# Match driver
# ---------------------------------------------------------------------------

@dataclass
class MatchRunner:
    A: Engine
    B: Engine
    games: int = 100
    first: str = "black"
    alternate: bool = True
    root: Path = Path(results_root)
    soft: bool = False
    max_parallel: int = 1
    games_csv_out: str | Path | None = None
    games_pgn_out: str | Path | None = None
    matches_csv_out: str | Path | None = None

    def __post_init__(self):
        # Make players distinguishable
        if self.A.name == self.B.name:
            self.A.name = self.A.name + "_a"
            self.B.name = self.B.name + "_b"
        assert self.first in ["black", "white"]
        self.parity = 0 if self.first == "black" else 1
        self.mid = uuid4().hex[:8]
        self.thread_local = threading.local()
        self.counter      = itertools.count()
        self.gpu_id       = itertools.cycle(range(NGPUS))
        if self.games_csv_out is None:
            self.games_csv_out = Path(results_root) / "games.csv"
        if self.games_pgn_out is None:
            self.games_pgn_out = Path(results_root) / "games.pgn"
        if self.matches_csv_out is None:
            self.matches_csv_out = Path(results_root) / "matches.csv"

    def copy_engine(self, org_eng: Engine) -> Engine:
        new_eng = org_eng.clone()
        if new_eng.category == "leelaz" and not new_eng.cpu_only:
            gpu = next(self.gpu_id) % NGPUS
            new_eng.set_gpu(gpu)
        return new_eng

    def load_engines(self):
        newA = self.copy_engine(self.A)
        newB = self.copy_engine(self.B)
        newA.start()
        newB.start()

        return (newA, newB)

    def _get_my_engines(self):
        """
        Return the resource permanently assigned to *this* worker thread.
        The first time each worker calls it we hand out the next free
        resource; afterwards the same object is returned instantly.
        """
        if not hasattr(self.thread_local, "res"):          # first call in this thread?
            idx = next(self.counter) % len(self.engines)   # atomic under the GIL
            self.thread_local.res = self.engines[idx]      # remember for ever
        return self.thread_local.res

    def run_game(self, game: int):
        A, B = self._get_my_engines()
        index = game % 2 if self.alternate else 0
        black, white = (A, B) if index == self.parity else (B, A)
        return game, black, white, Game(black, white, game + 1, self.mid, self.tmp, self.soft).play()

    def update_match_results(self, points_AB, points_BA):
        stats = {
            "wins_A_black": points_AB[0],
            "draws_AB":     points_AB[1],
            "wins_B_white": points_AB[2],
            "wins_A_white": points_BA[2],
            "draws_BA":     points_BA[1],
            "wins_B_black": points_BA[0],
            "games_AB":     sum(points_AB),
            "games_BA":     sum(points_BA),
            }
        stats["games"]  = stats["games_AB"] + stats["games_BA"]
        stats["wins_A"] = stats["wins_A_black"] + stats["wins_A_white"]
        stats["draws"]  = stats["draws_AB"]     + stats["draws_BA"]
        stats["wins_B"] = stats["wins_B_black"] + stats["wins_B_white"]
        stats["rate_A_black"] = (stats["wins_A_black"] + stats["draws_AB"]) / stats["games_AB"]
        stats["rate_A_white"] = (stats["wins_A_white"] + stats["draws_BA"]) / stats["games_BA"]
        stats["rate_B_white"] = 1 - stats["rate_A_black"]
        stats["rate_B_black"] = 1 - stats["rate_A_white"]
        self.match_results.update(stats)

    def get_game_stats(self, g, black, white, game_result):
        return {
            "match_id": self.mid,
            "game_no": g + 1,
            "black": black.name,
            "white": white.name,
            "score": game_result.result,
            "move_count": game_result.moves,
            "sgf_file": game_result.sgf.name,
        }

    def run(self):
        self.tmp = self.root / "tmp" / self.mid
        self.tmp.mkdir(parents=True, exist_ok=True)
        game_fields = [
            "match_id", "game_no", "black", "white",
            "score", "move_count", "sgf_file",
        ]
        self.match_results = {
            "match_id": self.mid, "A": self.A.name, "B": self.B.name,
            "first": "B" if self.first == "black" else "W", "alternate": int(self.alternate),
        }
        points_AB = [0, 0, 0]
        points_BA = [0, 0, 0]
        with open(self.games_csv_out, "a", newline="", encoding="utf-8") as fcsv:
            with open(self.games_pgn_out, "a", newline="", encoding="utf-8") as fpgn:
                writer = csv.DictWriter(fcsv, fieldnames=game_fields)
                if os.path.getsize(self.games_csv_out) == 0:
                    writer.writeheader()

                print(f"Starting match {self.mid}")
                self.engines = []
                try:
                    for _ in range(self.max_parallel):
                        self.engines.append(self.load_engines())
                    print("Engines loaded")

                    with ThreadPoolExecutor(max_workers=self.max_parallel) as pool:
                        futures = {pool.submit(self.run_game, i): i for i in range(self.games)}

                        for f in as_completed(futures):
                            g, black, white, game_result = f.result()
                            writer.writerow(self.get_game_stats(g, black, white, game_result))

                            blk, drw, wht = game_result.point
                            if drw:
                                winner = None
                                print(f"Game {g+1} ended with a draw")
                                pgn_score = "1/2-1/2"
                            else:
                                winner = black.name if blk else white.name
                                print(f"Game {g+1} won by {winner} with score {game_result.result}")
                                pgn_score = "0-1" if blk else "1-0"
                            # minimal PGN file that can be parsed by bayeselo according to
                            # https://www.yuzeh.com/etc/2019-04-07-bayeselo-for-games
                            fpgn.write(f'[White "{white}"][Black "{black}"][Result "{pgn_score}"] 1. c4 Nf6')

                            if black.name == self.A.name:
                                points_AB = [x + int(y) for (x,y) in zip(points_AB, game_result.point)]
                            else:
                                points_BA = [x + int(y) for (x,y) in zip(points_BA, game_result.point)]

                finally:
                    for engA, engB in self.engines:
                        engA.close(); engB.close()

        self.update_match_results(points_AB, points_BA)
        with open(self.matches_csv_out, "a", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=list(self.match_results))
            if os.path.getsize(self.matches_csv_out) == 0:
                writer.writeheader()
            writer.writerow(self.match_results)

        # tar.gz of SGFs copied under both categories
        tar_name = f"match_{self.mid}.tar.gz"
        tar_tmp = self.tmp.parent / tar_name
        with tarfile.open(tar_tmp, "w:gz") as tar:
            for f in self.tmp.glob("*.sgf"):
                tar.add(f, arcname=f.name)

        for cat in (self.A.short(), self.B.short()):
            dest = self.root / cat
            dest.mkdir(parents=True, exist_ok=True)
            (dest / tar_name).write_bytes(tar_tmp.read_bytes())

        # clean tmp
        tar_tmp.unlink()
        for f in self.tmp.glob("*"):
            f.unlink()
        self.tmp.rmdir()



# ---------------------------------------------------------------------------
# Demo
# ---------------------------------------------------------------------------


def elys_net(gen: int | None = None):
    if gen is None:
        return network
    else:
        return save_gen_dir_ + f"\\{gen}\\leelaz-model-{gen*4000}.txt.gz"

def adri_net(gen: int = 300):
    return adri_gen_dir_ + f"\\{gen}\\{gen}gen.txt.gz"


if __name__ == "__main__":
    from config import *

    # adri50  = LeelaEngine(adri_net(50))
    kalmia_easy = KalmiaEngine("easy")
    kalmia_normal = KalmiaEngine("normal")
    kalmia_pro = KalmiaEngine("proffesional")
    kalmia_super = KalmiaEngine("superhuman")
    default = LeelaEngine(elys_net())
    default109 = LeelaEngine(elys_net(109))
    adrieng = LeelaEngine(elys_net(), exe=lzadri)
    std_othello = LeelaEngine(elys_net(), exe=lzflip)
    # std_othello109 = LeelaEngine(elys_net(109), exe=lzflip)
    edax_default = EdaxEngine()
    edax_lvl20 = EdaxEngine(20)

    # edax_engine = EdaxEngine()

    MatchRunner(EdaxEngine(10, book=False), LeelaEngine(elys_net(), visits=1000),
                games=20, soft=True).run()
