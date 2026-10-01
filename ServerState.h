#ifndef SERVER_STATE_H
#define SERVER_STATE_H

// Состояние, восстановленное со своего сервера при старте устройства.
// Сервер сам отвечает только за текущие сутки, поэтому отдельной проверки
// даты на стороне устройства не требуется — либо found=true и все поля
// валидны, либо found=false (записи за сегодня ещё не было).
struct ServerState {
    bool found = false;
    float soc = 0;
    float mppt1Wh = 0;
    float mppt2Wh = 0;
    float mppt3Wh = 0;
    float mppt4Wh = 0;
    float consumedWh = 0;
    float batteryChargedWh = 0;
    float batteryDischargedWh = 0;
};

#endif
