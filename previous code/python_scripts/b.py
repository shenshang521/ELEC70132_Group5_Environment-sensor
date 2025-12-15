import requests
import json
import sqlite3
import os
import time
from datetime import datetime

# ================= Configuration Area =================
# Get the current script path to ensure the database is generated nearby
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.path.join(BASE_DIR, "esp32_sse_data.sqlite3")
TABLE_NAME = "sensor_data"


# change this to the IP address displayed on the ESP32 screen
ESP_IP = "http://172.20.10.6"  
URL = f"{ESP_IP}/events"
# =========================================

def init_database():
    try:
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()
        
    
        # Create a table containing all sensor data
        create_table_sql = f"""
        CREATE TABLE IF NOT EXISTS {TABLE_NAME} (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            receive_time TEXT NOT NULL,
            temp REAL,
            hum REAL,
            press REAL,
            gas REAL,
            dist INTEGER,
            lat REAL,
            lon REAL,
            alt REAL
        );
        """
        cursor.execute(create_table_sql)
        conn.commit()
        conn.close()
        print(f" Database ready: {DB_PATH}")
    except Exception as e:
        print(f" Database initialization failed: {e}")

def insert_data(data):
    try:
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()
        current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        insert_sql = f"""
        INSERT INTO {TABLE_NAME} 
        (receive_time, temp, hum, press, gas, dist, lat, lon, alt) 
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
        """
        
        # Use the .get() method to prevent errors caused by missing data
        cursor.execute(insert_sql, (
            current_time, 
            data.get('temp', 0),
            data.get('hum', 0),
            data.get('press', 0),
            data.get('gas', 0),
            data.get('dist', -1),
            data.get('lat', 0.0),
            data.get('lon', 0.0),
            data.get('alt', 0.0)
        ))
        conn.commit()
        conn.close()
    except Exception as e:
        print(f" Failed to write to database: {e}")

if __name__ == "__main__":
    init_database()
    
    headers = {
        "Accept": "text/event-stream",
        "Cache-Control": "no-cache",
        "Connection": "keep-alive",
    }
    
    print(f"Connecting to {URL} ...")

    while True:
        try:
            print("Initiating connection request...")
            with requests.get(URL, stream=True, headers=headers, timeout=None) as response:
                response.raise_for_status()
                print(" Connection successful! Waiting for sensor data...")
                
                for line in response.iter_lines():
                    if line:
                        decoded_line = line.decode("utf-8")
                        if decoded_line.startswith("data: "):
                            try:
                                json_str = decoded_line.split("data: ", 1)[1].strip()
                                data = json.loads(json_str)
                                
                        
                                # Print complete data
                                print(f" T:{data.get('temp')}C | H:{data.get('hum')}% | P:{data.get('press')} | D:{data.get('dist')}mm | GPS:{data.get('lat')},{data.get('lon')}")
                                
                                insert_data(data)
                                
                            except Exception as e:
                                print(f" Data processing error: {e}")

        except KeyboardInterrupt:
            print("\n Program stopped.")
            break
        except Exception as e:
            print(f" Connection lost: {e}")
            print(" Reconnecting in 3 seconds...")
            time.sleep(3)