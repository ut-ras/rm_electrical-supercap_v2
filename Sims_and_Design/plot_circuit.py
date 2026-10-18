import csv
from pathlib import Path

import matplotlib.pyplot as plt

csv_path = Path("circuit_data.csv")

with csv_path.open(newline="") as file:
    reader = csv.DictReader(file)
    data = {name: [] for name in reader.fieldnames}

    for row in reader:
        for name in data:
            data[name].append(float(row[name]))

start_time = 1.00
end_time = 1.03

indices = [
    index
    for index, time in enumerate(data["Time"])
    if start_time <= time <= end_time
]

if not indices:
    raise ValueError("The CSV does not contain the requested plotting interval.")

time_ms = [
    (data["Time"][index] - start_time) * 1000
    for index in indices
]

def selected(name):
    return [data[name][index] for index in indices]

load_current = selected("Iref")
battery_current = selected("Ibat")
battery_target = selected("IbatRef")
converter_current = selected("Iconverter")
battery_power = selected("Power")
load_power = selected("LoadPower")
power_target = selected("PowerRef")

ideal_converter = [
    target - load
    for target, load in zip(battery_target, load_current)
]

balance_error = [
    battery - load - converter
    for battery, load, converter in zip(
        battery_current, load_current, converter_current
    )
]

steady_indices = [
    index
    for index, time in enumerate(data["Time"])
    if time >= start_time
]

steady_power = [data["Power"][index] for index in steady_indices]
steady_load = [data["LoadPower"][index] for index in steady_indices]

battery_ripple = max(steady_power) - min(steady_power)
load_ripple = max(steady_load) - min(steady_load)
reduction = 100 * (1 - battery_ripple / load_ripple)

print(f"Battery power range: {min(steady_power):.3f}–"
      f"{max(steady_power):.3f} W")
print(f"Battery power peak-to-peak ripple: {battery_ripple:.3f} W")
print(f"Ripple reduction relative to load power: {reduction:.2f}%")
print(f"Maximum current-balance residual in plotted interval: "
      f"{max(abs(value) for value in balance_error):.3e} A")

plt.rcParams.update({
    "font.size": 10,
    "axes.grid": True,
    "grid.alpha": 0.25,
})

fig, axes = plt.subplots(
    2, 2,
    figsize=(13, 8),
    sharex=True,
    constrained_layout=True,
)

ax = axes[0, 0]
ax.plot(time_ms, load_current, label="Load demand: i_ref")
ax.plot(time_ms, battery_current, label="Actual battery: ibat")
ax.plot(
    time_ms, battery_target,
    "--", color="black", label="Battery target: ibat_ref"
)
ax.set_title("Battery current versus changing load")
ax.set_ylabel("Current (A)")
ax.legend()

ax = axes[0, 1]
ax.plot(time_ms, load_power, label="Load / battery alone")
ax.plot(time_ms, battery_power, label="Battery with supercap")
ax.plot(
    time_ms, power_target,
    "--", color="black", label="90 W target"
)
ax.set_title("Power buffering")
ax.set_ylabel("Power (W)")
ax.legend()

ax = axes[1, 0]
ax.plot(
    time_ms, ideal_converter,
    "--", color="black", label="Ideal converter current"
)
ax.plot(time_ms, converter_current, label="Actual converter current")
ax.axhline(0, color="gray", linewidth=0.8)
ax.set_title("Converter bus current: + charging / − discharging")
ax.set_xlabel(f"Time after {start_time:.2f} s (ms)")
ax.set_ylabel("Current (A)")
ax.legend()

ax = axes[1, 1]
ax.plot(time_ms, battery_power, label="Actual battery power")
ax.plot(
    time_ms, power_target,
    "--", color="black", label="90 W target"
)
ax.set_title("Battery-power ripple — expanded scale")
ax.set_xlabel(f"Time after {start_time:.2f} s (ms)")
ax.set_ylabel("Power (W)")
ax.legend()

fig.suptitle(
    "Supercap load buffering — averaged simulation\n"
    f"100 Hz load | 1200 Hz controller tuning | "
    f"{reduction:.1f}% power-ripple reduction",
    fontsize=14,
)

fig.savefig("circuit_plots.png", dpi=200)
print("Plot saved to circuit_plots.png")

plt.show()