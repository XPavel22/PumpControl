/*
  # Форматируем без изменения структуры
  prettier --print-width 120 --html-whitespace-sensitivity css --write compressed.html
  Далее сжатие gzip -k -9 index.html
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

// Подключаем наши модули
#include "settings.h"
#include "wifimanager.h"
#include "control.h"
#include "webserver.h"

// Глобальные объекты
SettingsManager settingsManager;
WiFiManager wifiManager(settingsManager);
ControlManager controlManager(settingsManager);
WebServerManager webServer(settingsManager, controlManager); // <-- Создали объект сервера

void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting...");

  // 1. Инициализируем и загружаем настройки
  settingsManager.begin();

  delay(200);
  // 3. Инициализируем управление насосом и датчиком
  controlManager.begin();

  delay(200);
  if (digitalRead(settingsManager.settings.control.pin_button) == LOW ) {
    settingsManager.settings.isWifiTurnedOn = false;
  } else {
    settingsManager.settings.isWifiTurnedOn = true;
  }

  delay(500);
  // 2. Запускаем менеджер Wi-Fi.
  wifiManager.begin();
  delay(200);
  // 4. Запускаем веб-сервер. Он будет работать независимо от статуса Wi-Fi
  // (в режиме AP он будет доступен по IP 192.168.4.1, в режиме клиента - по его IP)
  webServer.begin();
}

void loop() {
  // 1. Обрабатываем логику Wi-Fi (переподключения, таймауты)
  wifiManager.loop();

  // 2. Обрабатываем логику управления насосом
  controlManager.update();

  // --- НОВЫЙ БЛОК: ОБРАБОТКА ЗАПРОСОВ ОТ ВЕБ-СЕРВЕРА ---
  if (settingsManager.settings.isSaveRequested) {
    Serial.println("Main: Save request detected. Saving settings to SPIFFS...");

    bool needsReboot = settingsManager.settings.isRebootRequested;

    // Выполняем сохранение в файл
    if (settingsManager.saveSettings()) {
      Serial.println("Main: Settings saved successfully.");
    } else {
      Serial.println("Main: ERROR: Failed to save settings to SPIFFS!");
    }

    // Сбрасываем флаги
    settingsManager.settings.isSaveRequested = false;
    settingsManager.settings.isRebootRequested = false;

    // Если была запрошена перезагрузка, выполняем ее
    if (needsReboot) {
      Serial.println("Main: Reboot requested by web server. Restarting...");
      delay(100);
      ESP.restart();
    }
  }

}
