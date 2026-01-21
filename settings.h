#ifndef SETTINGS_H
#define SETTINGS_H

#pragma once
#include <vector>
#include <IPAddress.h>
#include <WString.h>
#include <ArduinoJson.h>
#include <FS.h>

struct PumpControlSettings {
  float minTrigger;
  float maxTrigger;
  float currentDistance;
  int pin_pump;
  int pin_led;
  int pin_button;
  int pin_echo;
  int pin_trig;
  bool manualMode_pump;
};

struct NetworkSetting {
  String ssid;
  String password;
  bool useStaticIP;
  IPAddress staticIP;
  IPAddress staticGateway;
  IPAddress staticSubnet;
  IPAddress staticDNS;
};

struct DeviceSettings {

  PumpControlSettings control;

  bool isWifiTurnedOn = true;
  std::vector<NetworkSetting> networkSettings;

  bool isAP = true;
  String ssidAP = "WaterPump-AP";
  String passwordAP = "";
  IPAddress staticIpAP = IPAddress(192, 168, 4, 1);

  String mDNS = "waterpump";
  bool autoReconnect = true;
  int8_t timeZone = 3;

  bool isSaveRequested = false;
  bool isRebootRequested = false;
};

class SettingsManager {
public:
    SettingsManager();

    void begin();
    bool loadSettings();
    bool saveSettings();
    void loadDefaults();

    bool isFSMounted();
    void printFsInfo();
    void formatFS();

    DeviceSettings settings;

    String serializeSettings(const DeviceSettings& settings);
    bool deserializeSettings(JsonObject doc, DeviceSettings& settings);

private:
    bool spiffsMounted = false;
};

#endif
