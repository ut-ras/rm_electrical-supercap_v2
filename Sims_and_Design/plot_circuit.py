import csv
import math
from pathlib import Path

import matplotlib.pyplot as plt

with Path("circuit_data.csv").open(newline="") as file:
    reader = csv.DictReader(file)
    data = {name: [] for name in reader.fieldnames}

    for row in reader:
        for name in data:
            data[name].append(float(row[name]))

start_time = 1.0
end_time = 1.03

indices = [
    index
    for index, time in enumerate(data["Time"])
    if start_time <= time <= end_time
]

if not indices:
    raise ValueError("No data exists in the requested plotting interval.")

time_ms = [
    (data["Time"][index] - start_time) * 1000
    for index in indices
]

def selected(name):
    return [data[name][index] for index in indices]

steady_indices = [
    index
    for index, time in enumerate(data["Time"])
    if time >= start_time
]

steady_errors = [data["Error"][index] for index in steady_indices]
rmse = math.sqrt(
    sum(error * error for error in steady_errors) / len(steady_errors)
)
peak_error = max(abs(error) for error in steady_errors)

print(f"Tracking RMSE after {start_time:.1f} s: {rmse:.6f} A")
print(f"Peak absolute tracking error: {peak_error:.6f} A")
print(f"Capacitor voltage range: "
      f"{min(data['Vc']):.6f}–{max(data['Vc']):.6f} V")

plt.rcParams.update({
    "font.size": 10,
    "axes.grid": True,
    "grid.alpha": 0.25,
})

fig, axes = plt.subplots(
    2, 2,
    figsize=(13, 8),
    constrained_layout=True,
)

ax = axes[0, 0]
ax.plot(
    time_ms, selected("Iref"),
    "--", color="black", label="Reference current"
)
ax.plot(time_ms, selected("Ibat"), label="Actual converter bus current")
ax.set_title("100 Hz current tracking")
ax.set_xlabel("Time after 1.00 s (ms)")
ax.set_ylabel("Bus current (A)")
ax.legend()

ax = axes[0, 1]
ax.plot(time_ms, selected("Error"))
ax.axhline(0, color="black", linestyle="--")
ax.set_title("Tracking error: reference − actual")
ax.set_xlabel("Time after 1.00 s (ms)")
ax.set_ylabel("Error (A)")

ax = axes[1, 0]
ax.plot(time_ms, selected("I"))
ax.axhline(0, color="black", linestyle="--")
ax.set_title("Inductor / capacitor-side current")
ax.set_xlabel("Time after 1.00 s (ms)")
ax.set_ylabel("Current (A)")

ax = axes[1, 1]
ax.plot(data["Time"], data["Vc"])
ax.axhline(
    3.0, color="red", linestyle="--", label="3 V rating"
)
ax.set_title("Capacitor voltage over the full simulation")
ax.set_xlabel("Time (s)")
ax.set_ylabel("Voltage (V)")
ax.legend()

fig.suptitle(
    "Converter current-tracking test — averaged simulation\n"
    "100 Hz reference | 12 kHz controller updates | 1.2 kHz tuning",
    fontsize=14,
)

fig.savefig("circuit_plots.png", dpi=200)
plt.show()