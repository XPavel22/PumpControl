#include "control.h"

ControlManager::ControlManager(SettingsManager& settingsManager)
    : _settingsManager(settingsManager), _sonar(nullptr) {

    for (int i = 0; i < MEDIAN_FILTER_SIZE; i++) {
        _distanceReadings[i] = 0.0;
    }
    _readingIndex = 0;
}

void ControlManager::begin() {
    const auto& control = _settingsManager.settings.control;

    pinMode(control.pin_pump, OUTPUT);
    pinMode(control.pin_led, OUTPUT);
    pinMode(control.pin_button, INPUT_PULLUP);

    analogWrite(control.pin_led, 80);

    _controlPin(false);
    _controlLed(0, false);

    _sonar = new NewPing(control.pin_trig, control.pin_echo, 400);

    Serial.println("ControlManager: Pins initialized and sensor object created.");
}

void ControlManager::update() {

    float distance = _readSensor();
    _settingsManager.settings.control.currentDistance = distance;

    const int maxSensorDistance = 400;

 bool isOutOfRange = (distance < 30) ||  (distance > 380);

    if (!_settingsManager.settings.control.manualMode_pump) {
        if (isOutOfRange) {
            if (!_isPotentialErrorState) {
                Serial.println("ControlManager: Out of range detected. Starting error timer.");
                _isPotentialErrorState = true;
                _errorConditionStartTime = millis();
            } else if (!_isErrorState && (millis() - _errorConditionStartTime > _errorDelayMs)) {
                Serial.println("ControlManager: Error timer expired. Entering ERROR state.");
                _isErrorState = true;
            }
        } else {
            if (_isErrorState || _isPotentialErrorState) {
                Serial.println("ControlManager: Back in range. Clearing error state.");
            }
            _isErrorState = false;
            _isPotentialErrorState = false;
        }
    } else {
        if (_isErrorState || _isPotentialErrorState) {
            Serial.println("ControlManager: Manual mode active. Clearing any previous error state.");
            _isErrorState = false;
            _isPotentialErrorState = false;
        }
    }

    if (!_settingsManager.settings.control.manualMode_pump) {
        _controlPump();
    }

    if (_isErrorState) {
        _controlLed(0, true);
    } else {
        if (_isPumpOn) {
            _controlLed(200, false);
        } else {
            _controlLed(80, false);
        }
    }
}

float ControlManager::getCurrentDistance() const {
    return _settingsManager.settings.control.currentDistance;
}

void ControlManager::setManualMode(bool enabled) {
    _settingsManager.settings.control.manualMode_pump = enabled;
    if (enabled) {
        Serial.println("ControlManager: Switched to MANUAL mode.");
    } else {
        Serial.println("ControlManager: Switched to AUTOMATIC mode.");
    }
}

void ControlManager::setPumpState(bool isOn) {
    if (_settingsManager.settings.control.manualMode_pump) {
        _isPumpOn = isOn;
        _controlPin(_isPumpOn);
        Serial.printf("ControlManager: Manual pump state set to %s\n", _isPumpOn ? "ON" : "OFF");
    }
}

bool ControlManager::getPumpState() const {
    return _isPumpOn;
}

bool ControlManager::isErrorState() const {
    return _isErrorState;
}

float ControlManager::_readSensor() {
    if (!_sonar) return 0.0;
    delay(29);

    float newReading = _sonar->ping_cm();
    _distanceReadings[_readingIndex] = newReading;
    _readingIndex = (_readingIndex + 1) % MEDIAN_FILTER_SIZE;

    float sortedReadings[MEDIAN_FILTER_SIZE];
    for (int i = 0; i < MEDIAN_FILTER_SIZE; i++) {
        sortedReadings[i] = _distanceReadings[i];
    }

    for (int i = 0; i < MEDIAN_FILTER_SIZE - 1; i++) {
        for (int j = 0; j < MEDIAN_FILTER_SIZE - i - 1; j++) {
            if (sortedReadings[j] > sortedReadings[j + 1]) {
                float temp = sortedReadings[j];
                sortedReadings[j] = sortedReadings[j + 1];
                sortedReadings[j + 1] = temp;
            }
        }
    }

    return sortedReadings[MEDIAN_FILTER_SIZE / 2];
}

void ControlManager::_controlPump() {
    if (_isErrorState) {
        if (_isPumpOn) {
            _isPumpOn = false;
            _controlPin(_isPumpOn);
            Serial.println("ControlManager: ERROR state. Pump forced OFF.");
        }
        return;
    }

    float distance = _settingsManager.settings.control.currentDistance;

    float onDistance = _settingsManager.settings.control.minTrigger;

    float offDistance = _settingsManager.settings.control.maxTrigger;

    if (distance <= onDistance && !_isPumpOn) {
        _isPumpOn = true;
        Serial.printf("ControlManager: Water level is high (%.2f cm). Pump ON.\n", distance);
    }

    else if (distance >= offDistance && _isPumpOn) {
        _isPumpOn = false;
        Serial.printf("ControlManager: Water level is low (%.2f cm). Pump OFF.\n", distance);
    }

    _controlPin(_isPumpOn);
}

void ControlManager::_controlPin(bool state) {
    int pin = _settingsManager.settings.control.pin_pump;
    if (state) {
         digitalWrite(pin, HIGH);
    } else {
         digitalWrite(pin, LOW);
    }
}

void ControlManager::_controlLed(int pwm, bool error) {
    int pin = _settingsManager.settings.control.pin_led;

    if (error) {
        if (millis() - _lastBlinkTime > _blinkInterval) {
            _lastBlinkTime = millis();
            _ledState = !_ledState;
        }
        digitalWrite(pin, _ledState ? HIGH : LOW);
    } else {
        analogWrite(pin, pwm);
    }
}
