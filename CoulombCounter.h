#ifndef COULOMB_COUNTER_H
#define COULOMB_COUNTER_H

#include <Arduino.h>

// Кулонометр для свинцового АКБ.
// Интегрирует ток по времени и периодически ресинхронизируется
// по напряжению покоя (через переданную таблицу voltageToSoc),
// чтобы компенсировать накопленный дрейф интегрирования.
class CoulombCounter {
public:
    // capacityAh — номинальная ёмкость АКБ, Ач
    // chargeEff  — кулоновская эффективность заряда (0.85–0.95 для свинца)
    CoulombCounter(float capacityAh, float chargeEff = 0.92f)
        : _capacityAh(capacityAh), _chargeEff(chargeEff) {}

    // Инициализация начального SOC (0-100%), например по напряжению при старте
    void begin(float initialSocPercent) {
        _remainingAh = _capacityAh * constrain(initialSocPercent, 0.0f, 100.0f) / 100.0f;
        _lastUpdate = millis();
        _restSince = 0;
    }

    // Вызывать регулярно с текущим током (A, + = заряд, - = разряд),
    // текущим напряжением и функцией пересчёта напряжения в SOC (%).
    void update(float currentA, float voltage, float (*voltageToSoc)(float)) {
        uint32_t now = millis();
        float dtHours = (now - _lastUpdate) / 3600000.0f;
        _lastUpdate = now;

        // --- интегрирование тока ---
        float deltaAh = currentA * dtHours;
        if (deltaAh > 0) {
            deltaAh *= _chargeEff; // не весь ток заряда реально запасается
        }
        _remainingAh += deltaAh;
        _remainingAh = constrain(_remainingAh, 0.0f, _capacityAh);

        // --- ресинхронизация по напряжению покоя ---
        if (fabs(currentA) < REST_CURRENT_THRESHOLD_A) {
            if (_restSince == 0) _restSince = now;
            if (now - _restSince > REST_TIME_MS) {
                float tableSoc = voltageToSoc(voltage);
                float tableAh = _capacityAh * tableSoc / 100.0f;
                // Плавная подтяжка, чтобы не было визуального скачка
                _remainingAh += (tableAh - _remainingAh) * RESYNC_BLEND;
                _restSince = now; // не ресинкаем каждую итерацию подряд
            }
        } else {
            _restSince = 0;
        }
    }

    float getSocPercent() const {
        return 100.0f * _remainingAh / _capacityAh;
    }

    float getRemainingAh() const {
        return _remainingAh;
    }

    // Ручная коррекция (например, известно, что АКБ полностью заряжена)
    void setSocPercent(float socPercent) {
        _remainingAh = _capacityAh * constrain(socPercent, 0.0f, 100.0f) / 100.0f;
    }

private:
    static constexpr float REST_CURRENT_THRESHOLD_A = 0.15f;        // ток ниже — считаем "покой"
    static constexpr uint32_t REST_TIME_MS = 15UL * 60UL * 1000UL;  // 15 минут покоя для ресинхронизации
    static constexpr float RESYNC_BLEND = 0.3f;                     // доля подтяжки к табличному значению за один ресинк

    float _capacityAh;
    float _chargeEff;
    float _remainingAh = 0;
    uint32_t _lastUpdate = 0;
    uint32_t _restSince = 0;
};

#endif
