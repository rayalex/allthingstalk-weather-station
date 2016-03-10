#include <ArduinoJson.h>
#include <espduino.h>
#include <mqtt.h>
#include <rest.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "credentials.h"

#define BME_ADDR 0x76
#define SEALEVELPRESSURE_HPA (1013.25)

#define LED_A 38
#define LED_C 39

#define S_TMP "Temperature"
#define S_ALT "Altitude"
#define S_HUM "Humidity"
#define S_PSA "Pressure"
#define A_LED "LED"

// SmartLiving endpoints
#define MQTT_BROKER "broker.smartliving.io"
#define API_HOST "api.smartliving.io"

// Uses Due's Serial1 as data port. Change according to your platform
HardwareSerial *data  = &Serial1;
HardwareSerial *debug = &Serial;

// ESP stuff
ESP esp(data, debug, 4);
REST rest(&esp);
MQTT mqtt(&esp);

// Sensors
Adafruit_BME280 bme;  // on I2C

boolean wifiConnected = false;

/*
  Adds single asset, by it's name.
*/
void addAsset(String name, String type, String dataType)
{
  StaticJsonBuffer<128> jsonBuffer;
  JsonObject &profile = jsonBuffer.createObject();
  JsonObject &root = jsonBuffer.createObject();

  root["title"] = name;
  root["is"] = type;
  profile["type"] = dataType;
  root["profile"] = profile;

  char url[64];
  char header[128];
  char body[128];

  root.printTo(body, sizeof(body));
  int contentLength = strlen(body);

  // We're authorizing using Auth-ClientId and Auth-ClientKey headers on /device/:id/asset/:name URI
  sprintf(header, "Auth-ClientId: %s\r\nAuth-ClientKey: %s\r\nContent-Length: %d\r\n", CLIENT_ID, CLIENT_KEY, contentLength);
  sprintf(url, "/device/%s/asset/%s", DEVICE_ID, name.c_str());

  char resp[1024];
  rest.setHeader(header);
  rest.put(url, body);

  // dump response for debugging purposes, you can turn this off
  rest.getResponse(resp, sizeof(resp));
  debug->println(resp);

  delay(100);
}

void sendState(String name, float value) {
  char data[32];
  char topic[128];

  // payload format is timestamp|data. We can use 0 here, in that case
  // state update will be marked with server's time
  sprintf(data, "0|%.2f", value);

  // publish state via MQTT
  sprintf(topic, "client.%s.out.device.%s.asset.%s.state", CLIENT_ID, DEVICE_ID, name.c_str());

  mqtt.publish(topic, data);
}

void setupAssets() {
  rest.begin(API_HOST);
  rest.setContentType("application/json");

  addAsset(S_TMP, "sensor", "number");
  addAsset(S_PSA, "sensor", "number");
  addAsset(S_HUM, "sensor", "number");
  addAsset(S_ALT, "sensor", "number");
  addAsset(A_LED, "actuator", "boolean");
}

void wifiCb(void* response)
{
  uint32_t status;
  RESPONSE res(response);

  if (res.getArgc() == 1) {
    res.popArgs((uint8_t*)&status, 4);
    if (status == STATION_GOT_IP) {
      debug->println("WIFI CONNECTED");

      setupAssets();
      debug->println("Assets created");

      mqtt.connect(MQTT_BROKER, 1883);
      wifiConnected = true;
    } else {
      wifiConnected = false;
      mqtt.disconnect();
    }

  }
}

void mqttConnected(void* response)
{
  char topic[128];
  sprintf(topic, "client.%s.in.device.%s.asset.*.command", CLIENT_ID, DEVICE_ID);
  debug->println("Connected");
  mqtt.subscribe(topic);
}

void mqttDisconnected(void* response)
{
  debug->println("Disconnected");
}

void mqttData(void* response)
{
  RESPONSE res(response);

  debug->print("Received: topic=");
  String topic = res.popString();
  debug->println(topic);

  debug->print("data=");
  String data = res.popString();
  debug->println(data);

  digitalWrite(LED_A, data == "true");
}
void mqttPublished(void* response)
{
  // debug->println("mqtt published");
}
void setup() {
  pinMode(LED_A, OUTPUT);
  pinMode(LED_C, OUTPUT);

  data->begin(19200);
  debug->begin(19200);

  // setup sensor
  if (!bme.begin(BME_ADDR)) {
    debug->println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  esp.enable();
  delay(500);
  esp.reset();
  delay(500);
  while (!esp.ready());

  char user[32];
  sprintf(user, "%s:%s", CLIENT_ID, CLIENT_ID);
  debug->println("ARDUINO: setup mqtt client");
  if (!mqtt.begin("WeatherStation", user, CLIENT_KEY, 30, 1)) {
    debug->println("ARDUINO: fail to setup mqtt");
    while (1);
  }

  /*setup mqtt events */
  mqtt.connectedCb.attach(&mqttConnected);
  mqtt.disconnectedCb.attach(&mqttDisconnected);
  mqtt.publishedCb.attach(&mqttPublished);
  mqtt.dataCb.attach(&mqttData);

  /*setup wifi*/
  debug->println("ARDUINO: setup wifi");
  esp.wifiCb.attach(&wifiCb);

  esp.wifiConnect(WIFI_SSID, WIFI_PASS);
  debug->println("ARDUINO: system started");
}

void sense() {
  digitalWrite(LED_C, HIGH);

  float temperature = bme.readTemperature();
  float pressure = bme.readPressure() / 100.0F;
  float altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
  float humidity = bme.readHumidity();
  char buff[24];

  sendState(S_TMP, temperature);
  sendState(S_PSA, pressure);
  sendState(S_HUM, humidity);
  sendState(S_ALT, altitude);

  digitalWrite(LED_C, LOW);
}

void loop() {
  static uint32_t refresh = 0;
  esp.process();
  if (wifiConnected) {
    if (refresh++ > 500) {
      debug->println("Refreshing...");
      sense();
      refresh = 0;
    }
    delay(10);
  }
}
