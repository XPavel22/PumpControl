/*
  Форматирование prettier --print-width 120 --html-whitespace-sensitivity css --write compressed.html
  Cжатие gzip -k -9 index.html
  Перевод .gz в .h - % xxd -i index.html.gz > index_html_gz.h

  #ifndef INDEX_HTML_GZ_H
  #define INDEX_HTML_GZ_H

  #include <stdint.h>

  static const uint8_t index_html_gz[] PROGMEM = {
  ... 0x00
  };

  static const unsigned int index_html_gz_len = sizeof(index_html_gz);

  #endif
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "settings.h"
#include "wifimanager.h"
#include "control.h"
#include "webserver.h"

SettingsManager settingsManager;
WiFiManager wifiManager(settingsManager);
ControlManager controlManager(settingsManager);
WebServerManager webServer(settingsManager, controlManager); // <-- Создали объект сервера

void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting...");

  settingsManager.begin();

  delay(200);
  
  controlManager.begin();

  delay(200);
  if (digitalRead(settingsManager.settings.control.pin_button) == LOW ) {
    settingsManager.settings.isWifiTurnedOn = false;
  } else {
    settingsManager.settings.isWifiTurnedOn = true;
  }

  delay(500);
  wifiManager.begin();
  delay(200);
  webServer.begin();
}

void loop() {
  wifiManager.loop();

  controlManager.update();

  if (settingsManager.settings.isSaveRequested) {
    Serial.println("Main: Save request detected. Saving settings to SPIFFS...");

    bool needsReboot = settingsManager.settings.isRebootRequested;

    if (settingsManager.saveSettings()) {
      Serial.println("Main: Settings saved successfully.");
    } else {
      Serial.println("Main: ERROR: Failed to save settings to SPIFFS!");
    }

    settingsManager.settings.isSaveRequested = false;
    settingsManager.settings.isRebootRequested = false;

    if (needsReboot) {
      Serial.println("Main: Reboot requested by web server. Restarting...");
      delay(100);
      ESP.restart();
    }
  }

}
