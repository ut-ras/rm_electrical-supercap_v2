import csv
import math
import shutil
import subprocess
import sys
from pathlib import Path

import matplotlib.pyplot as plt

# ---------------- Build and run the C simulation ----------------
HERE = Path(__file__).resolve().parent
C_SOURCE = HERE / "plecs_sim.c"
EXECUTABLE = HERE / ("plecs_sim.exe" if sys.platform == "win32" else "plecs_sim")
CSV_PATH = HERE / "plecs_data.csv"  # Must match OUTPUT_NAME in the C file
PLOT_PATH = HERE / "plecs_plots.png"

STATS_START = 80.0  # Tracking statistics are taken from this time onward (s)


def find_compiler():
    for name in ("gcc", "clang", "cc"):
        path = shutil.which(name)
        if path:
            return path
    raise RuntimeError("No C compiler found (looked for gcc, clang, cc). "
                       "Install one or add it to your PATH.")


def build_and_run():
    compiler = find_compiler()
    subprocess.run(
        [compiler, "-O2", str(C_SOURCE), "-o", str(EXECUTABLE), "-lm"],
        check=True,
    )
    # Run in HERE so the CSV is written next to this script.
    subprocess.run([str(EXECUTABLE)], cwd=HERE, check=True)


build_and_run()

# ---------------- Load the CSV ----------------
with CSV_PATH.open(newline="") as file:
    reader = csv.DictReader(file)
    data = {name: [] for name in reader.fieldnames}

    for row in reader:
        for name in data:
            data[name].append(float(row[name]))

time = data["Time"]

# ---------------- Statistics ----------------
stat_indices = [i for i, t in enumerate(time) if t >= STATS_START]
errors = [data["Iref"][i] - data["Ibat"][i] for i in stat_indices]
rmse = math.sqrt(sum(e * e for e in errors) / len(errors))

print(f"Tracking RMSE after {STATS_START:.0f} s: {rmse:.6f} A")
print(f"Peak absolute tracking error: {max(abs(e) for e in errors):.6f} A")
print(f"Peak inductor current: {max(data['IL']):.2f} A, "
      f"minimum: {min(data['IL']):.2f} A")
print(f"Capacitor voltage range: "
      f"{min(data['Vc']):.4f}-{max(data['Vc']):.4f} V")

# ---------------- Plot (same three panels as the PLECS scope) ----------------
plt.rcParams.update({
    "font.size": 10,
    "axes.grid": True,
    "grid.alpha": 0.25,
})

fig, axes = plt.subplots(
    2, 2,
    figsize=(16, 9),
    sharex=True,
    constrained_layout=True,
)

ax = axes[0,0]
ax.plot(time, data["Iref"], color="deepskyblue", linewidth=2.5,
        label="Min/Max (reference, limited by I_min)")
ax.plot(time, data["Imin"], color="red", linewidth=1.2, label="I_min")
ax.plot(time, data["Ibat"], color="green", linewidth=0.8,
        label="Am1: measured battery current")
ax.set_title("Battery Current")
ax.set_ylabel("Current (A)")
ax.set_ylim(top=1.3 * max(data["Iref"]))  # Headroom so the legend clears the peaks
ax.legend(loc="upper right", ncol=3)

ax = axes[0,1]
ax.plot(time, data["IL"], color="green", label="L1: inductor current")
ax.axhline(0, color="black", linewidth=0.8)
ax.set_title("Inductor Current")
ax.set_ylabel("Current (A)")
ax.legend(loc="upper right")

ax = axes[1,0]
ax.plot(time, data["Vc"], color="magenta", label="C1: capacitor voltage")
ax.axhline(3.0, color="pink", linestyle="--", label="3 V rating")
ax.set_title("Capacitor Voltage")
ax.set_xlabel("Time (s)")
ax.set_ylabel("Voltage (V)")
ax.legend(loc="center right")

ax = axes[1,1]
ax.plot(time, [data["Iref"][i] - data["Ibat"][i] for i in range(len(time))], color="purple", label="Tracking error")
ax.set_title("Tracking Error")
ax.set_xlabel("Time (s)")
ax.set_ylabel("Current (A)")
ax.legend(loc="center right")

fig.suptitle("Power Stage Simulation - Toy Model (C translation of the PLECS model)",
             fontsize=14)

fig.savefig(PLOT_PATH, dpi=200)
plt.show()