#pragma once

#include <Arduino.h>
#include "Configuration.h"

template <int WINDOW_SIZE = 15>
class BatteryMonitor {
private:
    uint8_t _pin;
    uint32_t _readings[WINDOW_SIZE] = {};
    int _readIndex = 0;
    uint32_t _total = 0;

    float getBatteryPercentage(float voltage) {
        if (voltage >= 4.2f) return 100.0f;
        if (voltage <= 3.3f) return 0.0f;
        return (voltage - 3.3f) / (4.2f - 3.3f) * 100.0f;
    }

    uint32_t readBatteryMilliVolts() {
#if BOARD == BOARD_NRF52_XIAO
        // The XIAO battery divider is high impedance. Enable it only while
        // sampling, allow it to settle, discard a few conversions, then
        // average a batch. This follows Seeed's current nRF52840 battery
        // sampling approach and avoids seeding the long moving average from
        // one unstable startup conversion.
        digitalWrite(VBAT_ENABLE, LOW);
        delay(30);

        for (uint8_t i = 0; i < 6; ++i) {
            (void)analogRead(_pin);
            delay(2);
        }

        uint32_t sum = 0;
        const uint8_t samples = 16;
        for (uint8_t i = 0; i < samples; ++i) {
            sum += analogRead(_pin);
            delay(2);
        }

        digitalWrite(VBAT_ENABLE, HIGH);

        const float raw = (float)sum / (float)samples;
        const float pinMv = (raw * (float)MILLIVOLTFULLSCALE) /
                            (float)(STEPSFULLSCALE - 1);
        return (uint32_t)(pinMv * BATRESISTORCOMP);
#elif BOARD == BOARD_NRF52_FEATHER
        const uint32_t raw = analogRead(_pin);
        const float pinMv = ((float)raw * (float)MILLIVOLTFULLSCALE) /
                            (float)(STEPSFULLSCALE - 1);
        return (uint32_t)(pinMv * BATRESISTORCOMP);
#else
        // Preserve the existing ESP32 behavior.
        return analogReadMilliVolts(_pin) * 2U;
#endif
    }

public:
    void setup(uint8_t pin) {
        _pin = pin;
        analogReadResolution(12);

#if BOARD == BOARD_NRF52_XIAO
        pinMode(VBAT_ENABLE, OUTPUT);
        digitalWrite(VBAT_ENABLE, HIGH);
#endif

        const uint32_t startValue = readBatteryMilliVolts();
        for (int i = 0; i < WINDOW_SIZE; i++) {
            _readings[i] = startValue;
            _total += startValue;
        }
    }

    void read(float &voltage, float &percentage) {
        _total -= _readings[_readIndex];

        _readings[_readIndex] = readBatteryMilliVolts();
        _total += _readings[_readIndex];

        _readIndex = (_readIndex + 1) % WINDOW_SIZE;

        const float averageMv = (float)_total / WINDOW_SIZE;
        voltage = averageMv / 1000.0f;
        percentage = getBatteryPercentage(voltage);
    }
};
