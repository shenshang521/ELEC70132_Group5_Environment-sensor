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

// ==========================================
// 1. WiFi Configuration
// ==========================================
const char* ssid = "iPhone";
const char* password = "nishituzima";

// ==========================================
// 2. Time Configuration (NTP)
// ==========================================
const char* ntpServer = "pool.ntp.org";
// UK Time Zone (London) handling BST/GMT automatically
const char* timeZone = "GMT0BST,M3.5.0/1,M10.5.0";

// ==========================================
// 3. SD Card Configuration
// ==========================================
#define SD_CS_PIN 5 

// ==========================================
// 4. HTML Page Content
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Sensor Hub</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin:0; background-color: #f4f4f4; }
    h2 { background-color: #0c69d6; color: white; padding: 15px; margin: 0; }
    .content { padding: 20px; max-width: 600px; margin: 0 auto; }
    .card { background: white; padding: 20px; margin: 10px; border-radius: 10px; box-shadow: 2px 2px 10px rgba(0,0,0,0.1); }
    .unit { font-size: 14px; color: #888; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .btn { display: inline-block; padding: 10px 20px; margin: 5px; text-decoration: none; color: white; border-radius: 5px; cursor: pointer; border: none; font-size: 16px; }
    .btn-down { background-color: #28a745; }
    .btn-del { background-color: #dc3545; }
    .icon { font-size: 20px; margin-right: 5px; }
    #clock { font-size: 32px; font-weight: bold; color: #333; margin-bottom: 5px; }
    #date { font-size: 16px; color: #666; margin-bottom: 20px; }
  </style>
</head>
<body>
  <h2>ESP32 Sensor Dashboard</h2>
  <div class="content">
    <div class="card">
       <div id="clock">--:--:--</div>
       <div id="date">Loading Time...</div>
    </div>
    <div class="card">
      <p><span class="icon">&#128190;</span> SD Card Data</p>
      <a href="/download" class="btn btn-down">&#128229; Download CSV</a>
      <button onclick="deleteData()" class="btn btn-del">&#128465; Clear Data</button>
      <p id="sd_status" style="font-size:12px; color:#666; margin-top:5px;">State: Waiting...</p>
    </div>
    <div class="grid">
      <div class="card">
        <p><span class="icon">&#127777;</span> Temperature</p>
        <p><span id="temp">--</span> <span class="unit">°C</span></p>
      </div>
      <div class="card">
        <p><span class="icon">&#128167;</span> Humidity</p>
        <p><span id="hum">--</span> <span class="unit">%</span></p>
      </div>
      <div class="card">
        <p><span class="icon">&#128336;</span> Pressure</p>
        <p><span id="press">--</span> <span class="unit">hPa</span></p>
      </div>
      <div class="card">
        <p><span class="icon">&#128168;</span> Gas Res</p>
        <p><span id="gas">--</span> <span class="unit">KΩ</span></p>
      </div>
    </div>
    <div class="card">
      <p><span class="icon">&#128207;</span> ToF Distance</p>
      <p><span id="dist">--</span> <span class="unit">mm</span></p>
    </div>
    <div class="card">
      <p><span class="icon">&#127757;</span> GPS Location</p>
      <p>Lat: <span id="lat">--</span> | Lon: <span id="lon">--</span></p>
      <p>Alt: <span id="alt">--</span> <span class="unit">m</span></p>
      <a id="maps" href="#" target="_blank" style="display:inline-block;margin-top:10px;text-decoration:none;color:#0c69d6;border:1px solid #0c69d6;padding:5px 10px;border-radius:5px;">View on Google Maps</a>
    </div>
  </div>
<script>
function deleteData() {
  if (confirm("Are you sure?")) {
    fetch('/delete').then(r => r.ok ? alert("Deleted!") : alert("Failed"));
  }
}
if (!!window.EventSource) {
 var source = new EventSource('/events');
 source.addEventListener('update', function(e) {
  var obj = JSON.parse(e.data);
  document.getElementById("temp").innerHTML = obj.temp;
  document.getElementById("hum").innerHTML = obj.hum;
  document.getElementById("press").innerHTML = obj.press;
  document.getElementById("gas").innerHTML = obj.gas;
  document.getElementById("dist").innerHTML = obj.dist;
  document.getElementById("lat").innerHTML = obj.lat;
  document.getElementById("lon").innerHTML = obj.lon;
  document.getElementById("alt").innerHTML = obj.alt;
  document.getElementById("sd_status").innerHTML = "SD Status: " + (obj.sd ? "Writing OK" : "Error/Missing");
  document.getElementById("clock").innerHTML = obj.time.split(" ")[1]; 
  document.getElementById("date").innerHTML = obj.time.split(" ")[0]; 
  if(obj.lat != 0) {
      document.getElementById("maps").href = "https://www.google.com/maps/search/?api=1&query=" + obj.lat + "," + obj.lon;
  }
 }, false);
}
</script>
</body>
</html>
)rawliteral";

// ==========================================
// 5. Hardware Objects Definition
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Adafruit_BME680 bme; 
VL53L4CX sensor_vl53l4cx_sat;

Adafruit_GPS GPS(&Wire);
#define GPS_I2C_ADDRESS 0x10

AsyncWebServer server(80);
AsyncEventSource events("/events");

unsigned long lastMeasurement = 0;
const unsigned long interval = 5000; 
bool sdReady = false;

// Helper to get formatted London time string
String getFormattedTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){ return "Time Error"; }
  char timeStringBuff[30];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

// ==========================================
// 6. SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("\n System Starting...");

  Wire.begin();

  // --- OLED Initialization ---
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

  // --- SD Card Initialization ---
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(" SD Mount Failed");
    sdReady = false;
  } else {
    Serial.println(" SD Mounted");
    sdReady = true;
    if (!SD.exists("/data.csv")) {
      File file = SD.open("/data.csv", FILE_WRITE);
      if (file) {
        file.println("Timestamp,Temp_C,Hum_%,Press_hPa,Gas_KOhms,Dist_mm,Lat,Lon,Alt_m");
        file.close();
      }
    }
  }

  // --- WiFi Connection ---
  display.clearDisplay(); display.setCursor(0,0); display.println("Connecting WiFi..."); display.display();
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500); Serial.print("."); retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n WiFi Connected!");
    // Sync Time
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

  // --- Sensors Initialization ---
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

  // --- Web Routes ---
  server.addHandler(&events);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200, "text/html", index_html); });
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *r){ if(SD.exists("/data.csv")) r->send(SD, "/data.csv", "text/csv", true); else r->send(404); });
  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *r){ 
    SD.remove("/data.csv"); 
    File f = SD.open("/data.csv", FILE_WRITE); if(f){f.println("Timestamp,Temp_C,Hum_%,Press_hPa,Gas_KOhms,Dist_mm,Lat,Lon,Alt_m"); f.close();}
    r->send(200, "text/plain", "Deleted"); 
  });
  
  server.begin();
}

// ==========================================
// 7. LOOP
// ==========================================
void loop() {
  // GPS must be read constantly in the loop
  GPS.read();
  if (GPS.newNMEAreceived()) GPS.parse(GPS.lastNMEA());

  if (millis() - lastMeasurement >= interval) {
    lastMeasurement = millis();

    // 1. Read Sensors
    bme.performReading();
    float temp = bme.temperature;
    float hum = bme.humidity;
    float press = bme.pressure / 100.0;
    float gas = bme.gas_resistance / 1000.0;

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

    // 3. Write to SD Card
    bool writeSuccess = false;
    if (sdReady) {
      File file = SD.open("/data.csv", FILE_APPEND);
      if (file) {
        file.print(timeStr); file.print(",");
        file.print(temp); file.print(",");
        file.print(hum); file.print(",");
        file.print(press); file.print(",");
        file.print(gas); file.print(",");
        file.print(distance); file.print(",");
        file.print(lat, 6); file.print(",");
        file.print(lon, 6); file.print(",");
        file.println(alt);
        file.close();
        writeSuccess = true;
      }
    }

    // 4. Serial Output
    Serial.println("\n---  " + timeStr + " ---");
    Serial.printf("Temp:    %.2f C\n", temp);
    Serial.printf("Hum:     %.2f %%\n", hum);       
    Serial.printf("Press:   %.2f hPa\n", press);    
    Serial.printf("Gas:     %.2f KOhms\n", gas);    
    Serial.printf("Dist:    %d mm\n", distance);
    
    if (lat != 0.0) {
        Serial.printf("GPS:     Lat: %.6f, Lon: %.6f, Alt: %.2fm\n", lat, lon, alt);
    } else {
        Serial.println("GPS:     No Fix (Searching...)");
    }
    
    Serial.printf("SD:      %s\n", writeSuccess ? " OK" : " Error");
    Serial.printf("IP:      %s\n", WiFi.localIP().toString().c_str());

    // 5. Send Web Data
    String json = "{";
    json += "\"time\":\"" + timeStr + "\","; 
    json += "\"temp\":" + String(temp, 1) + ",";
    json += "\"hum\":" + String(hum, 0) + ",";
    json += "\"press\":" + String(press, 1) + ",";
    json += "\"gas\":" + String(gas, 1) + ",";
    json += "\"dist\":" + String(distance) + ",";
    json += "\"lat\":" + String(lat, 6) + ",";
    json += "\"lon\":" + String(lon, 6) + ",";
    json += "\"alt\":" + String(alt, 1) + ",";
    json += "\"sd\":" + String(writeSuccess ? 1 : 0);
    json += "}";
    events.send(json.c_str(), "update", millis());

    // 6. OLED Display Update (Includes Altitude now)
    display.clearDisplay();
    display.setCursor(0, 0);

    // Row 1: IP Address
    display.println(WiFi.localIP());

    // Row 2: Basic Sensor Data
    display.printf("T:%.1f H:%.0f%% D:%d\n", temp, hum, distance);

    // Row 3-4: GPS Data
    if (lat != 0.0) {
       display.printf("Lat:%.4f\n", lat);
       display.printf("Lon:%.4f\n", lon);
       // Row 5: Altitude + SD Status
       display.printf("Al:%.1fm SD:%s\n", alt, writeSuccess ? "OK" : "X");
    } else {
       display.println("GPS: Searching...");
       display.println("Wait for Fix...");
       // Row 5: No Alt + SD Status
       display.printf("Al:-- SD:%s\n", writeSuccess ? "OK" : "X");
    }

    display.display();
  }
}
