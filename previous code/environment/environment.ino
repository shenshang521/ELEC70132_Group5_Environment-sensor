#include <Wire.h>
#include <SPI.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Adafruit_BME680.h>
#include <Adafruit_GPS.h>
#include <vl53l4cx_class.h>

// ---------------------- OLED Configuration ----------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // No RST pin

// ---------------------- BME688 Sensor ----------------------
Adafruit_BME680 bme; // I2C address 0x77

// ---------------------- VL53L4CX ToF Sensor ----------------------
VL53L4CX sensor_vl53l4cx_sat;

// ---------------------- GPS Module ----------------------
Adafruit_GPS GPS(&Wire);
#define GPS_I2C_ADDRESS 0x10
uint32_t timer = millis();

// ---------------------- Timing ----------------------
unsigned long lastMeasurement = 0;
const unsigned long interval = 10000; // 10 seconds

// ---------------------- Setup ----------------------
void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("🌍 Environmental + GPS + ToF Sensor System Starting...");

  // ---------------------- Initialize I2C ----------------------
  Wire.begin();

  // ---------------------- Initialize OLED ----------------------
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    Serial.println("❌ OLED init failed!");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing sensors...");
  display.display();

  // ---------------------- Initialize BME688 ----------------------
  if (!bme.begin()) {
    Serial.println("❌ BME688 init failed!");
    display.println("BME688 fail!");
    display.display();
    while (1);
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  // ---------------------- Initialize ToF (VL53L4CX) ----------------------

  delay(10);
  if (Wire.endTransmission(VL53L4CX_DEFAULT_DEVICE_ADDRESS >> 1) == 0) {
    sensor_vl53l4cx_sat.setI2cDevice(&Wire);
    sensor_vl53l4cx_sat.begin();
    sensor_vl53l4cx_sat.VL53L4CX_Off();
    VL53L4CX_Error error = sensor_vl53l4cx_sat.InitSensor(VL53L4CX_DEFAULT_DEVICE_ADDRESS);
    if (error != VL53L4CX_ERROR_NONE) {
      Serial.print("❌ VL53L4CX init error: ");
      Serial.println(error);
      display.println("VL53L4CX fail!");
      display.display();
      while (1);
    }
    sensor_vl53l4cx_sat.VL53L4CX_StartMeasurement();
    Serial.println("✅ VL53L4CX ToF initialized successfully");
  } else {
    Serial.println("❌ VL53L4CX not found on I2C bus");
    display.println("ToF fail!");
    display.display();
    while (1);
  }

  // ---------------------- Initialize GPS ----------------------
  if (!GPS.begin(GPS_I2C_ADDRESS)) {
    Serial.println("❌ GPS init failed!");
    display.println("GPS fail!");
    display.display();
    while (1);
  }
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("✅ All sensors OK!");
  display.display();
  delay(1000);
}

// ---------------------- Loop ----------------------
void loop() {
  if (millis() - lastMeasurement >= interval) {
    lastMeasurement = millis();

    // ----------- Read BME688 ----------
    bme.performReading();
    float temperature = bme.temperature;
    float humidity = bme.humidity;
    float pressure = bme.pressure / 100.0; // hPa
    float gas = bme.gas_resistance / 1000.0; // kΩ

    // ----------- Read VL53L4CX ----------
    VL53L4CX_MultiRangingData_t MultiRangingData;
    uint8_t NewDataReady = 0;
    int distance = 0;

    sensor_vl53l4cx_sat.VL53L4CX_GetMeasurementDataReady(&NewDataReady);
    if (NewDataReady != 0) {
      sensor_vl53l4cx_sat.VL53L4CX_GetMultiRangingData(&MultiRangingData);
      if (MultiRangingData.NumberOfObjectsFound > 0) {
        distance = MultiRangingData.RangeData[0].RangeMilliMeter;
      }
      sensor_vl53l4cx_sat.VL53L4CX_ClearInterruptAndStartMeasurement();
    }

    // ----------- Read GPS ----------
    GPS.read();
    float latitude = GPS.latitudeDegrees;
    float longitude = GPS.longitudeDegrees;
    float altitude = GPS.altitude;
    float speed = GPS.speed * 0.514444; // knots → m/s

    // ----------- Serial Output ----------
    Serial.println("=====================================");
    Serial.printf("Temp: %.2f °C | Hum: %.2f %%\n", temperature, humidity);
    Serial.printf("Pressure: %.2f hPa | Gas: %.2f kΩ\n", pressure, gas);
    Serial.printf("Distance: %d mm\n", distance);
    Serial.printf("GPS: Lat %.5f, Lon %.5f, Alt %.2f m, Speed %.2f m/s\n",
                  latitude, longitude, altitude, speed);
    Serial.println("=====================================");

    // ----------- OLED Display ----------
    display.clearDisplay();
    display.setCursor(0, 0);
    display.printf("T: %.1fC  H: %.1f%%\n", temperature, humidity);
    display.printf("P: %.1fhPa G: %.1fk\n", pressure, gas);
    display.printf("D: %d mm\n", distance);
    display.printf("Lat: %.2f\nLon: %.2f\n", latitude, longitude);
    display.display();
  }
}
