#ifndef VICTRON_SCANNER_H
#define VICTRON_SCANNER_H

#include <Arduino.h>

// Структура данных одного контроллера
struct VictronData {
    float batteryV = 0, batteryI = 0, pvV = 0;
    long pvP = 0;
    int state = 0;
    uint32_t lastUpdate = 0;
};

class VictronScanner {
public:
    VictronScanner(HardwareSerial& serial, const int* pins, int count);
    void begin();
    void update();                  // Вызывать максимально часто в задаче
    VictronData getData(int index); // Безопасное получение данных
    void dataPrint(int index);
    int getCurrentIdx() { return _currentIdx; }

private:
    void parseLine(String line);
    
    HardwareSerial& _serial;
    const int* _pins;
    int _count;
    int _currentIdx = 0;
    VictronData* _data;
    String _buffer = "";
    uint32_t _lastSwitch = 0;
    SemaphoreHandle_t _mutex;
};

#endif
