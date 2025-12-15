import requests
import json
import sqlite3
import os
import time
from datetime import datetime

# ==========================================
# MODULE 1: Configuration & Global Variables
# ------------------------------------------
# Setup file paths and network settings.
# - BASE_DIR: Ensures the database is created in the same folder as this script.
# - DB_PATH: The full path to the SQLite database file.
# - URL: The endpoint to connect to on the ESP32 (must match ESP32's IP).
# ==========================================
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.path.join(BASE_DIR, "esp32_sse_data.sqlite3")
TABLE_NAME = "sensor_data"

# Change this IP to match the one displayed on your ESP32 OLED screen
ESP_IP = "http://172.20.10.6"  
URL = f"{ESP_IP}/events"

# ==========================================
# MODULE 2: Database Initialization
# ------------------------------------------
# Checks if the database file exists. If not, it creates it.
# It defines the table structure to match the data sent by the ESP32.
# *UPDATED*: Now includes 'aq' (Air Quality) and 'sd_status' columns.
# ==========================================
def init_database():
    try:
        conn = sqlite3.connect(DB_PATH) # Connect to (or create) the DB
        cursor = conn.cursor()
        
        # SQL query to create the table
        create_table_sql = f"""
        CREATE TABLE IF NOT EXISTS {TABLE_NAME} (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            receive_time TEXT NOT NULL,
            temp REAL,
            hum REAL,
            press REAL,
            gas REAL,
            aq REAL,          -- New: Air Quality Percentage
            dist INTEGER,
            lat REAL,
            lon REAL,
            alt REAL,
            sd_status INTEGER -- New: SD Card Status (1=OK, 0=Error)
        );
        """
        cursor.execute(create_table_sql) # Execute the query
        conn.commit()                    # Save changes
        conn.close()                     # Close connection
        print(f" Database ready: {DB_PATH}")
    except Exception as e:
        print(f" Database initialization failed: {e}")

# ==========================================
# MODULE 3: Data Insertion
# ------------------------------------------
# This function is called every time a new data packet arrives.
# It opens a connection, prepares the SQL INSERT statement, maps the 
# JSON keys to the database columns, and saves the data.
# ==========================================
def insert_data(data):
    try:
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()
        # Generate a timestamp for when the data was received by Python
        current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        # SQL INSERT statement with placeholders (?) for security
        insert_sql = f"""
        INSERT INTO {TABLE_NAME} 
        (receive_time, temp, hum, press, gas, aq, dist, lat, lon, alt, sd_status) 
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        """
        
        # Map JSON data to SQL columns using .get() to handle missing keys safely
        cursor.execute(insert_sql, (
            current_time, 
            data.get('temp', 0),
            data.get('hum', 0),
            data.get('press', 0),
            data.get('gas', 0),
            data.get('aq', 0),        # Insert Air Quality
            data.get('dist', -1),
            data.get('lat', 0.0),
            data.get('lon', 0.0),
            data.get('alt', 0.0),
            data.get('sd', 0)         # Insert SD Status
        ))
        conn.commit() # Commit the transaction
        conn.close()  # Close the connection
    except Exception as e:
        print(f" Failed to write to database: {e}")

# ==========================================
# MODULE 4: Main Execution Loop
# ------------------------------------------
# 1. Initializes the database.
# 2. Sets up HTTP headers for Server-Sent Events (SSE).
# 3. Connects to the ESP32 and enters a loop to process the stream.
# 4. Handles automatic reconnection if the WiFi drops.
# ==========================================
if __name__ == "__main__":
    # Check for old DB file to warn user about schema changes
    if os.path.exists(DB_PATH):
        print("Note: If you see 'no such column' errors, please delete the old .sqlite3 file.")
    
    init_database()
    
    # HTTP Headers required for Event Streams
    headers = {
        "Accept": "text/event-stream",
        "Cache-Control": "no-cache",
        "Connection": "keep-alive",
    }
    
    print(f"Connecting to {URL} ...")

    while True: # Infinite loop for auto-reconnection
        try:
            print("Initiating connection request...")
            # Open a persistent HTTP connection to the ESP32
            with requests.get(URL, stream=True, headers=headers, timeout=None) as response:
                response.raise_for_status() # Check for HTTP errors (e.g. 404)
                print(" Connection successful! Waiting for sensor data...")
                
                # Iterate over every line received from the ESP32
                for line in response.iter_lines():
                    if line:
                        decoded_line = line.decode("utf-8")
                        # SSE data lines always start with "data: "
                        if decoded_line.startswith("data: "):
                            try:
                                # Extract the JSON string part
                                json_str = decoded_line.split("data: ", 1)[1].strip()
                                data = json.loads(json_str) # Parse JSON
                                
                                # Print real-time data to console (including AQ now)
                                print(f" T:{data.get('temp')}C | H:{data.get('hum')}% | AQ:{data.get('aq')}% | D:{data.get('dist')}mm | GPS:{data.get('lat')},{data.get('lon')}")
                                
                                # Save to SQLite
                                insert_data(data)
                                
                            except Exception as e:
                                print(f" Data processing error: {e}")

        except KeyboardInterrupt:
            # Handle Ctrl+C gracefully
            print("\n Program stopped.")
            break
        except Exception as e:
            # Handle connection loss or timeouts
            print(f" Connection lost: {e}")
            print(" Reconnecting in 3 seconds...")
            time.sleep(3)