#!/usr/bin/env python3
"""
Merge TF-1.x scalar summaries (Policy Loss, MSE Loss) from many *.tower event files
into a single CSV.
"""

import csv
import glob
import os
import tensorflow as tf          # TF-1.x  (pip install tensorflow==1.15.*)
import numpy as np
import matplotlib

matplotlib.use("Agg")


import matplotlib.pyplot as plt

PATTERN      = "*gen/*.tower"
OUT_CSV      = "merged_scalars.csv"
OUT_PDF      = "plots.pdf"
KEEP_TAGS    = {"Policy Loss", "MSE Loss", "Accuracy"}

try:
    with open(OUT_CSV, newline='') as f:
        reader = csv.reader(f)
        next(reader, None)          # skip header if it’s there
        paths_in_csv = [row[-1] for row in reader]
except FileNotFoundError:
    paths_in_csv = []

paths_in_logsdir = glob.glob(PATTERN, recursive=True)

paths_to_read = [x for x in paths_in_logsdir if x not in paths_in_csv]
print(f"Found {len(paths_in_logsdir)} records, {len(paths_to_read)} to read.")

paths_sorted = sorted(paths_to_read, key=lambda s: int(s.split('gen', 1)[0]))

# --------------------------------------------------------------------------
rows = []                         # we’ll fill this with dicts for the CSV
for ev_file in paths_sorted:
    print(f"Reading {ev_file}...")
    step_cache = {}               # step → partial dict until we have both scalars
    s = None
    rec = None

    for ev in tf.train.summary_iterator(ev_file):
        if ev.step == 0:
            continue
        # when the step changes we can emit the previous row
        if s is not None and ev.step != s:
            assert(rec is not None)
            rows.append(rec)
            del step_cache[s]      # free memory
        s = ev.step
        wt = ev.wall_time
        rec = step_cache.setdefault(s, {"step": s,
                                        "wall_time": wt,
                                        "Policy Loss": None,
                                        "MSE Loss": None,
                                        "Accuracy": None,
                                        "source": ev_file})
        for val in ev.summary.value:
            if val.tag in KEEP_TAGS:
                rec[val.tag] = val.simple_value

    # emit the last row
    rows.append(rec)

# --------------------------------------------------------------------------

# Write the CSV
with open(OUT_CSV, "a", newline="") as f:
    fieldnames = ["step", "Policy Loss", "MSE Loss", "Accuracy", "wall_time", "source"]
    writer = csv.DictWriter(f, fieldnames=fieldnames)
    if len(paths_in_csv)==0:
        writer.writeheader()
    writer.writerows(rows)

print(f"Wrote {len(rows):,} rows → {OUT_CSV}")

def to_float(field):
    """bytes ▸ float, treating empty cells as 0.0"""
    s = field.decode()            # `loadtxt` passes raw bytes
    return float(s) if s else 0.0



data     = np.loadtxt(OUT_CSV, delimiter=',', skiprows=1, usecols=(0, 1, 2, 3),
                      converters={3: to_float})
step     = data[:,0]
pol_loss = data[:,1]
val_loss = data[:,2]
accuracy = data[:,3]

def conv(x, y, window=20):
    n = len(x)
    y_conv = np.convolve(y, np.array(np.ones(window)/window), mode='valid')
    x_range = np.linspace(x[0], x[-1], len(y_conv))
    return x_range, y_conv

m = 3 if accuracy.sum() > 0 else 2
window = 5 if m == 3 else 20

fig, axs = plt.subplots(m, 1, figsize=(7, 5*m), sharex=True)

axs[0].plot(step, pol_loss,        c='tab:blue', lw=0.5, alpha=0.7)
axs[0].plot(*conv(step, pol_loss, window=window), c='blue',     lw=0.5)
axs[0].set_ylabel('Policy Loss')
#axs[0].set_title('Policy Loss vs. Step')
axs[0].set_ylim(1.3, 1.8)
axs[0].grid(True)

axs[1].plot(step, val_loss,        c='tab:orange', lw=0.5, alpha=0.7)
axs[1].plot(*conv(step, val_loss, window=window), c='red',        lw=0.5)
axs[1].set_xlabel('step')
axs[1].set_ylabel('Value Loss')
#axs[1].set_title('Value Loss vs. Step')
axs[1].set_ylim(0.13, 0.21)
axs[1].grid(True)

if m == 3:
    axs[2].plot(step, accuracy,     c='tab:green', lw=0.5, alpha=0.7)
    axs[2].plot(*conv(step, accuracy, window=window), c='green',     lw=0.5)
    axs[2].set_xlabel('step')
    axs[2].set_ylabel('Accuracy')
    axs[2].set_ylim(0.66, 0.76)
    axs[2].grid(True)

fig.tight_layout()
plt.savefig(OUT_PDF)

print(f"Wrote plots → {OUT_PDF}")
