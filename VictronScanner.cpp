#include "VictronScanner.h"

VictronScanner::VictronScanner(HardwareSerial& serial, const int* pins, int count) 
    : _serial(serial), _pins(pins), _count(count) {
    _data = new VictronData[count];
    _mutex = xSemaphoreCreateMutex();
    _buffer.reserve(100);
}

void VictronScanner::begin() {
    _serial.begin(19200, SERIAL_8N1, _pins[_currentIdx], -1);
    _lastSwitch = millis();
}

void VictronScanner::update() {
    // 1. Чтение порта
    while (_serial.available()) {
        char c = _serial.read();
        if (c == '\n') {
            parseLine(_buffer);
            _buffer = "";
        } else if (c != '\r' && _buffer.length() < 80) {
            _buffer += c;
        }
    }

    // 2. Переключение пинов каждые 3 секунды
    if (millis() - _lastSwitch > 2000) {
        _currentIdx = (_currentIdx + 1) % _count;
        _serial.end();
        _serial.begin(19200, SERIAL_8N1, _pins[_currentIdx], -1);
        _buffer = "";
        _lastSwitch = millis();
    }
}

void VictronScanner::parseLine(String line) {
    line.trim();
    int tab = line.indexOf('\t');
    if (tab <= 0) return;

    String key = line.substring(0, tab);
    long val = line.substring(tab + 1).toInt();

    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10))) {
        if (key == "V")        _data[_currentIdx].batteryV = val / 1000.0;
        else if (key == "I")   _data[_currentIdx].batteryI = val / 1000.0;
        else if (key == "VPV") _data[_currentIdx].pvV = val / 1000.0;
        else if (key == "PPV") _data[_currentIdx].pvP = val;
        else if (key == "CS")  _data[_currentIdx].state = (int)val;
        _data[_currentIdx].lastUpdate = millis();
        xSemaphoreGive(_mutex);
    }
}

VictronData VictronScanner::getData(int index) {
    VictronData temp;
    if (index >= 0 && index < _count) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50))) {
            temp = _data[index];
            xSemaphoreGive(_mutex);
        }
    }
    return temp;
}

void VictronScanner::dataPrint(int index) {

    VictronData d = VictronScanner::getData(index);

    Serial.print("MPPT_");Serial.print(index);
    Serial.print(" Battery Voltage: "); Serial.print(d.batteryV); Serial.print("V ");
    Serial.print("Battery Current: "); Serial.print(d.batteryI); Serial.print("A ");
    Serial.print("PV Voltage: "); Serial.print(d.pvV); Serial.print("V ");
    Serial.print("PV Power: "); Serial.print(d.pvP); Serial.print("W ");
    Serial.print("State: "); Serial.println(d.state);
}
