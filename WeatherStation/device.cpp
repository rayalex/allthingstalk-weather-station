#include "device.h"
#include <ArduinoJson.h>

Device::Device(ESP &esp, MQTT &mqtt, REST &rest, DeviceInfo device):
  _esp(esp), _mqtt(mqtt), _rest(rest),
  _deviceId(device.deviceId),
  _clientId(device.clientId),
  _clientKey(device.clientKey) { }

void Device::connect() {

  // connect to MQTT broker
  char user[32];
  sprintf(user, "%s:%s", _clientId, _clientId);
  LOG("Setting up MQTT client...");
  if (!_mqtt.begin("WeatherStation", user, _clientKey, 30, 1)) {
    LOG("Unable to set up MQTT client.");
    while (1);
  }

  // setup rest
  _rest.begin(API_HOST);
  _rest.setContentType("application/json");

  /*setup mqtt events */
  _mqtt.connectedCb.attach(this, &Device::mqttConnected);
  _mqtt.disconnectedCb.attach(this, &Device::mqttDisconnected);
  _mqtt.publishedCb.attach(this, &Device::mqttPublished);
  _mqtt.dataCb.attach(this, &Device::mqttData);

  /*setup wifi*/
  LOG("Setting up WIFI...");
  _esp.wifiCb.attach(this, &Device::wifiCb);
}

void Device::addAsset(const char *name, const AssetType type, const char *profileType) const {
  StaticJsonBuffer<128> jsonBuffer;
  JsonObject &profile = jsonBuffer.createObject();
  JsonObject &root = jsonBuffer.createObject();

  root["title"] = name;
  root["is"] = assetTypeToString(type);
  profile["type"] = profileType;
  root["profile"] = profile;

  char url[64];
  char header[128];
  char body[128];

  root.printTo(body, sizeof(body));
  int contentLength = strlen(body);

  // We're authorizing using Auth-ClientId and Auth-ClientKey headers on /device/:id/asset/:name URI
  sprintf(header, "Auth-ClientId: %s\r\nAuth-ClientKey: %s\r\nContent-Length: %d\r\n", _clientId, _clientKey, contentLength);
  sprintf(url, "/device/%s/asset/%s", _deviceId, name);

  char resp[1024];
  _rest.setHeader(header);
  _rest.put(url, body);

  // dump response for debugging purposes, you can turn this off
  _rest.getResponse(resp, sizeof(resp));
  LOG(resp);
}

void Device::send(const char *name, const char *value) const {
  char data[32];
  char topic[128];

  // payload format is timestamp|data. We can use 0 here, in that case
  // state update will be marked with server's time
  sprintf(data, "0|%s", value);

  // publish state via MQTT
  sprintf(topic, "client.%s.out.device.%s.asset.%s.state", _clientId, _deviceId, name);

  _mqtt.publish(topic, data);
}

void Device::mqttConnected(void *response) {
  char topic[128];
  sprintf(topic, "client.%s.in.device.%s.asset.*.command", _clientId, _deviceId);
  LOG("Connected");
  _mqtt.subscribe(topic);
}

void Device::mqttDisconnected(void*) {
  LOG("MQTT Disconnected");
}

void Device::mqttPublished(void*) {}

void Device::mqttData(void* response) {
  RESPONSE res(response);

  LOG("Received: topic=");
  String topic = res.popString();
  LOG(topic.c_str());

  LOG("data=");
  String data = res.popString();
  LOG(data.c_str());

  uint8_t start = topic.indexOf("asset/") + 6;
  uint8_t end = topic.lastIndexOf("/command");

  String name = topic.substring(start, end);

  Command cmd(name, data);

  // echo back command to platform to confirm the receipt
  if (_autoEcho)
    send(name.c_str(), data.c_str());

  // invoke user callback, if set
  invokeCommand(cmd);
}

void Device::wifiCb(void* response) {
  uint32_t status;
  RESPONSE res(response);

  if (res.getArgc() == 1) {
    res.popArgs((uint8_t*)&status, 4);
    if (status == STATION_GOT_IP) {
      LOG("WIFI CONNECTED");

      // callback to program to allow it
      // to handle it's tasks on connection
      invokeConnect();

      _mqtt.connect(MQTT_BROKER, 1883);
      _wifiConnected = true;
    } else {
      _wifiConnected = false;
      _mqtt.disconnect();
    }
  }
}

void Device::invokeCommand(const Command command) const {
  if (_hCommand) _hCommand(command);
}

void Device::invokeConnect() const {
  if (_hConnect) _hConnect();
}

void Device::invokeLogging(const char* message) const {
  if (_hLogging) _hLogging(message);
}

