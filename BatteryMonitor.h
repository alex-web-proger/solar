#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <Adafruit_INA3221.h>

class BatteryMonitor {
private:
    Adafruit_INA3221 _ina;
    float _shunts[3]; // Массив для хранения номиналов шунтов
    int k[3];

public:
    // Конструктор: сохраняем номиналы в массив
    BatteryMonitor(float s0, float s1, float s2) {
        _shunts[0] = s0;
        _shunts[1] = s1;
        _shunts[2] = s2;
        k[0] = -1;   // напряжение-ток идущий в квартиру 12 вольт
        k[1] = -1;  // напряжение-ток питающий 5 вольт
        k[2] = -1;  // напряжение-ток батареи
    }

    bool begin() {
        if (!_ina.begin()) return false;
        
        // В этой версии методы называются так:
        _ina.setAveragingMode(INA3221_AVG_64_SAMPLES);
        _ina.setBusVoltageConvTime(INA3221_CONVTIME_1MS);
        _ina.setShuntVoltageConvTime(INA3221_CONVTIME_1MS);
        
        // Сообщаем библиотеке номиналы шунтов, чтобы она сама считала ток
        _ina.setShuntResistance(0, _shunts[0]);
        _ina.setShuntResistance(1, _shunts[1]);
        _ina.setShuntResistance(2, _shunts[2]);
        
        return true;
    }

    float getVoltage(uint8_t channel) {
        if (channel > 2) return 0;
        return _ina.getBusVoltage(channel);
    }

    float getCurrentAmps(uint8_t channel) {
        if (channel > 2) return 0;
        // В вашей библиотеке есть встроенный метод расчета тока
        return _ina.getCurrentAmps(channel) * k[channel];
    }

    void printStatus(uint8_t channel) {
        Serial.printf("CH%d: %.2f V | %.3f A\n", 
                      channel + 1, getVoltage(channel), getCurrentAmps(channel));
    }
};

#endif
