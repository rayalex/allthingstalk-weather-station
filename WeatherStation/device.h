#ifndef DEVICE_H
#define DEVICE_H

#include <Arduino.h>
#include <espduino.h>
#include <mqtt.h>
#include <rest.h>

#define MQTT_BROKER "broker.smartliving.io"
#define API_HOST "api.smartliving.io"

#define LOG(x) (invokeLogging(x))

enum AssetType {
  sensor,
  actuator
};

struct DeviceInfo {
  const char* deviceId;
  const char* clientId;
  const char* clientKey;
};

struct Command {
  Command(String name, String value):
    name(name), value(value) {}

  String name;
  String value;
};

typedef void (*CommandHandler) (Command);
typedef void (*ConnectHandler) (void);
typedef void (*LoggingHandler) (const char*);

class Device {
  public:
    Device(ESP &esp, MQTT &mqtt, REST &rest, DeviceInfo device);

    void connect();
    void send(const char *name, const char *value) const;
    void addAsset(const char *name, const AssetType type, const char *profileType) const;

    void process() {
      _esp.process();
    }

    inline bool isConnected() const {
      return _wifiConnected;
    }

    inline bool setAutoEcho(bool echo) {
      _autoEcho = echo;
    }

    void setCommandHandler(CommandHandler handler) {
      _hCommand = handler;
    }

    void setConnectHandler(ConnectHandler handler) {
      _hConnect = handler;
    }

    void setLoggingHandler(LoggingHandler handler) {
      _hLogging = handler;
    }

  private:
    void mqttConnected(void*);
    void mqttDisconnected(void*);
    void mqttPublished(void*);
    void mqttData(void*);
    void wifiCb(void*);

    void invokeCommand(Command) const;
    void invokeConnect() const;
    void invokeLogging(const char*) const;

    const char* assetTypeToString(uint8_t assetType) const {
      return (const char *[]) {
        "sensor",
        "actuator"
      }[assetType];
    }

    CommandHandler _hCommand;
    ConnectHandler _hConnect;
    LoggingHandler _hLogging;

    Stream *_debug;
    ESP &_esp;
    REST &_rest;
    MQTT &_mqtt;

    const char *_deviceId;
    const char *_clientId;
    const char *_clientKey;

    bool _wifiConnected = false;
    bool _autoEcho = false;
};

#endif

