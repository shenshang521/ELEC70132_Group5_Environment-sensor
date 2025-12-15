import sqlite3
import csv
import os

# ==========================================
# MODULE 1: Configuration & File Paths
# ------------------------------------------
# Sets up the paths for input (Database) and output (CSV).
# - BASE_DIR: Ensures we look in the same folder as the script.
# - DB_PATH: The SQLite database file created by the collection script.
# - CSV_OUTPUT_PATH: The destination file for the Excel-readable export.
# ==========================================
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.path.join(BASE_DIR, "esp32_sse_data.sqlite3")
CSV_OUTPUT_PATH = os.path.join(BASE_DIR, "sensor_data_export.csv")
TABLE_NAME = "sensor_data"

# ==========================================
# MODULE 2: Main Execution Block
# ------------------------------------------
# 1. Verifies the database exists.
# 2. Connects to SQLite.
# 3. Fetches all data (including new Air Quality & SD Status columns).
# 4. Writes the data to a CSV file with proper headers.
# ==========================================
if __name__ == "__main__":
    print(f" Reading database: {DB_PATH}")

    # 1. Check if the database exists
    if not os.path.exists(DB_PATH):
        print(f" Error: Database file not found!\nPlease run the data collection script first to collect some data before exporting.")
        exit()

    try:
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()

        # 2. Query all data
        # UPDATE: Added 'aq' and 'sd_status' to the SELECT statement
        sql = f"""
        SELECT 
            id, receive_time, 
            temp, hum, press, gas, 
            aq,         -- New: Air Quality
            dist, 
            lat, lon, alt,
            sd_status   -- New: SD Card Status
        FROM {TABLE_NAME}
        """
        cursor.execute(sql)
        rows = cursor.fetchall() # Retrieve all rows from the query
        conn.close()             # Close DB connection

        if not rows:
            print(" Warning: The database is empty, no data yet.")
        else:
            # 3. Write to CSV file
            # 'newline=""' is important to prevent blank lines between rows in Excel
            with open(CSV_OUTPUT_PATH, "w", newline="", encoding="utf-8") as csv_file:
                csv_writer = csv.writer(csv_file)
                
                # UPDATE: Write nice headers including the new columns
                headers = [
                    "ID", "Time", 
                    "Temp (C)", "Hum (%)", "Press (hPa)", "Gas (kOhm)", 
                    "Air Quality (%)", # New Header
                    "Dist (mm)", 
                    "Lat", "Lon", "Alt (m)",
                    "SD Status (1=OK)" # New Header
                ]
                csv_writer.writerow(headers) # Write the header row
                
                # Write all data rows
                csv_writer.writerows(rows)
            
            print(f" Success! Exported {len(rows)} rows to -> {CSV_OUTPUT_PATH}")
            print(" You can now go to the folder and double-click to open this CSV file!")

    except sqlite3.OperationalError:
        print(" Database error: Table structure mismatch.\nPlease ensure you deleted the old database and collected new data.")
    except PermissionError:
        print(f" Cannot write file!\nPlease check: Is '{os.path.basename(CSV_OUTPUT_PATH)}' already open in Excel?\nPlease close Excel first, then run this program.")
    except Exception as e:
        print(f" Unknown error occurred: {e}")