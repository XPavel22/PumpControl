#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Updater.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "settings.h"
#include "control.h"

class WebServerManager {
public:
    WebServerManager(SettingsManager& settingsManager, ControlManager& controlManager);
    void begin();
    void loop() {}

private:
    AsyncWebServer server;
    SettingsManager& _settingsManager;
    ControlManager& _controlManager;

    File _uploadFile;

    void _handleGetAllSettings(AsyncWebServerRequest *request);
    void _handleGetLiveData(AsyncWebServerRequest *request);
    void _handleSaveSettings(AsyncWebServerRequest *request);
    void _handleSetPump(AsyncWebServerRequest *request);
    void _handleResetManualMode(AsyncWebServerRequest *request);
    void _handleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);

    AsyncWebServerResponse* _getIndexResponse(AsyncWebServerRequest *request);
    String _getHTTPDate(time_t timestamp);
};

#endif
