# ============================================================
# SMART FARM INTELLIGENCE SYSTEM
# Stress Prediction Engine
# NMIT ECE Final Year Project 2025-26
# File: ai_engine/stress_prediction.py
# ============================================================
# This script:
# 1. Loads the sensor dataset
# 2. Trains a Random Forest model
# 3. Predicts crop stress type
# 4. Shows accuracy and results
# ============================================================

import pandas as pd
import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix, accuracy_score
from sklearn.preprocessing import LabelEncoder
import warnings
warnings.filterwarnings('ignore')

print("=" * 60)
print("  SMART FARM - STRESS PREDICTION ENGINE")
print("  NMIT ECE Final Year Project 2025-26")
print("=" * 60)

# ============================================================
# STEP 1 - Load Dataset
# ============================================================
print("\n[1] Loading dataset...")
df = pd.read_csv('ai_engine/data_logs/sensor_data.csv')
print(f"    Total records loaded : {len(df)}")
print(f"    Date range           : {df['timestamp'].min()} to {df['timestamp'].max()}")
print(f"    Columns              : {list(df.columns)}")

# ============================================================
# STEP 2 - Explore Data
# ============================================================
print("\n[2] Stress type distribution in dataset:")
stress_counts = df['stress_type'].value_counts()
for stress, count in stress_counts.items():
    percent = round(count / len(df) * 100, 1)
    bar = '█' * int(percent)
    print(f"    {stress:<20}: {count:>5} records  {percent}%  {bar}")

# ============================================================
# STEP 3 - Feature Engineering
# ============================================================
print("\n[3] Preparing features...")

# Extract time features from timestamp
df['timestamp'] = pd.to_datetime(df['timestamp'])
df['hour']      = df['timestamp'].dt.hour
df['month']     = df['timestamp'].dt.month
df['day']       = df['timestamp'].dt.day

# Encode categorical columns
le_season = LabelEncoder()
le_rainy  = LabelEncoder()
df['season_encoded'] = le_season.fit_transform(df['season'])
df['rainy_encoded']  = le_rainy.fit_transform(df['rainy_day'])

# Select features for prediction
features = [
    'soil_moisture',
    'temperature',
    'humidity',
    'light_intensity',
    'plant_resistance',
    'water_level',
    'season_encoded',
    'rainy_encoded',
    'hour',
    'month',
    'learned_soil_threshold',
    'learned_temp_threshold'
]

X = df[features]
y = df['stress_type']

print(f"    Features used        : {len(features)}")
print(f"    Feature list         : {features}")

# ============================================================
# STEP 4 - Split Data
# ============================================================
print("\n[4] Splitting data into train and test sets...")
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42, stratify=y
)
print(f"    Training samples     : {len(X_train)}")
print(f"    Testing samples      : {len(X_test)}")

# ============================================================
# STEP 5 - Train Model
# ============================================================
print("\n[5] Training Random Forest model...")
model = RandomForestClassifier(
    n_estimators=100,
    max_depth=10,
    random_state=42,
    class_weight='balanced'
)
model.fit(X_train, y_train)
print("    Model training complete!")

# ============================================================
# STEP 6 - Evaluate Model
# ============================================================
print("\n[6] Evaluating model performance...")
y_pred    = model.predict(X_test)
accuracy  = accuracy_score(y_test, y_pred)

print(f"\n    Overall Accuracy     : {round(accuracy * 100, 2)}%")
print("\n    Detailed Classification Report:")
print("    " + "-" * 55)
report = classification_report(y_test, y_pred, output_dict=True)
for label, metrics in report.items():
    if isinstance(metrics, dict):
        print(f"    {label:<20}: precision={round(metrics['precision'],2)}  recall={round(metrics['recall'],2)}  f1={round(metrics['f1-score'],2)}")

# ============================================================
# STEP 7 - Feature Importance
# ============================================================
print("\n[7] Feature importance (which sensor matters most):")
importances = model.feature_importances_
sorted_idx  = np.argsort(importances)[::-1]
for i in sorted_idx:
    bar = '█' * int(importances[i] * 100)
    print(f"    {features[i]:<28}: {round(importances[i]*100,1)}%  {bar}")

# ============================================================
# STEP 8 - Live Prediction Demo
# ============================================================
print("\n[8] Live prediction demo with sample readings:")
print("    " + "-" * 55)

sample_readings = [
    {
        'name'              : 'Healthy plant — normal morning',
        'soil_moisture'     : 65,
        'temperature'       : 26,
        'humidity'          : 65,
        'light_intensity'   : 800,
        'plant_resistance'  : 320,
        'water_level'       : 80,
        'season_encoded'    : 1,
        'rainy_encoded'     : 0,
        'hour'              : 9,
        'month'             : 3,
        'learned_soil_threshold': 42,
        'learned_temp_threshold': 36
    },
    {
        'name'              : 'Water stressed plant — dry soil',
        'soil_moisture'     : 28,
        'temperature'       : 30,
        'humidity'          : 55,
        'light_intensity'   : 950,
        'plant_resistance'  : 580,
        'water_level'       : 60,
        'season_encoded'    : 2,
        'rainy_encoded'     : 0,
        'hour'              : 11,
        'month'             : 5,
        'learned_soil_threshold': 42,
        'learned_temp_threshold': 36
    },
    {
        'name'              : 'Heat stressed plant — peak afternoon',
        'soil_moisture'     : 50,
        'temperature'       : 42,
        'humidity'          : 35,
        'light_intensity'   : 990,
        'plant_resistance'  : 480,
        'water_level'       : 70,
        'season_encoded'    : 2,
        'rainy_encoded'     : 0,
        'hour'              : 14,
        'month'             : 7,
        'learned_soil_threshold': 42,
        'learned_temp_threshold': 36
    },
    {
        'name'              : 'Disease risk — rainy night high humidity',
        'soil_moisture'     : 78,
        'temperature'       : 22,
        'humidity'          : 92,
        'light_intensity'   : 0,
        'plant_resistance'  : 260,
        'water_level'       : 95,
        'season_encoded'    : 0,
        'rainy_encoded'     : 1,
        'hour'              : 21,
        'month'             : 1,
        'learned_soil_threshold': 42,
        'learned_temp_threshold': 36
    }
]

actions = {
    'none'         : 'No action needed. Continue monitoring.',
    'water_stress' : 'ACTION: Turn ON water pump immediately.',
    'heat_stress'  : 'ACTION: Send heat stress alert to farmer.',
    'disease_risk' : 'ACTION: Send disease risk alert. Check for fungal infection.',
    'mild_stress'  : 'ACTION: Monitor closely. Consider irrigation soon.'
}

for sample in sample_readings:
    name   = sample.pop('name')
    values = pd.DataFrame([sample])
    pred   = model.predict(values)[0]
    proba  = model.predict_proba(values)[0]
    conf   = round(max(proba) * 100, 1)
    action = actions.get(pred, 'Monitor plant.')
    print(f"\n    Scenario : {name}")
    print(f"    Predicted: {pred.upper()}  (confidence: {conf}%)")
    print(f"    Response : {action}")

print("\n" + "=" * 60)
print("  Stress prediction engine ready.")
print("  Model can be integrated with ESP32 via serial or MQTT.")
print("=" * 60)
