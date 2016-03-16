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

    /**
     * Connect to ATT platform.
     */
    void connect();

    /**
     * Sends value state for the asset.
     * @param name  Name of the asset, local to this device.
     * @param value State value.
     */
    void send(const char *name, const char *value) const;

    /**
     * Adds asset to the device.
     * @param name        Name of the asset.
     * @param type        Asset type, either 'sensor' or 'actuator'
     * @param profileType Type of the profile, accepts any JSON Schema type.
     */
    void addAsset(const char *name, const AssetType type, const char *profileType) const;

    /**
     * Triggers the inner processing for TCP stack. Call as often as possible.
     */
    void process() {
      _esp.process();
    }

    /**
     * Indicates if the connection to WiFi has been established.
     * @return `true` if connection exists.
     */
    inline bool isConnected() const {
      return _wifiConnected;
    }

    /**
     * If set, device will automatically confirm command values
     * by echoing them back to platform.
     * @param  echo Auto echo flashing
     */
    inline void setAutoEcho(bool echo) {
      _autoEcho = echo;
    }

    /**
     * Sets the handler which is invoked when command is
     * sent to the device.
     * @param handler Handler
     */
    void setCommandHandler(CommandHandler handler) {
      _hCommand = handler;
    }

    /**
     * Sets the handler which is invoked when connection
     * has been established.
     * @param handler Handler
     */
    void setConnectHandler(ConnectHandler handler) {
      _hConnect = handler;
    }

    /**
     * Sets the handler which is invoked on debug
     * data logging.
     * @param handler Handler
     */
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
