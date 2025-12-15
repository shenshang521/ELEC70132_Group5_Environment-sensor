#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Adafruit_BME680.h>
#include <Adafruit_GPS.h>
#include <vl53l4cx_class.h>
#include <FS.h>
#include <SD.h>
#include <time.h> 
#include "secrets.h"     
#include "index_html.h"  

// ==========================================
// MODULE 1: Global Configurations
// ------------------------------------------
// NTP Settings and Hardware Pin Definitions
// =========================================
const char* ntpServer = "pool.ntp.org";
const char* timeZone = "GMT0BST,M3.5.0/1,M10.5.0";
#define SD_CS_PIN 5 
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define GPS_I2C_ADDRESS 0x10

// ==========================================
// MODULE 2: Hardware Objects & Variables
// ------------------------------------------
// Initialization of global objects for sensors, display, and web server.
// ==========================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BME680 bme; 
VL53L4CX sensor_vl53l4cx_sat;
Adafruit_GPS GPS(&Wire);
AsyncWebServer server(80);
AsyncEventSource events("/events");

unsigned long lastMeasurement = 0;
const unsigned long interval = 5000; 
bool sdReady = false;


// ==========================================
// MODULE 3: Helper Functions
// ------------------------------------------
// Utility functions used by the main logic.
// ==========================================
String getFormattedTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){ return "Time Error"; }
  char timeStringBuff[30];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

// ==========================================
// MODULE 4: SETUP FUNCTION
// ------------------------------------------
// Hardware initialization and Network connection sequence.
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("\n System Starting...");

  Wire.begin();

  // 4.1 OLED 
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { 
    Serial.println(" OLED Error");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Init Hardware...");
    display.display();
  }

  // 4.2 SD Card 
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(" SD Mount Failed");
    sdReady = false;
  } else {
    Serial.println(" SD Mounted");
    sdReady = true;
    if (!SD.exists("/data.csv")) {
      File file = SD.open("/data.csv", FILE_WRITE);
      if (file) {
        file.println("Timestamp,Temp_C,Hum_%,Press_hPa,Gas_KOhms,AQ_%,Dist_mm,Lat,Lon,Alt_m");
        file.close();
      }
    }
  }

  //  4.3 WiFi and Time Sync
  display.clearDisplay(); display.setCursor(0,0); display.println("Connecting WiFi..."); display.display();
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500); Serial.print("."); retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n WiFi Connected!");
    configTime(0, 0, ntpServer);
    setenv("TZ", timeZone, 1);
    tzset();
    struct tm timeinfo;
    int retryTime = 0;
    while (!getLocalTime(&timeinfo) && retryTime < 10) {
      Serial.print("."); delay(500); retryTime++;
    }
    Serial.println("\n Time Synced!");
  } 

  // 4.4 Sensors
  if (!bme.begin()) Serial.println(" BME688 Error");
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  sensor_vl53l4cx_sat.setI2cDevice(&Wire);
  sensor_vl53l4cx_sat.InitSensor(VL53L4CX_DEFAULT_DEVICE_ADDRESS);
  sensor_vl53l4cx_sat.VL53L4CX_StartMeasurement();

  if (!GPS.begin(GPS_I2C_ADDRESS)) Serial.println(" GPS Error");
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

  // 4.5 Web Server
  server.addHandler(&events);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200, "text/html", index_html); });
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *r){ if(SD.exists("/data.csv")) r->send(SD, "/data.csv", "text/csv", true); else r->send(404); });
  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *r){ 
    SD.remove("/data.csv"); 
    File f = SD.open("/data.csv", FILE_WRITE); 
    if(f){f.println("Timestamp,Temp_C,Hum_%,Press_hPa,Gas_KOhms,AQ_%,Dist_mm,Lat,Lon,Alt_m"); f.close();}
    r->send(200, "text/plain", "Deleted"); 
  });
  
  server.begin();
}

// ==========================================
// MODULE 5: LOOP FUNCTION
// ------------------------------------------
// Main execution loop.
// 1. GPS Parsing
// 2. Sensor Reading (Interval: 5s)
// 3. Data Processing & Logging
// 4. Interface Updates (Web & OLED)
// ==========================================
void loop() {
  GPS.read();
  if (GPS.newNMEAreceived()) GPS.parse(GPS.lastNMEA());

  if (millis() - lastMeasurement >= interval) {
    lastMeasurement = millis();

    bme.performReading();
    float temp = bme.temperature;
    float hum = bme.humidity;
    float press = bme.pressure / 100.0;
    float gas = bme.gas_resistance / 1000.0;

    float aq_score = 0.0;
    if (gas >= 50.0) aq_score = 100.0;       // good air quality
    else if (gas <= 5.0) aq_score = 0.0;     // poor air quality
    else aq_score = (gas - 5.0) * (100.0 / 45.0); // Interval Check
    // -------------------------------------------------------------

    VL53L4CX_MultiRangingData_t MultiRangingData;
    uint8_t NewDataReady = 0;
    int distance = 0;
    sensor_vl53l4cx_sat.VL53L4CX_GetMeasurementDataReady(&NewDataReady);
    if (NewDataReady) {
      sensor_vl53l4cx_sat.VL53L4CX_GetMultiRangingData(&MultiRangingData);
      if (MultiRangingData.NumberOfObjectsFound > 0) distance = MultiRangingData.RangeData[0].RangeMilliMeter;
      sensor_vl53l4cx_sat.VL53L4CX_ClearInterruptAndStartMeasurement();
    }

    float lat = GPS.latitudeDegrees;
    float lon = GPS.longitudeDegrees;
    float alt = GPS.altitude;
    if (isnan(temp)) temp = 0;

    String timeStr = getFormattedTime();

    //  SD CARD LOGGING 
    bool writeSuccess = false;
    if (sdReady) {
      File file = SD.open("/data.csv", FILE_APPEND);
      if (file) {
        file.print(timeStr); file.print(",");
        file.print(temp); file.print(",");
        file.print(hum); file.print(",");
        file.print(press); file.print(",");
        file.print(gas); file.print(",");
        file.print(aq_score, 0); file.print(","); // 写入 AQ
        file.print(distance); file.print(",");
        file.print(lat, 6); file.print(",");
        file.print(lon, 6); file.print(",");
        file.println(alt);
        file.close();
        writeSuccess = true;
      }
    }

     // SERIAL 
    Serial.println("\n---  " + timeStr + " ---");
    Serial.printf("Temp:    %.2f C\n", temp);
    Serial.printf("Hum:     %.2f %%\n", hum);       
    Serial.printf("Press:   %.2f hPa\n", press);    
    Serial.printf("Gas:     %.2f KOhms\n", gas);
    Serial.printf("AirQual: %.0f %%\n", aq_score); // AQ 
    Serial.printf("Dist:    %d mm\n", distance);
    
    if (lat != 0.0) {
        Serial.printf("GPS:     Lat: %.6f, Lon: %.6f, Alt: %.2fm\n", lat, lon, alt);
    } else {
        Serial.println("GPS:     No Fix (Searching...)");
    }
    
    Serial.printf("SD:      %s\n", writeSuccess ? " OK" : " Error");
    Serial.printf("IP:      %s\n", WiFi.localIP().toString().c_str());

    // Web Send
    String json = "{";
    json += "\"time\":\"" + timeStr + "\","; 
    json += "\"temp\":" + String(temp, 1) + ",";
    json += "\"hum\":" + String(hum, 0) + ",";
    json += "\"press\":" + String(press, 1) + ",";
    json += "\"gas\":" + String(gas, 1) + ",";
    json += "\"aq\":" + String(aq_score, 0) + ","; // AQ
    json += "\"dist\":" + String(distance) + ",";
    json += "\"lat\":" + String(lat, 6) + ",";
    json += "\"lon\":" + String(lon, 6) + ",";
    json += "\"alt\":" + String(alt, 1) + ",";
    json += "\"sd\":" + String(writeSuccess ? 1 : 0);
    json += "}";
    events.send(json.c_str(), "update", millis());

    // OLED Display
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(WiFi.localIP()); 
    
    if (timeStr.length() > 8) display.println(timeStr.substring(11));
    else display.println("Time Syncing..");

    display.printf("T:%.1f H:%.0f%%\n", temp, hum);
    display.printf("P:%.0f AQ:%.0f%%\n", press, aq_score); 
    display.printf("Dist: %d mm\n", distance);
    
    if (lat != 0.0) display.printf("GPS:OK SD:%s\n", writeSuccess ? "OK" : "X");
    else display.printf("GPS:No SD:%s\n", writeSuccess ? "OK" : "X");
    
    display.display();
  }
}