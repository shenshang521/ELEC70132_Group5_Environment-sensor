#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>  // 🟢 新增：SD卡库
#include <FS.h>  // 🟢 新增：文件系统
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Adafruit_BME680.h>
#include <Adafruit_GPS.h>
#include <vl53l4cx_class.h>

// ================= WIFI 配置 =================
const char* ssid = "iPhone";
const char* password = "nishituzima";

// ================= 🟢 SD卡配置 =================
const int SD_CS_PIN = 5;  // 你的原理图确认是 GPIO 5
const char* logFileName = "/sensor_data.csv";

// ================= 网页代码 =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 Sensor Hub</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f4; margin: 0; padding: 20px; }
    h1 { color: #333; }
    .container { display: flex; flex-wrap: wrap; justify-content: center; gap: 15px; }
    .card { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); width: 150px; }
    .card h3 { margin: 0; color: #666; font-size: 1rem; }
    .card p { margin: 10px 0 0; font-size: 1.5rem; font-weight: bold; color: #0275d8; }
    .gps-card { width: 320px; }
  </style>
</head>
<body>
  <h1>🌡️ ESP32 Environment Monitor</h1>
  <div class="container">
    <div class="card"><h3>Temperature</h3><p><span id="temp">--</span> &deg;C</p></div>
    <div class="card"><h3>Humidity</h3><p><span id="hum">--</span> %</p></div>
    <div class="card"><h3>Pressure</h3><p><span id="press">--</span> hPa</p></div>
    <div class="card"><h3>Gas Res.</h3><p><span id="gas">--</span> k&Omega;</p></div>
    <div class="card"><h3>Distance</h3><p><span id="dist">--</span> mm</p></div>
    <div class="card"><h3>Altitude</h3><p><span id="alt">--</span> m</p></div>
    <div class="card gps-card"><h3>GPS Location</h3><p style="font-size:1rem"><span id="lat">--</span>, <span id="lon">--</span></p></div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 source.addEventListener('open', function(e) { console.log("Events Connected"); }, false);
 source.addEventListener('error', function(e) { if (e.target.readyState != EventSource.OPEN) { console.log("Events Disconnected"); } }, false);
 source.addEventListener('update', function(e) {
  var obj = JSON.parse(e.data);
  document.getElementById("temp").innerHTML = obj.temp;
  document.getElementById("hum").innerHTML = obj.hum;
  document.getElementById("press").innerHTML = obj.press;
  document.getElementById("gas").innerHTML = obj.gas;
  document.getElementById("dist").innerHTML = obj.dist;
  document.getElementById("alt").innerHTML = obj.alt;
  document.getElementById("lat").innerHTML = obj.lat.toFixed(6);
  document.getElementById("lon").innerHTML = obj.lon.toFixed(6);
 }, false);
}
</script>
</body>
</html>
)rawliteral";

// ================= 硬件对象 =================
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
const unsigned long interval = 5000; // 5s

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n🚀 System Starting...");

  // ⚠️ 这里我们不碰 Pin 6，因为你舍友焊死了，它是常开的。
  // ⚠️ 这里我们也不碰 Pin 9，既然你之前的代码能跑，说明 GPS 默认也是开的。

  Wire.begin();

  // --- OLED 初始化 (保持原样) ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    Serial.println("❌ OLED Error");
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Init SD Card..."); // 提示正在初始化SD
  display.display();
  
  // 🟢 插入：SD 卡初始化
  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("❌ SD Fail!");
    display.println("SD Fail!");
  } else {
    Serial.println("✅ SD OK!");
    display.println("SD OK!");
    // 写入表头
    if (!SD.exists(logFileName)) {
      File file = SD.open(logFileName, FILE_WRITE);
      if (file) {
        file.println("TimeMillis,Temp,Hum,Press,Gas,Dist,Lat,Lon,Alt");
        file.close();
      }
    }
  }
  display.display();
  delay(1000); // 暂停一下让你看到 SD 状态

  // --- WiFi 连接 ---
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  int retry_count = 0;
  while (WiFi.status() != WL_CONNECTED && retry_count < 20) {
    delay(500);
    Serial.print(".");
    retry_count++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.println(WiFi.localIP());
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("WiFi OK!");
    display.println(WiFi.localIP());
    display.display();
  } else {
    Serial.println("\n❌ WiFi Failed");
    display.println("WiFi Fail!");
    display.display();
  }
  delay(1000);

  // --- BME688 初始化 ---
  if (!bme.begin()) {
    Serial.println("❌ BME688 Error");
    display.println("BME Fail");
    display.display();
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  // --- ToF 初始化 (保持队友逻辑) ---
  Serial.println("Init ToF...");
  sensor_vl53l4cx_sat.setI2cDevice(&Wire);
  sensor_vl53l4cx_sat.InitSensor(VL53L4CX_DEFAULT_DEVICE_ADDRESS);
  sensor_vl53l4cx_sat.VL53L4CX_StartMeasurement();

  // --- GPS 初始化 ---
  if (!GPS.begin(GPS_I2C_ADDRESS)) {
    Serial.println("❌ GPS Error");
    display.println("GPS Fail");
    display.display();
  }
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

  // --- Server ---
  server.addHandler(&events);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  server.begin();
  
  Serial.println("✅ All Ready!");
}

// ================= LOOP =================
void loop() {
  GPS.read();
  if (GPS.newNMEAreceived()) {
    GPS.parse(GPS.lastNMEA()); 
  }

  if (millis() - lastMeasurement >= interval) {
    lastMeasurement = millis();

    // 1. 读取传感器
    if (!bme.performReading()) {
      Serial.println("BME reading failed");
      return; 
    }
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
      if (MultiRangingData.NumberOfObjectsFound > 0) {
        distance = MultiRangingData.RangeData[0].RangeMilliMeter;
      }
      sensor_vl53l4cx_sat.VL53L4CX_ClearInterruptAndStartMeasurement();
    }

    float lat = GPS.latitudeDegrees;
    float lon = GPS.longitudeDegrees;
    float alt = GPS.altitude;

    // 2. 发送 WiFi
    String json = "{";
    json += "\"temp\":" + String(temp, 2) + ",";
    json += "\"hum\":" + String(hum, 2) + ",";
    json += "\"press\":" + String(press, 2) + ",";
    json += "\"gas\":" + String(gas, 2) + ",";
    json += "\"dist\":" + String(distance) + ",";
    json += "\"lat\":" + String(lat, 6) + ",";
    json += "\"lon\":" + String(lon, 6) + ",";
    json += "\"alt\":" + String(alt, 2);
    json += "}";
    events.send(json.c_str(), "update", millis());
    Serial.println("📤 Sent: " + json);

    // 🟢 3. 写入 SD 卡
    File file = SD.open(logFileName, FILE_APPEND);
    if (file) {
      file.print(millis()); file.print(",");
      file.print(temp); file.print(",");
      file.print(hum); file.print(",");
      file.print(press); file.print(",");
      file.print(gas); file.print(",");
      file.print(distance); file.print(",");
      file.print(lat, 6); file.print(",");
      file.print(lon, 6); file.print(",");
      file.println(alt);
      file.close();
      Serial.println("💾 SD Saved.");
    } else {
        // 如果 SD 卡没插或者坏了，这里会提示，但不影响 WiFi
        Serial.println("⚠️ SD Write Fail (No Card?)");
    }

    // 4. OLED 显示 (带 IP)
    display.clearDisplay();
    display.setCursor(0, 0);
    display.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    display.printf("T:%.1f H:%.0f%%\n", temp, hum);
    display.printf("D:%dmm SD:%s\n", distance, SD.begin(SD_CS_PIN)?"OK":"X"); // 状态栏显示 SD 状态
    if (lat != 0.0) {
       display.printf("Lat:%.4f\n", lat);
    } else {
       display.println("GPS Searching...");
    }
    display.display();
  }
}