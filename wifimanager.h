#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "settings.h"

class WiFiManager {
public:

    WiFiManager(SettingsManager& settingsManager);

    void begin();
    void loop();

    void startAccessPoint();
    void connectToWiFi();

    bool isConnected() const;
    String getStatusString() const;

private:
    SettingsManager& _settingsManager;

    bool _isConnecting = false;
    unsigned long _connectionStartTime = 0;
    const unsigned long _connectionTimeout = 15000;

    bool _isInFallbackAP = false;
    unsigned long _apStartTime = 0;
    const unsigned long _apTimeout = 300000;

    void _setupStationMode(const NetworkSetting& net);
    void _setupAccessPointMode();
};

#endif
