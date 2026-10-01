#ifndef DWEET_STATE_H
#define DWEET_STATE_H

// Состояние, восстановленное с dweet.io при старте устройства
struct DweetState {
    bool socValid = false;
    float soc = 0;
    bool energyValid = false;
    float energyWhToday = 0;
    bool consumedValid = false;
    float consumedWhToday = 0;
};

#endif
