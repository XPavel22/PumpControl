#include "settings.h"

SettingsManager::SettingsManager() {

}

void SettingsManager::begin() {

  spiffsMounted = SPIFFS.begin();
  if (!spiffsMounted) {
    Serial.println("Failed to mount SPIFFS, trying to format...");
    if (SPIFFS.format()) {
        Serial.println("SPIFFS formatted successfully.");
        spiffsMounted = SPIFFS.begin();
    }
  }

  if (spiffsMounted) {
    Serial.println("SPIFFS mounted successfully.");
    printFsInfo();
    loadSettings();
  } else {
    Serial.println("Failed to mount SPIFFS even after formatting. Using defaults.");
    loadDefaults();
  }
}

bool SettingsManager::isFSMounted() {
    return spiffsMounted;
}

void SettingsManager::printFsInfo() {
  if (!spiffsMounted) {
    Serial.println("SPIFFS is not mounted.");
    return;
  }
  FSInfo fs_info;
  SPIFFS.info(fs_info);
  Serial.printf("SPIFFS Total: %u bytes, Used: %u bytes, Block: %u, Page: %u\n",
                fs_info.totalBytes, fs_info.usedBytes, fs_info.blockSize, fs_info.pageSize);
}

void SettingsManager::formatFS() {
  Serial.println("Formatting SPIFFS and restarting...");
  SPIFFS.end();
  SPIFFS.format();
  ESP.restart();
}

void SettingsManager::loadDefaults() {
  Serial.println("Loading default settings...");

  settings.control.minTrigger = 260.0;
  settings.control.maxTrigger = 290.0;
  settings.control.pin_pump = 16;
  settings.control.pin_led = 5;
  settings.control.pin_button = 4;
  settings.control.pin_echo = 14;
  settings.control.pin_trig = 12;

  settings.isWifiTurnedOn = true;
  settings.networkSettings.clear();
  NetworkSetting defaultNetwork;
  defaultNetwork.ssid = "YourHomeWiFi";
  defaultNetwork.password = "yourpassword";
  defaultNetwork.useStaticIP = false;
  settings.networkSettings.push_back(defaultNetwork);

  settings.isAP = true;
  settings.ssidAP = "WaterPump-AP";
  settings.passwordAP = "";
  settings.staticIpAP = IPAddress(192, 168, 1, 1);

  settings.mDNS = "waterpump";
  settings.autoReconnect = true;
  settings.timeZone = 3;

  Serial.println("Default settings loaded into memory.");
}

bool SettingsManager::saveSettings() {
  if (!spiffsMounted) {
    Serial.println("Cannot save settings, SPIFFS not mounted.");
    return false;
  }

  File file = SPIFFS.open("/settings.json", "w");
  if (!file) {
    Serial.println("Failed to open settings file for writing.");
    return false;
  }

  String jsonString = serializeSettings(settings);
  file.print(jsonString);
  file.close();

  Serial.println("Settings successfully saved to /settings.json");
  return true;
}

bool SettingsManager::loadSettings() {
  if (!spiffsMounted) {
    Serial.println("Cannot load settings, SPIFFS not mounted.");
    loadDefaults();
    return false;
  }

  if (!SPIFFS.exists("/settings.json")) {
    Serial.println("Settings file not found. Loading defaults and saving them.");
    loadDefaults();
    saveSettings();
    return false;
  }

  File file = SPIFFS.open("/settings.json", "r");
  if (!file) {
    Serial.println("Failed to open settings file for reading.");
    return false;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("Failed to parse settings file: %s\n", error.c_str());
    Serial.println("Loading defaults.");
    loadDefaults();
    return false;
  }

  deserializeSettings(doc.as<JsonObject>(), settings);
  Serial.println("Settings successfully loaded from file.");
  return true;
}

String SettingsManager::serializeSettings(const DeviceSettings& settings) {
  DynamicJsonDocument doc(2048);

  JsonObject control = doc.createNestedObject("control");
  control["minTrigger"] = settings.control.minTrigger;
  control["maxTrigger"] = settings.control.maxTrigger;
  control["pin_pump"] = settings.control.pin_pump;
  control["pin_led"] = settings.control.pin_led;
  control["pin_button"] = settings.control.pin_button;
  control["pin_echo"] = settings.control.pin_echo;
  control["pin_trig"] = settings.control.pin_trig;

  doc["isWifiTurnedOn"] = settings.isWifiTurnedOn;
  JsonArray networks = doc.createNestedArray("networkSettings");
  for (const auto& net : settings.networkSettings) {
    JsonObject netObj = networks.createNestedObject();
    netObj["ssid"] = net.ssid;
    netObj["password"] = net.password;
    netObj["useStaticIP"] = net.useStaticIP;
    if (net.useStaticIP) {
      netObj["staticIP"] = net.staticIP.toString();
      netObj["staticGateway"] = net.staticGateway.toString();
      netObj["staticSubnet"] = net.staticSubnet.toString();
      netObj["staticDNS"] = net.staticDNS.toString();
    }
  }

  doc["isAP"] = settings.isAP;
  doc["ssidAP"] = settings.ssidAP;
  doc["passwordAP"] = settings.passwordAP;
  doc["staticIpAP"] = settings.staticIpAP.toString();

  doc["mDNS"] = settings.mDNS;
  doc["autoReconnect"] = settings.autoReconnect;
  doc["timeZone"] = settings.timeZone;

  String output;
  serializeJson(doc, output);
  return output;
}

bool SettingsManager::deserializeSettings(JsonObject doc, DeviceSettings& settings) {

  if (doc.containsKey("control")) {
    JsonObject control = doc["control"];
    settings.control.minTrigger = control["minTrigger"] | 260.0;
    settings.control.maxTrigger = control["maxTrigger"] | 290.0;
    settings.control.pin_pump = control["pin_pump"] | 15;
    settings.control.pin_led = control["pin_led"] | 0;
    settings.control.pin_button = control["pin_button"] | 2;
    settings.control.pin_echo = control["pin_echo"] | 13;
    settings.control.pin_trig = control["pin_trig"] | 12;
  }

  settings.isWifiTurnedOn = doc["isWifiTurnedOn"] | true;
  settings.networkSettings.clear();
  if (doc.containsKey("networkSettings")) {
    JsonArray networkArray = doc["networkSettings"];
    for (JsonObject net : networkArray) {
      NetworkSetting ns;
      ns.ssid = net["ssid"] | "";
      ns.password = net["password"] | "";
      ns.useStaticIP = net["useStaticIP"] | false;
      if (ns.useStaticIP) {
        ns.staticIP.fromString(net["staticIP"] | "0.0.0.0");
        ns.staticGateway.fromString(net["staticGateway"] | "0.0.0.0");
        ns.staticSubnet.fromString(net["staticSubnet"] | "255.255.255.0");
        ns.staticDNS.fromString(net["staticDNS"] | "8.8.8.8");
      }
      settings.networkSettings.push_back(ns);
    }
  }

  settings.isAP = doc["isAP"] | true;
  settings.ssidAP = doc["ssidAP"] | "WaterPump-AP";
  settings.passwordAP = doc["passwordAP"] | "";
  settings.staticIpAP.fromString(doc["staticIpAP"] | "192.168.4.1");

  settings.mDNS = doc["mDNS"] | "waterpump";
  settings.autoReconnect = doc["autoReconnect"] | true;
  settings.timeZone = doc["timeZone"] | 3;

  return true;
}
