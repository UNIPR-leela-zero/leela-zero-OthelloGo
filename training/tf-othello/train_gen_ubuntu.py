import os
import subprocess
import time
import hashlib
from pathlib import Path

from config import *

def sha256sum(path: str) -> str:
    data = Path(path).read_bytes()
    return hashlib.sha256(data).hexdigest()

while True:
	# Run self-plays (auto-leela.py)
        subprocess.run(['python', auto_leela])

        curr_gen = int(max(os.listdir(save_gen_dir), key=int))
        curr_gen_dir = os.path.join(save_gen_dir, str(curr_gen))
        last_model = [s for s in os.listdir(curr_gen_dir)
                      if s.endswith("meta")][0].replace(".meta", "")
        # Run training (parse.py)
        if learning_rate=="auto":
            rate = (0.02 if curr_gen<200 else
                    0.002 if curr_gen<350 else
                    0.0002)
        else:
            rate = learning_rate
        
        parse_args = ["--blocks", str(blocks),
                      "--filters", str(filters),
                      "--train", os.path.join(dirname, ""),
                      "--test", os.path.join(test_dir, ""),
                      "--restore", os.path.join(curr_gen_dir, last_model),
                      "--rate", str(rate),
                      "--batchsize", str(batch_size)]
        subprocess.run(['python', parse] + parse_args)

        os.system(f"for f in {white_networks}/leelaz-model-*.txt ; do sha256sum $f > $f.sha256 ; done")
        # -q flag to suppress error message of gzip not being able to write the Unix metadata
        os.system(f"gzip -q {white_networks}/leelaz-model-*.txt")
        os.system(f"cp {white_networks}/leelaz-model-*.txt.gz {best_network}")
        new_gen_dir = os.path.join(save_gen_dir, str(curr_gen+1))
        os.makedirs(new_gen_dir)
        os.system(f"mv {white_networks}/* {new_gen_dir}")

        print(f"Network for generation {curr_gen+1} is ready. Starting self-plays in 10 seconds.")
        subprocess.run(['python', tf_events], cwd=leela_logs+"/train")
        subprocess.run(['python', tf_events], cwd=leela_logs+"/test")
        
        time.sleep(10)
