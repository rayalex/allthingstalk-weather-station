#ifndef DEVICE_H
#define DEVICE_H

#include <Arduino.h>
#include <espduino.h>
#include <mqtt.h>
#include <rest.h>
#include <fp.h>

#define MQTT_BROKER "broker.smartliving.io"
#define API_HOST "api.smartliving.io"

#define LOG(x) (_debug->println(x)) // TODO: null check

enum AssetType {
  sensor,
  actuator
};

struct DeviceInfo {
  const char* deviceId;
  const char* clientId;
  const char* clientKey;
};

typedef struct Command {
  String name;
  String data;
};

class Device {
  public:
    Device(ESP &esp, MQTT &mqtt, REST &rest, DeviceInfo device, Stream *debug);

    void connect();
    void addAsset(const char *name, const AssetType type, const char *profileType) const;
    void send(const char *name, const char *value) const;

    void process() {
      _esp.process();
    }

    inline bool isConnected() const {
      return _wifiConnected;
    }

    FP<void, void*> deviceConnected;
    FP<void, Command> onCommand;

  private:
    void mqttConnected(void*);
    void mqttDisconnected(void*);
    void mqttPublished(void*);
    void mqttData(void*);
    void wifiCb(void*);

    const char* assetTypeToString(uint8_t assetType) const {
      return (const char *[]) {
        "sensor",
        "actuator"
      }[assetType];
    }

    Stream *_debug;
    ESP &_esp;
    REST &_rest;
    MQTT &_mqtt;

    const char *_deviceId;
    const char *_clientId;
    const char *_clientKey;

    bool _wifiConnected = false;
};

#endif

