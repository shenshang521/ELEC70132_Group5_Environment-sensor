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
// *** 用户配置区 (在这里修改校准值) ***
// ==========================================
// 如果是裸机（无外壳），建议填 3.0
// 如果加了塑料外壳（有积热），建议填 5.0
float TEMP_OFFSET = 5.0; 

// WiFi 设置
const char* ssid = "iPhone";
const char* password = "nishituzima";

// ==========================================
// 1. 硬件引脚定义
// ==========================================
#define I2C_SDA 21
#define I2C_SCL 22
#define SD_CS_PIN 5 

// ==========================================
// 2. 时间配置
// ==========================================
const char* ntpServer = "pool.ntp.org";
const char* timeZone = "GMT0BST,M3.5.0/1,M10.5.0"; 

// ==========================================
// 3. 硬件对象初始化
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Adafruit_BME680 bme; 
VL53L4CX sensor_vl53l4cx_sat(&Wire, -1); 

Adafruit_GPS GPS(&Wire);
#define GPS_I2C_ADDRESS 0x10

AsyncWebServer server(80);
AsyncEventSource events("/events");

// ==========================================
// 4. 全局变量
// ==========================================
unsigned long lastMeasurement = 0;
const unsigned long interval = 5000; 
bool sdReady = false;
bool bmeFound = false;
float gas_baseline = 0.0;
int burn_in_count = 0;

// ==========================================
// 5. HTML 网页内容
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Ultimate Sensor Hub</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Helvetica Neue', Arial, sans-serif; text-align: center; margin:0; background-color: #f0f2f5; color: #333; }
    h2 { background-color: #007bff; color: white; padding: 15px; margin: 0; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
    .content { padding: 20px; max-width: 600px; margin: 0 auto; }
    .card { background: white; padding: 20px; margin: 15px 0; border-radius: 15px; box-shadow: 0 4px 15px rgba(0,0,0,0.05); }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
    .val { font-size: 28px; font-weight: bold; color: #007bff; }
    .unit { font-size: 14px; color: #888; }
    .label { font-size: 14px; color: #555; margin-top: 5px; font-weight: 600; }
    .btn { display: inline-block; padding: 10px 20px; margin: 5px; text-decoration: none; color: white; border-radius: 5px; cursor: pointer; border: none; font-size: 14px; }
    .btn-down { background-color: #28a745; }
    .btn-del { background-color: #dc3545; }
    #clock { font-size: 40px; font-weight: bold; color: #222; }
    #sd_status { font-size: 12px; margin-top: 10px; }
    .quality-bar { height: 5px; width: 100%; background: #eee; margin-top:5px; border-radius:3px; overflow:hidden;}
    .quality-fill { height: 100%; background: #28a745; width: 0%; transition: width 1s;}
  </style>
</head>
<body>
  <h2>ESP32 Sensor Monitor</h2>
  <div class="content">
    <div class="card">
       <div id="clock">--:--:--</div>
       <div id="date" style="color:#777">Syncing...</div>
    </div>
    <div class="grid">
      <div class="card">
        <div class="val"><span id="temp">--</span><span class="unit">°C</span></div>
        <div class="label">Temperature</div>
      </div>
      <div class="card">
        <div class="val"><span id="hum">--</span><span class="unit">%</span></div>
        <div class="label">Humidity</div>
      </div>
      <div class="card">
        <div class="val"><span id="press">--</span><span class="unit">hPa</span></div>
        <div class="label">Pressure</div>
      </div>
      <div class="card">
        <div class="val"><span id="gas">--</span><span class="unit">k&Omega;</span></div>
        <div class="label">Gas Res</div>
        <div class="quality-bar"><div id="gas_bar" class="quality-fill"></div></div>
      </div>
    </div>
    <div class="card">
      <div class="label" style="margin-bottom:10px;">Laser Distance (ToF)</div>
      <div class="val"><span id="dist">--</span> <span class="unit">mm</span></div>
    </div>
    <div class="card">
      <div class="label">GPS Location</div>
      <div style="margin:10px 0; font-size:16px;">Lat: <span id="lat">--</span> <br> Lon: <span id="lon">--</span></div>
      <div style="color:#666;">Alt: <span id="alt">--</span> m</div>
      <a id="maps" href="#" target="_blank" class="btn" style="background:#007bff; display:none;">Google Maps</a>
    </div>
    <div class="card">
      <div class="label">SD Card Logger</div>
      <div id="sd_status" style="color:orange;">Checking...</div>
      <br><a href="/download" class="btn btn-down">Download CSV</a>
      <button onclick="deleteData()" class="btn btn-del">Clear Data</button>
    </div>
  </div>
<script>
function deleteData() {
  if (confirm("Delete all data?")) { fetch('/delete').then(r => r.ok ? alert("Cleared") : alert("Error")); }
}
if (!!window.EventSource) {
 var source = new EventSource('/events');
 source.addEventListener('update', function(e) {
  var obj = JSON.parse(e.data);
  document.getElementById("temp").innerText = obj.temp;
  document.getElementById("hum").innerText = obj.hum;
  document.getElementById("press").innerText = obj.press;
  document.getElementById("gas").innerText = obj.gas;
  document.getElementById("dist").innerText = obj.dist;
  document.getElementById("lat").innerText = obj.lat;
  document.getElementById("lon").innerText = obj.lon;
  document.getElementById("alt").innerText = obj.alt;
  document.getElementById("clock").innerText = obj.time.split(" ")[1]; 
  document.getElementById("date").innerText = obj.time.split(" ")[0]; 
  var sdDiv = document.getElementById("sd_status");
  if(obj.sd) { sdDiv.innerText = "Status: Writing (OK)"; sdDiv.style.color = "green"; }
  else { sdDiv.innerText = "Status: Error / Missing"; sdDiv.style.color = "red"; }
  var gasVal = parseFloat(obj.gas); var maxGas = 500.0; var pct = (gasVal / maxGas) * 100;
  if(pct > 100) pct = 100; document.getElementById("gas_bar").style.width = pct + "%";
  if(obj.lat != 0) {
      document.getElementById("maps").href = "https://www.google.com/maps/search/?api=1&query=" + obj.lat + "," + obj.lon;
      document.getElementById("maps").style.display = "inline-block";
  }
 }, false);
}
</script>
</body>
</html>
)rawliteral";

// ==========================================
// 6. 辅助函数
// ==========================================
String getFormattedTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){ return "N/A"; }
  char timeStringBuff[30];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

void initSD() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD: Mount Failed");
    sdReady = false;
  } else {
    Serial.println("SD: Mounted");
    sdReady = true;
    if (!SD.exists("/data.csv")) {
      File file = SD.open("/data.csv", FILE_WRITE);
      if (file) {
        file.println("Timestamp,Temp_C,Hum_%,Press_hPa,Gas_KOhms,Dist_mm,Lat,Lon,Alt_m");
        file.close();
      }
    }
  }
}

// ==========================================
// 7. SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000); 
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); 

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println("OLED Error");
  } else {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0); display.println("System Booting..."); display.display();
  }

  initSD();

  // WiFi
  display.setCursor(0,15); display.println("WiFi Connecting..."); display.display();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) { delay(500); Serial.print("."); retry++; }

  if (WiFi.status() == WL_CONNECTED) {
    display.println("WiFi OK!"); display.display();
    configTime(0, 0, ntpServer); setenv("TZ", timeZone, 1); tzset();
  } else {
    display.println("WiFi Failed"); display.display();
  }

  // BME680
  if (!bme.begin()) {
    Serial.println("BME680 Not Found"); bmeFound = false;
  } else {
    bmeFound = true;
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
  }

  // VL53L4CX & GPS
  sensor_vl53l4cx_sat.InitSensor(0x29);
  sensor_vl53l4cx_sat.VL53L4CX_StartMeasurement();
  if (!GPS.begin(GPS_I2C_ADDRESS)) Serial.println("GPS Not Found");
  else { GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA); GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); }

  server.addHandler(&events);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200, "text/html", index_html); });
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *r){ if(SD.exists("/data.csv")) r->send(SD, "/data.csv", "text/csv", true); else r->send(404); });
  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *r){ SD.remove("/data.csv"); initSD(); r->send(200, "text/plain", "Deleted"); });
  server.begin();
}

// ==========================================
// 8. LOOP
// ==========================================
void loop() {
  GPS.read();
  if (GPS.newNMEAreceived()) GPS.parse(GPS.lastNMEA());

  if (millis() - lastMeasurement >= interval) {
    lastMeasurement = millis();

    float temp = 0, hum = 0, press = 0, gas = 0;
    if (bmeFound && bme.performReading()) {
      // ----------------------------------------------------
      // 【这里使用了顶部的全局变量进行校准】
      temp = bme.temperature - TEMP_OFFSET; 
      // ----------------------------------------------------
      hum = bme.humidity;
      press = bme.pressure / 100.0;
      gas = bme.gas_resistance / 1000.0;

      if(burn_in_count < 30) burn_in_count++; 
      else if (gas > gas_baseline) gas_baseline = gas;
    }

    int distance = 0;
    VL53L4CX_MultiRangingData_t MultiRangingData;
    uint8_t NewDataReady = 0;
    sensor_vl53l4cx_sat.VL53L4CX_GetMeasurementDataReady(&NewDataReady);
    if (NewDataReady) {
      sensor_vl53l4cx_sat.VL53L4CX_GetMultiRangingData(&MultiRangingData);
      if (MultiRangingData.NumberOfObjectsFound > 0) distance = MultiRangingData.RangeData[0].RangeMilliMeter;
      sensor_vl53l4cx_sat.VL53L4CX_ClearInterruptAndStartMeasurement();
    }

    float lat = GPS.latitudeDegrees; float lon = GPS.longitudeDegrees; float alt = GPS.altitude;
    if(isnan(lat)) lat = 0; if(isnan(lon)) lon = 0;
    String timeStr = getFormattedTime();

    if(!sdReady) initSD();
    bool writeSuccess = false;
    if (sdReady) {
      File file = SD.open("/data.csv", FILE_APPEND);
      if (file) {
        file.printf("%s,%.2f,%.1f,%.1f,%.2f,%d,%.6f,%.6f,%.1f\n", timeStr.c_str(), temp, hum, press, gas, distance, lat, lon, alt);
        file.close(); writeSuccess = true;
      } else sdReady = false;
    }

    Serial.printf("[%s] T:%.2f H:%.1f P:%.1f G:%.2f D:%d\n", timeStr.c_str(), temp, hum, press, gas, distance);

    char jsonBuff[512];
    snprintf(jsonBuff, sizeof(jsonBuff), "{\"time\":\"%s\",\"temp\":%.1f,\"hum\":%.0f,\"press\":%.1f,\"gas\":%.1f,\"dist\":%d,\"lat\":%.6f,\"lon\":%.6f,\"alt\":%.1f,\"sd\":%d}",
      timeStr.c_str(), temp, hum, press, gas, distance, lat, lon, alt, writeSuccess ? 1 : 0);
    events.send(jsonBuff, "update", millis());

    display.clearDisplay(); display.setCursor(0, 0);
    display.println(WiFi.localIP());
    display.setCursor(0, 12); display.printf("T:%.1f H:%.0f%%", temp, hum);
    display.setCursor(0, 24); display.printf("Gas:%.0f D:%d", gas, distance);
    display.setCursor(0, 36); 
    if(lat != 0) { display.printf("Lat:%.4f", lat); display.setCursor(0, 46); display.printf("Lon:%.4f", lon); } 
    else display.print("GPS Searching...");
    display.setCursor(0, 56); display.printf("SD: %s", writeSuccess ? "REC" : "ERR");
    display.display();
  }
}