#ifndef CONTROL_H
#define CONTROL_H

#include <Arduino.h>
#include <NewPing.h>
#include "settings.h"

class ControlManager {
public:
    ControlManager(SettingsManager& settingsManager);
    void begin();
    void update();

    float getCurrentDistance() const;
    void setManualMode(bool enabled);
    void setPumpState(bool isOn);
    bool getPumpState() const;
    bool isErrorState() const;

private:
    SettingsManager& _settingsManager;
    NewPing* _sonar;

    bool _isPumpOn = false;
    bool _isErrorState = false;
    bool _isPotentialErrorState = false;
    unsigned long _errorConditionStartTime = 0;
    const unsigned long _errorDelayMs = 3000;

    unsigned long _lastBlinkTime = 0;
    bool _ledState = false;
    const unsigned long _blinkInterval = 150;

    static const int MEDIAN_FILTER_SIZE = 5;
    float _distanceReadings[MEDIAN_FILTER_SIZE];
    int _readingIndex = 0;

    float _readSensor();
    void _controlPump();
    void _controlPin(bool state);
    void _controlLed(int pwm, bool error);
};

#endif
