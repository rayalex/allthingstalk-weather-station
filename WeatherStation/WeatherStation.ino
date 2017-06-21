#include <espduino.h>
#include <mqtt.h>
#include <rest.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "credentials.h"
#include "device.h"

#define LED_A 38
#define LED_C 39

const uint8_t BME_ADDR = 0x76;
const float   SEALEVELPRESSURE_HPA = 1013.25;

const char*   S_TMP = "Temperature";
const char*   S_ALT = "Altitude";
const char*   S_HUM = "Humidity";
const char*   S_PSA = "Pressure";
const char*   A_LED = "LED";

// Uses Due's Serial1 as data port. Change according to your platform
HardwareSerial *data  = &Serial1;
HardwareSerial *debug = &Serial;

// ESP stuff
ESP esp(data, debug, 4);
REST rest(&esp);
MQTT mqtt(&esp);

// SmartLiving stuff
DeviceInfo info = {DEVICE_ID, CLIENT_ID, CLIENT_KEY};
Device device(esp, mqtt, rest, info);

// Sensors
Adafruit_BME280 bme;  // on I2C

void log(const char*);

void setupAssets() {
  log("Setting up device assets...");
  device.addAsset(S_TMP, AssetType::sensor, "number");
  device.addAsset(S_PSA, AssetType::sensor, "number");
  device.addAsset(S_HUM, AssetType::sensor, "number");
  device.addAsset(S_ALT, AssetType::sensor, "number");
  device.addAsset(A_LED, AssetType::actuator, "boolean");
}

void deviceConnected() {
  setupAssets();
}

void onCommand(Command command) {
  if (command.name == A_LED) {
    digitalWrite(LED_A, command.value == "true");
  }
}

void setup() {
  pinMode(LED_A, OUTPUT);
  pinMode(LED_C, OUTPUT);

  data->begin(19200);
  debug->begin(19200);

  // setup sensor
  if (!bme.begin(BME_ADDR)) {
    log("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  esp.enable();
  delay(500);

  esp.reset();
  delay(500);
  while (!esp.ready());

  // setup ATT device
  device.setAutoEcho(true);
  device.setLoggingHandler(&log);
  device.setConnectHandler(&deviceConnected);
  device.setCommandHandler(&onCommand);
  device.connect();

  esp.wifiConnect(WIFI_SSID, WIFI_PASS);
  log("Sketch started.");
}

String toString(float value) {
  char buffer[12];
  sprintf(buffer, "%.2f", value);
  return String(buffer);
}

void sense() {
  digitalWrite(LED_C, HIGH);

  float temperature = bme.readTemperature();
  float pressure = bme.readPressure() / 100.0F;
  float altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
  float humidity = bme.readHumidity();

  device.send(S_TMP, toString(temperature).c_str());
  device.send(S_PSA, toString(pressure).c_str());
  device.send(S_HUM, toString(humidity).c_str());
  device.send(S_ALT, toString(altitude).c_str());

  digitalWrite(LED_C, LOW);
}

void loop() {
  static uint32_t refresh = 0;

  device.process();
  if (device.isConnected()) {
    if (refresh++ > 500) {
      log("Update.");
      sense();
      refresh = 0;
    }
    delay(10);
  }
}

void log(const char* message) {
  debug->println(message);
}
