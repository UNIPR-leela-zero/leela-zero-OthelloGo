import os

# Config file to set paths and iteration numbers

#################################
##     SETTINGS TO CHANGE
#################################

blocks               = 20
filters              = 256

# Number of iterations for auto-leela
games_per_generation = 1600      # 2500
max_parallel         = 4
training_window      = "auto"    # 5
learning_rate        = "auto"    # 0.02
visits               = 300       # 150
random_moves         = 25        # 10
resign_pct           = 5
resign_pct_match     = 0
puct                 = 1.5       # 0.5
logpuct              = 0.001     # 0.015
logconst             = 2         # 1.7
# softmax_temp         = 1.0
# fpu_reduction        = 0.25
# ci_alpha             = 1e-5f

leelaz_args = ['-v', str(visits),
               '-m', str(random_moves),
               '--puct', str(puct),
               '--logpuct', str(logpuct),
               '--logconst', str(logconst),
               # '--softmax_temp', str(softmax_temp),
               # '--fpu_reduction', str(fpu_reduction),
               # '--ci_alpha' , str(ci_alpha),
               '--std-othello',
               '--noponder', '-n', '-q']

match_args  = ['-r', str(resign_pct_match),
               '--puct', str(puct),
               '--logpuct', str(logpuct),
               '--logconst', str(logconst),
               # '--softmax_temp', str(softmax_temp),
               # '--fpu_reduction', str(fpu_reduction),
               # '--ci_alpha' , str(ci_alpha),
               '--noponder', '-g', '-q']


# Path to LZO - general directory
LZO = "/mnt/d/lzo"
LZO_ = "D:\\lzo"
lzo_aincrad = "/mnt/d/lzo-aincrad"
lzo_aincrad_ = "D:\\lzo-aincrad"


#################################
#################################

rundir    = os.path.join(LZO, "run")
edax      = os.path.join(LZO, "bin", "edax", "wEdax-x86-64-v3.exe")
kalmia    = "/opt/kalmia/Kalmia"
egaroucid = os.path.join(LZO, "bin", "egaroucid")

results_root = LZO + "/results"    # where results/SGFs are stored


# leela_files path
leela_files = LZO + "/leela_files"
leela_files_ = LZO_ + "\\leela_files"
adri_files_ = lzo_aincrad_ + "\\leela_files"
# training scripts path
tf_othello = "/home/cs-project/repo/leela-zero-othello/training/tf-othello"
# best-network weights path
network = leela_files_ + "\\current_promoted\\best-network.gz"
# general best-network files path
best_network = leela_files + "/current_promoted/best-network.gz"
# leelaz program path
leelaz = LZO + "/bin/leelaz.exe"
lzadri = LZO + "/bin/adri/leelaz.exe"
lzflip = LZO + "/bin/flip/leelaz.exe"
lzdraw = LZO + "/bin/draw/leelaz.exe"
# Training directory path
dirname = leela_files + "/Training"
test_dir = leela_files + "/Test"
# training archives path
archive_path = leela_files + "/archived_training"
# general white_networks directory path
white_networks = leela_files + "/white_networks" 
# white_networks files path
path = white_networks + "/leelaz-model"
# leelalogs path
leela_logs = leela_files + "/leelalogs"
# network archives path
save_gen_dir = leela_files + "/network_generations"
save_gen_dir_ = leela_files_ + "\\network_generations"
adri_gen_dir_ = adri_files_ + "\\network_generations"

# sgf archives path
sgf_archive = leela_files + "/archived_sgf"
sgf_archive_ = leela_files_ + "\archived_sgf"
# auto_leela sgf archive
al_sgf = sgf_archive + "/auto_leela_sgf"
# autoplay sgf archive
ap_sgf = sgf_archive + "/autoplay_sgf"
# matchmaker sgf archive
matchmaker_sgf = sgf_archive + "/matchmaker_sgf"
# compare-checkpoint sgf archive
compare_chkpt_sgf = sgf_archive + "/comp_chkpt_sgf"
# edax sgf archive
edax_sgf = sgf_archive + "/edax_sgf"
# egaroucid sgf archive
egaroucid_sgf = sgf_archive + "/egaroucid_sgf"

# auto-leela path
auto_leela = tf_othello + "/auto-leela.py"
# parse path
parse = tf_othello + "/parse.py"
# autoplay-best path
autoplay_best = tf_othello + "/autoplay-best.py"
tf_events = tf_othello + "/events.py"
