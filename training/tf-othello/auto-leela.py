import subprocess
import os
import time
import tarfile
from config import *

# imports for multithread execution
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading
import itertools


def wait_for_prompt(process):
    line=process.stdout.read(7)
def skip_credentials(process):
    for x in range(5):
        line=process.stdout.readline()

def sgf_edit(sgf, num_moves):
    # Read the content of the SGF file
    with open(sgf, 'r') as file:
        sgf_content = file.read()

    # Replace 'GM[1]' with 'GM[2]' for Othello
    updated_sgf_content = sgf_content.replace('GM[1]', 'GM[2]')

    # Append a comment with match result and moves number
    comment = f'Match ended with {num_moves} moves.'
    updated_sgf_content += f"\n(;C[{comment}])"  # Append the comment as a new node

    # Write the updated content back to the same file
    with open(sgf, 'w') as file:
        file.write(updated_sgf_content)

def play(process, turn_player):

    process.stdin.write('genmove '+turn_player+'\n')
    process.stdin.flush()
    line=process.stdout.readline()
    while not line.startswith('Leela:'):
        line=process.stdout.readline()
    new_move=''
    for i in range(len(line) - 1, -1, -1):
        if line[i]==' ':
            new_move=line[i+1:len(new_move)-1]
            # print(new_move)
            break
    return new_move

def load_engine(i):
    p = subprocess.Popen(
        [leelaz, '-w', network] + leelaz_args + ['-r', str(resign_pct)]
        + ['--gpu', str(i%2)],
        stdout=subprocess.PIPE,
        stdin=subprocess.PIPE,
        bufsize=1,
        text=True
    )
    skip_credentials(p)
    return p

thread_local = threading.local()
counter      = itertools.count()
engines = [load_engine(i) for i in range(max_parallel)]

def _get_my_engine():
    """
    Return the resource permanently assigned to *this* worker thread.
    The first time each worker calls it we hand out the next free
    resource; afterwards the same object is returned instantly.
    """
    if not hasattr(thread_local, "res"):          # first call in this thread?
        idx = next(counter)                       # atomic under the GIL
        thread_local.res = engines[idx]           # remember for ever
    return thread_local.res


def run_game(i, delay=None):
    if delay:
        time.sleep(delay)

    # resign_percent = 0 if i % 7 == 0 else resign_pct
    p = _get_my_engine()
    p.stdin.write('clear_board\n')
    p.stdin.flush()
    p.stdin.write('clear_cache\n')
    p.stdin.flush()
    turn_player='black'
    winner=None
    pass_counter=0
    num_moves=0
    
    while True:
        new_move=play(p,turn_player)
        #print("Played "+new_move+" for "+turn_player)
        num_moves+=1

        if "resign" in new_move:
            #print("Played "+new_move+" for "+turn_player)
            if turn_player=='black':
                winner='w'
                score='W+R'
            else:
                winner='b'
                score='B+R'
            break

        if "pass" in new_move:
            pass_counter+=1
        else:
            pass_counter=0

        # Match not over, switch players and update move
        #print(f"Pass counter is:{pass_counter}")
        if pass_counter>=2:
            #print("2 passes in a row, game over)__________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________")
            break

        if turn_player=='black':
            turn_player='white'
        else:
            turn_player='black'

    # p.stdin.write('auto\n')
    # p.stdin.flush()
    wait_for_prompt(p)
    p.stdin.write('final_score\n')
    p.stdin.flush()
    final_score= p.stdout.readline().split(":")[-1]
    #print("Final score is "+final_score)
    if(winner==None):
        # print(f"Victory by double pass after {num_moves} moves")
        # print(final_score)
        score = final_score.lstrip("= ").strip()
        if 'B' in final_score:
            winner = 'b'
        elif 'W' in final_score:
            winner = 'w'
        else:
            winner = '0'
    # p.stdin.write('final_score\n')
    # p.stdin.flush()


    # line = ''
    # while True:
    #     line = p.stdout.readline()
    #     if line.startswith('Leela:'):
    #         break

    # # Determine the winner
    # winner = line[16].lower()
    # print("Winner: "+ (winner))
    assert winner == 'w' or winner == 'b' or winner == '0'

    # Prints the sgf file of the match
    command = f'printsgf {i}_al.sgf\n'
    p.stdin.write(command)
    p.stdin.flush()

    # Send the dump_training command
    command = f'dump_training {winner} tmp{i}\n'
    #print(command)
    p.stdin.write(command)
    p.stdin.flush()

    # # Quit Leela Zero
    # p.stdin.write('quit\n')
    # p.stdin.flush()

    time.sleep(0.1)

    # Edits the SGF
    sgf_edit(f"{i}_al.sgf", num_moves)
    return i, score


# Creates the leela_files directory
os.makedirs(leela_files, exist_ok=True)

# Creates the Training and Test directories
os.makedirs(dirname, exist_ok=True)
os.makedirs(test_dir, exist_ok=True)

# Creates the networks directories
os.makedirs(save_gen_dir, exist_ok=True)
os.makedirs(current_network_dir, exist_ok=True)

# Creates the sgf archives directory
os.makedirs(sgf_archive, exist_ok=True)
os.makedirs(al_sgf, exist_ok=True)

# Creates the data archives directory
os.makedirs(archive_path, exist_ok=True)


if not os.path.isfile(network):
    print(f"There is no {network} file!")
    exit(1)

# Collect list of files both in Training and in running directory
dirlist = os.listdir(dirname) + os.listdir(test_dir) + os.listdir(".")

# Select dump_training files of format tmp1234.0.gz, get the number and make a list
# Add 0 to deal with empty directory at the beginning
nums = [0] + [int(x.replace("tmp", "").replace(".0.gz", "")) for x in dirlist if x.endswith(".0.gz")]

max_num = max(nums)
print(f"Current number of games {max_num}.")

# current_gen   = max_num // games_per_generation
prev_networks = [int(x) for x in os.listdir(save_gen_dir) if x.isnumeric()]
current_gen   = 1 + max(prev_networks) if len(prev_networks)>0 else 1
print(f"Current generation {current_gen}")

prev_data_dirs = [x.split('_')[0] for x in os.listdir(archive_path)]
prev_data_gens = [int(x) for x in prev_data_dirs if x.isnumeric()]
if len(prev_data_gens)>0:
    with tarfile.open(os.path.join(archive_path, f'{max(prev_data_gens)}_gen.tar')) as f:
    prev_games = max([int(m.name.lstrip('tmp').split('.')[0]) for m in f.getmembers()])
else:
    prev_games = 0
# missing_games = -max_num % games_per_generation
missing_games = prev_games + games_per_generation - max_num
games_to_play = missing_games or games_per_generation
target_games  = max_num + games_to_play

print(f"Starting {games_to_play} games for generation {current_gen}.")
game_indices  = range(max_num+1, target_games+1)
print(f"Up to {max_parallel} games to be played in parallel.")
delays = len(game_indices) * [None]
delays[0:max_parallel] = [i*0.2 for i in range(max_parallel)]

start = time.perf_counter()

with ThreadPoolExecutor(max_workers=max_parallel) as pool:
    futures = {pool.submit(run_game, i, wait): i for (i, wait) in zip(game_indices, delays)}

    for f in as_completed(futures):
        i, score = f.result()
        print(f"✓ finished game {i}/{target_games} [{current_gen}]  (result: {score})", flush=True)

elapsed = time.perf_counter() - start

print(f"All games finished. Elapsed: {elapsed:g} seconds")
print(f"{elapsed/games_to_play:g} seconds per game")

print("Tidying up files...")

# Zip the sgfs and remove them
full_path = os.path.join(al_sgf, f"{current_gen}_gen")
os.system(f"tar -czf {full_path}.tar.gz *.sgf --remove-files")

# Zip the training data
full_path = os.path.join(archive_path, f"{current_gen}_gen")
os.system(f"tar -cf {full_path}.tar *.0.gz")

# Move the training data in Training and Test
os.system(f"mv tmp* {dirname}")
os.system(f"mv {dirname}/*0.0.gz {test_dir}")

if training_window=='auto':
    training_window = min(10, (current_gen+2) // 6 + 1)
out_of_window = target_games - games_per_generation * training_window
if(out_of_window > 0):
    for directory in [dirname, test_dir]:
        for s in os.listdir(directory):
            if s.endswith(".0.gz"):
                if int(s.replace("tmp", "").replace(".0.gz", "")) <= out_of_window:
                    os.unlink(os.path.join(directory, s))

print("Finished")
