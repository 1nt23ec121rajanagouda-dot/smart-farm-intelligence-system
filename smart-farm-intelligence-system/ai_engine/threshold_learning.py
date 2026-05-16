# Self-Learning Threshold Engine


import pandas as pd
import numpy as np
import json
import os
from datetime import datetime, timedelta
import warnings
warnings.filterwarnings('ignore')

print("=" * 60)
print("  SMART FARM - SELF LEARNING THRESHOLD ENGINE")
print("  NMIT ECE Final Year Project 2025-26")
print("=" * 60)

# ============================================================
# STEP 1 - Load Dataset
# ============================================================
print("\n[1] Loading sensor dataset...")
df = pd.read_csv('ai_engine/data_logs/sensor_data.csv')
df['timestamp'] = pd.to_datetime(df['timestamp'])
df['week'] = df['timestamp'].dt.isocalendar().week
df['month'] = df['timestamp'].dt.month
df['season'] = df['season']
print(f"    Records loaded       : {len(df)}")
print(f"    Date range           : {df['timestamp'].min().date()} to {df['timestamp'].max().date()}")
print(f"    Total weeks          : {df['week'].nunique()}")

# ============================================================
# STEP 2 - Initial Fixed Thresholds (before learning)
# ============================================================
print("\n[2] Initial fixed thresholds (before self-learning):")

initial_thresholds = {
    'soil_moisture_min'    : 40.0,   # water if below this
    'temperature_max'      : 38.0,   # heat alert if above this
    'humidity_max'         : 85.0,   # disease risk if above this
    'plant_resistance_max' : 450.0,  # stress if above this
    'water_level_min'      : 20.0,   # refill alert if below this
}

for k, v in initial_thresholds.items():
    print(f"    {k:<28}: {v}")

# ============================================================
# STEP 3 - Self Learning Function
# ============================================================
def learn_thresholds(data, current_thresholds):
    """
    Learn better thresholds from stress events.
    Logic:
    - Find all stress events
    - Look at sensor values JUST BEFORE stress happened
    - Update threshold to catch stress earlier next time
    """
    new_thresholds = current_thresholds.copy()

    # --- Learn soil moisture threshold ---
    water_stress = data[data['stress_type'] == 'water_stress']
    if len(water_stress) > 5:
        # Find average soil moisture at start of water stress
        avg_soil_at_stress = water_stress['soil_moisture'].mean()
        # Set new threshold slightly above stress point
        new_soil_thr = round(avg_soil_at_stress + 5.0, 1)
        # Only update if meaningfully different
        if abs(new_soil_thr - current_thresholds['soil_moisture_min']) > 1.0:
            new_thresholds['soil_moisture_min'] = min(55.0, new_soil_thr)

    # --- Learn temperature threshold ---
    heat_stress = data[data['stress_type'] == 'heat_stress']
    if len(heat_stress) > 3:
        avg_temp_at_stress = heat_stress['temperature'].mean()
        new_temp_thr = round(avg_temp_at_stress - 2.0, 1)
        if abs(new_temp_thr - current_thresholds['temperature_max']) > 0.5:
            new_thresholds['temperature_max'] = max(32.0, new_temp_thr)

    # --- Learn humidity threshold for disease ---
    disease_risk = data[data['stress_type'] == 'disease_risk']
    if len(disease_risk) > 3:
        avg_hum_at_disease = disease_risk['humidity'].mean()
        new_hum_thr = round(avg_hum_at_disease - 3.0, 1)
        if abs(new_hum_thr - current_thresholds['humidity_max']) > 1.0:
            new_thresholds['humidity_max'] = max(75.0, new_hum_thr)

    # --- Learn plant resistance threshold ---
    all_stress = data[data['stress_type'].isin(['water_stress','heat_stress','mild_stress'])]
    if len(all_stress) > 5:
        avg_res_at_stress = all_stress['plant_resistance'].mean()
        new_res_thr = round(avg_res_at_stress - 10.0, 1)
        if abs(new_res_thr - current_thresholds['plant_resistance_max']) > 5.0:
            new_thresholds['plant_resistance_max'] = max(350.0, new_res_thr)

    return new_thresholds

# ============================================================
# STEP 4 - Week by Week Learning Simulation
# ============================================================
print("\n[3] Week by week self-learning simulation:")
print("    " + "-" * 55)
print(f"    {'Week':<6} {'Soil Thr':<12} {'Temp Thr':<12} {'Humid Thr':<12} {'Stress Events'}")
print("    " + "-" * 55)

current_thresholds = initial_thresholds.copy()
weekly_history = []

weeks = sorted(df['week'].unique())

for week in weeks:
    week_data = df[df['week'] == week]
    stress_events = len(week_data[week_data['stress_type'] != 'none'])

    # Learn from this week's data
    current_thresholds = learn_thresholds(week_data, current_thresholds)

    weekly_history.append({
        'week'                 : int(week),
        'soil_threshold'       : current_thresholds['soil_moisture_min'],
        'temp_threshold'       : current_thresholds['temperature_max'],
        'humidity_threshold'   : current_thresholds['humidity_max'],
        'resistance_threshold' : current_thresholds['plant_resistance_max'],
        'stress_events'        : stress_events
    })

    if week % 4 == 0 or week == weeks[0] or week == weeks[-1]:
        print(f"    W{str(week):<5} "
              f"{current_thresholds['soil_moisture_min']:<12} "
              f"{current_thresholds['temperature_max']:<12} "
              f"{current_thresholds['humidity_max']:<12} "
              f"{stress_events}")

# ============================================================
# STEP 5 - Show How Thresholds Improved
# ============================================================
print("\n[4] How thresholds changed through self-learning:")
print("    " + "-" * 55)

first = weekly_history[0]
last  = weekly_history[-1]

metrics = [
    ('Soil moisture min',  'soil_threshold',       '%'),
    ('Temperature max',    'temp_threshold',        '°C'),
    ('Humidity max',       'humidity_threshold',    '%'),
    ('Plant resistance',   'resistance_threshold',  'Ω'),
]

for name, key, unit in metrics:
    start_val = initial_thresholds.get(
        [k for k in initial_thresholds if name.lower().split()[0] in k.lower()][0],
        first[key]
    )
    end_val   = last[key]
    change    = round(end_val - start_val, 1)
    direction = '↑ increased' if change > 0 else ('↓ decreased' if change < 0 else '→ unchanged')
    print(f"    {name:<22}: {start_val}{unit} → {end_val}{unit}  ({direction} by {abs(change)}{unit})")

# ============================================================
# STEP 6 - Season Based Learning
# ============================================================
print("\n[5] Season-based threshold analysis:")
print("    " + "-" * 55)

seasons = df['season'].unique()
for season in ['winter', 'spring', 'summer', 'autumn']:
    season_data = df[df['season'] == season]
    if len(season_data) == 0:
        continue
    stress_data  = season_data[season_data['stress_type'] != 'none']
    stress_rate  = round(len(stress_data) / len(season_data) * 100, 1)
    avg_soil     = round(season_data['soil_moisture'].mean(), 1)
    avg_temp     = round(season_data['temperature'].mean(), 1)
    avg_res      = round(season_data['plant_resistance'].mean(), 1)
    print(f"\n    {season.upper()}")
    print(f"      Stress rate        : {stress_rate}%")
    print(f"      Avg soil moisture  : {avg_soil}%")
    print(f"      Avg temperature    : {avg_temp}°C")
    print(f"      Avg plant resistance: {avg_res}Ω")

# ============================================================
# STEP 7 - Save Final Learned Thresholds
# ============================================================
print("\n[6] Saving learned thresholds to file...")

final_thresholds = {
    'last_updated'          : datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
    'learning_weeks'        : len(weeks),
    'thresholds'            : current_thresholds,
    'initial_thresholds'    : initial_thresholds,
    'improvement_summary'   : {
        m[0]: {
            'initial' : initial_thresholds.get(
                [k for k in initial_thresholds if m[0].lower().split()[0] in k.lower()][0],
                weekly_history[0][m[1]]
            ),
            'learned' : last[m[1]]
        }
        for m in metrics
    }
}

os.makedirs('ai_engine', exist_ok=True)
with open('ai_engine/learned_thresholds.json', 'w') as f:
    json.dump(final_thresholds, f, indent=4)

print("    Saved to: ai_engine/learned_thresholds.json")

# ============================================================
# STEP 8 - What ESP32 Uses
# ============================================================
print("\n[7] Final learned thresholds for ESP32:")
print("    " + "-" * 55)
print("    Copy these values into your ESP32 firmware:\n")
print(f"    int SOIL_THRESHOLD      = {int(current_thresholds['soil_moisture_min'])};")
print(f"    int TEMP_THRESHOLD      = {int(current_thresholds['temperature_max'])};")
print(f"    int HUMIDITY_THRESHOLD  = {int(current_thresholds['humidity_max'])};")
print(f"    int RESIST_THRESHOLD    = {int(current_thresholds['plant_resistance_max'])};")
print(f"    int WATER_MIN           = {int(initial_thresholds['water_level_min'])};")

print("\n" + "=" * 60)
print("  Self-learning engine complete.")
print("  System has learned optimal thresholds from 1 year of data.")
print("  Thresholds saved and ready for ESP32 integration.")
print("=" * 60)