#include "webserver.h"
#include <time.h>

 #include "index_html_gz.h"

WebServerManager::WebServerManager(SettingsManager& settingsManager, ControlManager& controlManager)
    : server(80), _settingsManager(settingsManager), _controlManager(controlManager) {}

void WebServerManager::begin() {

   if (!_settingsManager.settings.isWifiTurnedOn) {
        Serial.println("WEB SERVER OFF, WiFi is turned off in settings.");
        return;
    }

    Serial.println("WebServer: Starting server...");

    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(_getIndexResponse(request));
    });

    server.onNotFound([this](AsyncWebServerRequest *request) {
        request->send(_getIndexResponse(request));
    });

    server.on("/getAllSettings", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->_handleGetAllSettings(request);
    });

    server.on("/getLiveData", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->_handleGetLiveData(request);
    });

    server.on("/saveSettings", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->_handleSaveSettings(request);
    });

    server.on("/setPump", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->_handleSetPump(request);
    });

    server.on("/resetManualMode", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->_handleResetManualMode(request);
    });

    server.on("/uploadFile", HTTP_POST, [](AsyncWebServerRequest *request) {
            request->send(200);
        },
        [this](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
            this->_handleFileUpload(request, filename, index, data, len, final);
        });

    server.begin();
    Serial.println("WebServer: Server started on port 80.");
}

void WebServerManager::_handleResetManualMode(AsyncWebServerRequest *request) {
    Serial.println("WebServer: Received request to reset manual mode.");

    _controlManager.setManualMode(false);

    request->send(200, "application/json", "{\"status\":\"success\"}");
}

void WebServerManager::_handleGetAllSettings(AsyncWebServerRequest *request) {
    Serial.println("WebServer: Received request for /getAllSettings");
    String jsonString = _settingsManager.serializeSettings(_settingsManager.settings);
    request->send(200, "application/json", jsonString);
}

void WebServerManager::_handleGetLiveData(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(256);
    doc["currentDistance"] = _controlManager.getCurrentDistance();
    doc["pumpState"] = _controlManager.getPumpState();
    doc["isErrorState"] = _controlManager.isErrorState();
    doc["manualMode_pump"] = _settingsManager.settings.control.manualMode_pump;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::_handleSaveSettings(AsyncWebServerRequest *request) {
    Serial.println("\n--- WebServer: Received POST to /saveSettings ---");

    if (!request->hasParam("dataSettings", true)) {
        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing 'dataSettings' parameter\"}");
        return;
    }

    String body = request->getParam("dataSettings", true)->value();
    if (body.isEmpty()) {
        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Empty 'dataSettings' parameter\"}");
        return;
    }

    StaticJsonDocument<4096> newDoc;
    DeserializationError error = deserializeJson(newDoc, body);

    if (error) {
        String errorMsg = "Failed to parse JSON: " + String(error.c_str());
        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"" + errorMsg + "\"}");
        return;
    }

    Serial.println("JSON parsed successfully.");

    bool needsReboot = false;

    if (_settingsManager.settings.isWifiTurnedOn != newDoc["isWifiTurnedOn"].as<bool>()) needsReboot = true;
    if (_settingsManager.settings.isAP != newDoc["isAP"].as<bool>()) needsReboot = true;

    if (_settingsManager.settings.ssidAP != newDoc["ssidAP"].as<String>()) needsReboot = true;
    if (_settingsManager.settings.passwordAP != newDoc["passwordAP"].as<String>()) needsReboot = true;

    if (_settingsManager.settings.mDNS != newDoc["mDNS"].as<String>()) needsReboot = true;

if (newDoc.containsKey("networkSettings") && newDoc["networkSettings"].is<JsonArray>() && newDoc["networkSettings"].size() > 0) {
    JsonObject newNet = newDoc["networkSettings"][0];
    NetworkSetting& currentNet = _settingsManager.settings.networkSettings[0];

    IPAddress newIP, newGateway, newSubnet;
    newIP.fromString(newNet["staticIP"].as<String>());
    newGateway.fromString(newNet["staticGateway"].as<String>());
    newSubnet.fromString(newNet["staticSubnet"].as<String>());

    if (currentNet.ssid != newNet["ssid"].as<String>()) needsReboot = true;
    if (currentNet.password != newNet["password"].as<String>()) needsReboot = true;
    if (currentNet.useStaticIP != newNet["useStaticIP"].as<bool>()) needsReboot = true;

    if (currentNet.staticIP != newIP) needsReboot = true;
    if (currentNet.staticGateway != newGateway) needsReboot = true;
    if (currentNet.staticSubnet != newSubnet) needsReboot = true;
}

    Serial.println("Applying settings from received JSON...");
    if (_settingsManager.deserializeSettings(newDoc.as<JsonObject>(), _settingsManager.settings)) {
        Serial.println("Settings applied successfully.");

        _settingsManager.settings.isSaveRequested = true;
        _settingsManager.settings.isRebootRequested = needsReboot;

        String responseMessage = needsReboot ? "Settings received. Saving and rebooting..." : "Settings received. Saving...";
        request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"" + responseMessage + "\"}");

        Serial.printf("Flags set: Save=%d, Reboot=%d. Waiting for main loop.\n", _settingsManager.settings.isSaveRequested, _settingsManager.settings.isRebootRequested);

    } else {
        request->send(500, "application/json", "{\"status\":\"error\", \"message\":\"Failed to apply settings\"}");
    }
    Serial.println("--- End of /saveSettings request ---\n");
}

void WebServerManager::_handleSetPump(AsyncWebServerRequest *request) {
    if (request->hasParam("state")) {
        String state = request->getParam("state")->value();
        _controlManager.setManualMode(true);
        if (state == "on") {
            _controlManager.setPumpState(true);
        } else if (state == "off") {
            _controlManager.setPumpState(false);

        } else {
            request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid state value\"}");
            return;
        }
        request->send(200, "application/json", "{\"status\":\"success\"}");
    } else {
        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing 'state' parameter\"}");
    }
}

AsyncWebServerResponse* WebServerManager::_getIndexResponse(AsyncWebServerRequest *request) {
    bool hasHtml = SPIFFS.exists("/index.html");
    bool hasGz = SPIFFS.exists("/index.html.gz");
    time_t htmlTime = 0, gzTime = 0;
    time_t now = time(nullptr);

    if (hasHtml) {
        File htmlFile = SPIFFS.open("/index.html", "r");
        if (htmlFile) {
            htmlTime = htmlFile.getLastWrite();
            htmlFile.close();
        }
    }
    if (hasGz) {
        File gzFile = SPIFFS.open("/index.html.gz", "r");
        if (gzFile) {
            gzTime = gzFile.getLastWrite();
            gzFile.close();
        }
    }

    AsyncWebServerResponse *response = nullptr;
    String selectedFile = "";
    String reason = "";

    if (hasHtml && (!hasGz || htmlTime > gzTime)) {
        response = request->beginResponse(SPIFFS, "/index.html", "text/html");
        selectedFile = "/index.html";
        reason = (!hasGz) ? "GZ file not exists" : "HTML is newer";
    } else if (hasGz) {
        response = request->beginResponse(SPIFFS, "/index.html.gz", "text/html");
        response->addHeader("Content-Encoding", "gzip");
        selectedFile = "/index.html.gz";
        reason = (!hasHtml) ? "HTML file not exists" : "GZ is newer or equal";
    } else {

        Serial.println("[WebServer] No files in SPIFFS, using embedded version.");
        response = request->beginResponse_P(200, "text/html", index_html_gz, index_html_gz_len);
        response->addHeader("Content-Encoding", "gzip");
        selectedFile = "EMBEDDED";
        reason = "No files in SPIFFS";
    }

    Serial.printf("[WebServer] Serving: %s, Reason: %s\n", selectedFile.c_str(), reason.c_str());

    if (response) {
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
    }

    return response;
}

String WebServerManager::_getHTTPDate(time_t timestamp) {
    struct tm *timeinfo = gmtime(&timestamp);
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
    return String(buffer);
}

void WebServerManager::_handleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    static bool isFirmwareUpdate = false;
    static bool updateFailed = false;
    static size_t totalSize = 0;

    String extension = filename.substring(filename.lastIndexOf('.') + 1);

    if (index == 0) {
        Serial.printf("Upload start: %s\n", filename.c_str());
        totalSize = 0;
        updateFailed = false;

        if (extension == "bin") {
            isFirmwareUpdate = true;
            Update.runAsync(true);

            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(maxSketchSpace, U_FLASH)) {
                Update.printError(Serial);
                request->send(500, "text/plain", "OTA Begin Failed");
                updateFailed = true;
                return;
            }
            Serial.println("Firmware update started...");
        } else {
            isFirmwareUpdate = false;
            String path = "/" + filename;
            _uploadFile = SPIFFS.open(path, "w");
            if (!_uploadFile) {
                Serial.printf("Failed to open file %s for writing\n", path.c_str());
                request->send(500, "text/plain", "File open error");
                updateFailed = true;
                return;
            }
            Serial.printf("File upload started: %s\n", path.c_str());
        }
    }

    if (len > 0 && !updateFailed) {
        totalSize += len;
        if (isFirmwareUpdate) {
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
                request->send(500, "text/plain", "OTA Write Error");
                updateFailed = true;
                return;
            }
        } else {
            if (_uploadFile) {
                _uploadFile.write(data, len);
            }
        }
    }

    if (final) {
        if (updateFailed) {

            return;
        }

        if (isFirmwareUpdate) {
            if (Update.end(true)) {
                Serial.println("Update Complete. Rebooting...");
                request->send(200, "text/plain", "OTA Complete");

                delay(300);
                _settingsManager.settings.isRebootRequested = true;
            } else {
                Update.printError(Serial);
                request->send(500, "text/plain", "OTA End Failed");
            }
        } else {
            if (_uploadFile) {
                _uploadFile.close();
                Serial.printf("File %s upload complete\n", filename.c_str());
                request->send(200, "text/plain", "File Uploaded Successfully");
            }
        }
    }
}
