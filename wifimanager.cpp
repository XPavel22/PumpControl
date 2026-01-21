#include "wifimanager.h"

WiFiManager::WiFiManager(SettingsManager& settingsManager) : _settingsManager(settingsManager) {}

void WiFiManager::begin() {
    if (!_settingsManager.settings.isWifiTurnedOn) {
        Serial.println("WiFi is turned off in settings.");
        return;
    }

     WiFi.hostname(_settingsManager.settings.mDNS.c_str());

    if (_settingsManager.settings.isAP) {
        Serial.println("WiFiManager: 'AP Only' mode is enabled. Skipping client connection.");
        startAccessPoint();
        return;
    }

    Serial.println("WiFiManager: Starting Wi-Fi connection process...");
    connectToWiFi();
}

void WiFiManager::loop() {

   MDNS.update();

    if (!_settingsManager.settings.isWifiTurnedOn || _settingsManager.settings.isAP) {
        return;
    }

    if (_isInFallbackAP) {

        if (millis() - _apStartTime > _apTimeout) {
            Serial.println("WiFiManager: Fallback AP timeout expired. Retrying Wi-Fi connection.");
            _isInFallbackAP = false;
            connectToWiFi();
        }
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {

        if (_isConnecting) {
            _isConnecting = false;
            Serial.printf("WiFiManager: Successfully connected to %s\n", WiFi.SSID().c_str());
            Serial.print("WiFiManager: IP Address: ");
            Serial.println(WiFi.localIP());

            if (!MDNS.begin(_settingsManager.settings.mDNS.c_str())) {
                Serial.println("Error setting up MDNS responder!");
            } else {
                MDNS.addService("http", "tcp", 80);
                Serial.printf("MDNS responder started: http://%s.local\n", _settingsManager.settings.mDNS.c_str());
            }
        }
        return;
    }

    if (_isConnecting) {

        if (millis() - _connectionStartTime > _connectionTimeout) {
            Serial.println("WiFiManager: Connection timeout. Starting fallback AP.");
            _isConnecting = false;
            _isInFallbackAP = true;
            _apStartTime = millis();
            startAccessPoint();
        }
    } else {

        Serial.println("WiFiManager: Connection lost. Trying to reconnect...");
        connectToWiFi();
    }
}

void WiFiManager::connectToWiFi() {

    if (_settingsManager.settings.networkSettings.empty()) {
        Serial.println("WiFiManager: No saved networks. Starting fallback AP.");
        _isInFallbackAP = true;
        _apStartTime = millis();
        startAccessPoint();
        return;
    }

    NetworkSetting& net = _settingsManager.settings.networkSettings[0];

    if (net.ssid.isEmpty()) {
        Serial.println("WiFiManager: SSID is empty. Starting fallback AP.");
        _isInFallbackAP = true;
        _apStartTime = millis();
        startAccessPoint();
        return;
    }

    Serial.printf("WiFiManager: Attempting to connect to SSID: %s\n", net.ssid.c_str());

    _setupStationMode(net);
    WiFi.begin(net.ssid.c_str(), net.password.c_str());

    _isConnecting = true;
    _isInFallbackAP = false;
    _connectionStartTime = millis();
}

void WiFiManager::startAccessPoint() {
    Serial.println("WiFiManager: Entering Access Point mode...");
    _setupAccessPointMode();

    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("WiFiManager: Access Point '%s' started.\n", _settingsManager.settings.ssidAP.c_str());
    Serial.print("WiFiManager: AP IP Address: ");
    Serial.println(apIP);

    if (!MDNS.begin(_settingsManager.settings.mDNS.c_str())) {
        Serial.println("Error setting up MDNS responder in AP mode!");
    } else {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("MDNS responder started in AP mode: http://%s.local\n", _settingsManager.settings.mDNS.c_str());
    }
}

bool WiFiManager::isConnected() const {
    return (WiFi.status() == WL_CONNECTED);
}

String WiFiManager::getStatusString() const {
    switch (WiFi.status()) {
        case WL_CONNECTED: return "Connected";
        case WL_NO_SHIELD: return "No WiFi Shield";
        case WL_IDLE_STATUS: return "Idle";
        case WL_NO_SSID_AVAIL: return "No SSID Available";
        case WL_SCAN_COMPLETED: return "Scan Completed";
        case WL_CONNECT_FAILED: return "Connection Failed";
        case WL_CONNECTION_LOST: return "Connection Lost";
        case WL_DISCONNECTED: return "Disconnected";
        default: return "Unknown Status";
    }
}

void WiFiManager::_setupStationMode(const NetworkSetting& net) {
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);

    WiFi.hostname(_settingsManager.settings.mDNS.c_str());

    if (net.useStaticIP) {
        Serial.println("WiFiManager: Using static IP.");
        WiFi.config(net.staticIP, net.staticGateway, net.staticSubnet, net.staticDNS);
    } else {
        Serial.println("WiFiManager: Using DHCP.");
    }
}

void WiFiManager::_setupAccessPointMode() {
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);

    WiFi.softAPConfig(
        _settingsManager.settings.staticIpAP,
        _settingsManager.settings.staticIpAP,
        IPAddress(255, 255, 255, 0)
    );

    if (_settingsManager.settings.passwordAP.isEmpty()) {
        WiFi.softAP(_settingsManager.settings.ssidAP.c_str());
    } else {
        WiFi.softAP(_settingsManager.settings.ssidAP.c_str(), _settingsManager.settings.passwordAP.c_str());
         Serial.print("Password: ");
        Serial.println(_settingsManager.settings.passwordAP.c_str());
    }
}
