#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <time.h>
#include <ESPmDNS.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include "JKBMSInterface.h"
#include "VictronScanner.h"
#include "web_page.h"
#include <ArduinoJson.h>
#include <ZMPT101B.h>
#include "BatteryMonitor.h"
#include "CoulombCounter.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "ServerState.h"
#include <Preferences.h>
#include "settings_page.h"
#include "bms_page.h"
#include "mobile_page.h"

AsyncWebServer server(80);

JKBMSInterface bms(&Serial2);

// Очередь команды управления MOS заряда JK-BMS: -1 = нет команды, 0 = запретить заряд, 1 = разрешить.
// HTTP-обработчик только выставляет флаг, а саму команду по UART выполняет taskBMS — так UART не
// используется из двух задач одновременно, а асинхронный веб-сервер не блокируется до 3 секунд.
volatile int8_t bmsChargeCommand = -1;

String chargerState = "off";

const int pins[] = {25, 32, 33, 26};
VictronScanner scanner(Serial1, pins, 4);

#define SENSITIVITY 500.0f
ZMPT101B voltageSensor(35, 50.0);

// --- Фильтрация сетевого напряжения ---
// Сырые показания ZMPT101B могут давать единичные выбросы (наводки, дребезг АЦП).
// Медианный фильтр по последним N измерениям полностью отбрасывает такие одиночные скачки
// (пока их не больше половины окна подряд), а экспоненциальное сглаживание поверх медианы
// убирает остаточное дрожание между соседними измерениями.
#define AC_VOLTAGE_MEDIAN_WINDOW 5
int acVoltageRawBuffer[AC_VOLTAGE_MEDIAN_WINDOW] = {0};
int acVoltageBufferIndex = 0;
bool acVoltageBufferFilled = false;
float acVoltageFiltered = 0;
const float AC_VOLTAGE_EMA_ALPHA = 0.3f; // меньше — плавнее, но медленнее реакция на реальное изменение

int filterMainsVoltage(int rawReading) {
    // 1. Кладём новое сырое значение в кольцевой буфер
    acVoltageRawBuffer[acVoltageBufferIndex] = rawReading;
    acVoltageBufferIndex = (acVoltageBufferIndex + 1) % AC_VOLTAGE_MEDIAN_WINDOW;
    if (acVoltageBufferIndex == 0) acVoltageBufferFilled = true;

    int sampleCount = acVoltageBufferFilled ? AC_VOLTAGE_MEDIAN_WINDOW : acVoltageBufferIndex;
    if (sampleCount == 0) return rawReading; // буфер ещё пуст (первый вызов)

    // 2. Сортировка вставками для медианы — окно маленькое (5), дёшево на каждой итерации
    int sorted[AC_VOLTAGE_MEDIAN_WINDOW];
    for (int i = 0; i < sampleCount; i++) sorted[i] = acVoltageRawBuffer[i];
    for (int i = 1; i < sampleCount; i++) {
        int key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }
    int median = sorted[sampleCount / 2];

    // 3. Экспоненциальное сглаживание поверх медианы
    if (acVoltageFiltered == 0) {
        acVoltageFiltered = median; // первая инициализация без сглаживания, чтобы не ждать разгона EMA
    } else {
        acVoltageFiltered = AC_VOLTAGE_EMA_ALPHA * median + (1.0f - AC_VOLTAGE_EMA_ALPHA) * acVoltageFiltered;
    }

    return (int)roundf(acVoltageFiltered);
}

int inputACVoltage = 0;
float solarMainVoltage = 0;
bool inaError = false;

// Создаем объект с шунтами: R050, R100, R010
BatteryMonitor battery(0.05, 0.1, 0.01);

// Кулонометр для свинцового АКБ. Ёмкость (Ач) подставьте реальную для вашей батареи.
CoulombCounter coulomb(44.0, 0.92);

float batteryVoltage = 0;
float batteryCurrent = 0;
float loadDC_Current = 0;
float loadMainCurrent = 0;

// --- Учёт выработки солнечных панелей за сутки (Втч), по каждому из 4 MPPT и суммарно
float dailyMpptWh[4] = {0, 0, 0, 0};
float dailyEnergyWh = 0;
int lastYday = -1;   // день года (0-365), для определения смены суток
int lastYear = -1;
uint32_t lastEnergyUpdate = 0;

// --- Учёт потребленной мощности за сутки (Втч), каналы 0 (12В квартира) и 1 (5В нагрузка) INA3221
float dailyConsumedWh = 0;
uint32_t lastLoadEnergyUpdate = 0;

// --- Энергия, полученная/отданная батареей за сутки (Втч), канал 2 INA3221
float dailyBatteryChargedWh = 0;
float dailyBatteryDischargedWh = 0;
uint32_t lastBatteryEnergyUpdate = 0;

// --- Автоматическая подзарядка от сети (настраиваемые параметры, хранятся в NVS через Preferences) ---
Preferences chargerPrefs;

// Тип аккумулятора: "gel" (свинцовый/гелевый, через INA3221+кулонометр) или "lfp" (через JK-BMS напрямую)
String BATTERY_CHEMISTRY = "gel";

// Номинальная ёмкость LFP-пакета (Ач) — в протоколе JK нет достоверно подтверждённого
// поля для этого, поэтому задаётся вручную (как и еёмкость гелевого АКБ в CoulombCounter выше)
float LFP_CAPACITY_AH = 100.0;

// Дефолты для первого запуска каждой химии (одинаковые, дальше настраиваются независимо на /settings)
const float DEFAULT_SOC_LOW = 50.0;
const float DEFAULT_LOAD_SOC = 50.0;
const int DEFAULT_MAINS_V = 150;
const float DEFAULT_SOLAR_NEG = 10.0;
const float DEFAULT_LOAD_ON = 2.0;
const float DEFAULT_LOAD_OFF = 1.5;
const float DEFAULT_LOAD_OFF_DELAY = 30.0;
const uint32_t DEFAULT_OFF_SUPPRESS_MIN = 60;

// Текущие активные значения (загружаются из NVS под текущую химию — см. loadChemistrySettings())
float BATTERY_LOW_SOC_THRESHOLD = DEFAULT_SOC_LOW;       // %, ниже этого — начинаем подзаряжать
float HIGH_LOAD_SOC_THRESHOLD = DEFAULT_LOAD_SOC;         // %, отдельный порог SOC для триггера по высокому току нагрузки
int MAINS_VOLTAGE_ON_THRESHOLD = DEFAULT_MAINS_V;         // В, порог наличия сети (как и в load.mainsStatus)
float SOLAR_POWER_NEGLIGIBLE_W = DEFAULT_SOLAR_NEG;        // Вт, суммарная мощность всех MPPT ниже этого считается мизерной
float LOAD_CURRENT_ON_THRESHOLD = DEFAULT_LOAD_ON;        // А, при таком токе нагрузки включаем подзарядку от сети
float LOAD_CURRENT_OFF_THRESHOLD = DEFAULT_LOAD_OFF;       // А, гистерезис — выключаем, когда ток опустился до этого значения
float LOAD_CURRENT_OFF_DELAY_S = DEFAULT_LOAD_OFF_DELAY;        // сек, ток должен пробыть ниже порога выключения не менее этого времени подряд
uint32_t MANUAL_OFF_SUPPRESS_MS = DEFAULT_OFF_SUPPRESS_MIN * 60UL * 1000UL; // мс, пауза автоматики после ручного выключения

bool chargeNeeded = false; // требуется ли подзарядка от сети (держится до достижения float)
bool mainsPresent = false; // есть ли сейчас сетевое напряжение
bool solarNegligible = false; // солнечная генерация сейчас мизерная/отсутствует
bool highLoadChargeActive = false; // держит состояние гистерезиса подзарядки по высокому току нагрузки
uint32_t loadCurrentOffSince = 0; // millis() момента, когда ток нагрузки впервые опустился ниже порога выключения (0 = не опускался)

// Общий выключатель всей автоматики подзарядки (по разряду и по высокому току). Ручное
// управление кнопкой работает независимо от этого флага.
bool AUTOMATION_ENABLED = true;

// Приоритет сети: после каждого зафиксированного пропадания сети на заданное время включается
// повышенный порог активации подзарядки (цель — догнать батарею до высокого SOC на случай
// повторного блекаута), вместо обычного экономного порога.
bool MAINS_PRIORITY_ENABLED = false;
float MAINS_PRIORITY_TARGET_SOC = 90.0;      // %, целевой порог активации в окне приоритета
float MAINS_PRIORITY_MEMORY_HOURS = 12.0;    // часов, на сколько долго помним про последнее пропадание сети
uint32_t lastMainsLossTime = 0;              // millis() момента последнего зафиксированного пропадания сети (0 = ещё не было)
bool mainsPriorityActive = false;            // сейчас ли активно окно повышенного приоритета (для отображения в API)

// Пороги разбалансировки ячеек LFP (мВ) для индикатора на главной странице
float CELL_IMBALANCE_WARN_MV = 30.0;   // жёлтый
float CELL_IMBALANCE_CRIT_MV = 60.0;   // красный

// --- Ручное вмешательство в автоматику подзарядки ---
bool manualChargeRequested = false;     // пользователь принудительно запросил заряд кнопкой — идёт до float, игнорируя SOC/солнце
uint32_t manualOverrideOffUntil = 0;    // millis(), до которого подавлено автовключение после ручного выключения (0 = нет подавления)

// Загрузка/сохранение типа аккумулятора из NVS
void loadBatteryChemistry() {
    chargerPrefs.begin("chgsettings", false);
    BATTERY_CHEMISTRY = chargerPrefs.getString("chemistry", "gel");
    LFP_CAPACITY_AH = chargerPrefs.getFloat("lfpCapAh", LFP_CAPACITY_AH);
    AUTOMATION_ENABLED = chargerPrefs.getBool("autoEnabled", true);
    MAINS_PRIORITY_ENABLED = chargerPrefs.getBool("mainsPrio", false);
    MAINS_PRIORITY_TARGET_SOC = chargerPrefs.getFloat("mpTargetSoc", MAINS_PRIORITY_TARGET_SOC);
    MAINS_PRIORITY_MEMORY_HOURS = chargerPrefs.getFloat("mpMemH", MAINS_PRIORITY_MEMORY_HOURS);
    CELL_IMBALANCE_WARN_MV = chargerPrefs.getFloat("cellWarnMv", CELL_IMBALANCE_WARN_MV);
    CELL_IMBALANCE_CRIT_MV = chargerPrefs.getFloat("cellCritMv", CELL_IMBALANCE_CRIT_MV);
    chargerPrefs.end();
}

void saveBatteryChemistry() {
    chargerPrefs.begin("chgsettings", false);
    chargerPrefs.putString("chemistry", BATTERY_CHEMISTRY);
    chargerPrefs.end();
}

void saveLfpCapacity() {
    chargerPrefs.begin("chgsettings", false);
    chargerPrefs.putFloat("lfpCapAh", LFP_CAPACITY_AH);
    chargerPrefs.end();
}

void saveAutomationEnabled() {
    chargerPrefs.begin("chgsettings", false);
    chargerPrefs.putBool("autoEnabled", AUTOMATION_ENABLED);
    chargerPrefs.end();
}

void saveMainsPriority() {
    chargerPrefs.begin("chgsettings", false);
    chargerPrefs.putBool("mainsPrio", MAINS_PRIORITY_ENABLED);
    chargerPrefs.end();
}

void saveMainsPriorityParams() {
    chargerPrefs.begin("chgsettings", false);
    chargerPrefs.putFloat("mpTargetSoc", MAINS_PRIORITY_TARGET_SOC);
    chargerPrefs.putFloat("mpMemH", MAINS_PRIORITY_MEMORY_HOURS);
    chargerPrefs.end();
}

void saveCellImbalanceThresholds() {
    chargerPrefs.begin("chgsettings", false);
    chargerPrefs.putFloat("cellWarnMv", CELL_IMBALANCE_WARN_MV);
    chargerPrefs.putFloat("cellCritMv", CELL_IMBALANCE_CRIT_MV);
    chargerPrefs.end();
}

// Загрузка настроек автоподзарядки из NVS для УКАЗАННОЙ химии (ключи с префиксом "g_"/"l_",
// чтобы gel и lfp хранили свои значения независимо). Если значений ещё нет — дефолты.
void loadChemistrySettings(const String &chem) {
    String p = (chem == "lfp") ? "l_" : "g_";
    chargerPrefs.begin("chgsettings", false);
    BATTERY_LOW_SOC_THRESHOLD = chargerPrefs.getFloat((p + "socLow").c_str(), DEFAULT_SOC_LOW);
    HIGH_LOAD_SOC_THRESHOLD = chargerPrefs.getFloat((p + "loadSoc").c_str(), DEFAULT_LOAD_SOC);
    MAINS_VOLTAGE_ON_THRESHOLD = chargerPrefs.getInt((p + "mainsV").c_str(), DEFAULT_MAINS_V);
    SOLAR_POWER_NEGLIGIBLE_W = chargerPrefs.getFloat((p + "solarNeg").c_str(), DEFAULT_SOLAR_NEG);
    LOAD_CURRENT_ON_THRESHOLD = chargerPrefs.getFloat((p + "loadOn").c_str(), DEFAULT_LOAD_ON);
    LOAD_CURRENT_OFF_THRESHOLD = chargerPrefs.getFloat((p + "loadOff").c_str(), DEFAULT_LOAD_OFF);
    LOAD_CURRENT_OFF_DELAY_S = chargerPrefs.getFloat((p + "loadOffDelay").c_str(), DEFAULT_LOAD_OFF_DELAY);
    uint32_t suppressMin = chargerPrefs.getUInt((p + "offSuppMin").c_str(), DEFAULT_OFF_SUPPRESS_MIN);
    MANUAL_OFF_SUPPRESS_MS = suppressMin * 60UL * 1000UL;
    chargerPrefs.end();
    Serial.printf("Настройки автоподзарядки загружены для химии: %s\n", chem.c_str());
}

void saveChemistrySettings(const String &chem) {
    String p = (chem == "lfp") ? "l_" : "g_";
    chargerPrefs.begin("chgsettings", false);
    chargerPrefs.putFloat((p + "socLow").c_str(), BATTERY_LOW_SOC_THRESHOLD);
    chargerPrefs.putFloat((p + "loadSoc").c_str(), HIGH_LOAD_SOC_THRESHOLD);
    chargerPrefs.putInt((p + "mainsV").c_str(), MAINS_VOLTAGE_ON_THRESHOLD);
    chargerPrefs.putFloat((p + "solarNeg").c_str(), SOLAR_POWER_NEGLIGIBLE_W);
    chargerPrefs.putFloat((p + "loadOn").c_str(), LOAD_CURRENT_ON_THRESHOLD);
    chargerPrefs.putFloat((p + "loadOff").c_str(), LOAD_CURRENT_OFF_THRESHOLD);
    chargerPrefs.putFloat((p + "loadOffDelay").c_str(), LOAD_CURRENT_OFF_DELAY_S);
    chargerPrefs.putUInt((p + "offSuppMin").c_str(), MANUAL_OFF_SUPPRESS_MS / 60000UL);
    chargerPrefs.end();
    Serial.printf("Настройки автоподзарядки сохранены для химии: %s\n", chem.c_str());
}

// Источник SOC зависит от выбранной химии: гелевый — кулонометр (INA3221),
// LFP — напрямую с JK-BMS. При отсутствии связи с BMS возвращаем 100% — чтобы коммуникационный
// сбой не вызвал ложный запуск подзарядки из-за мнимого SOC=0.
float getBatterySocPercent() {
    if (BATTERY_CHEMISTRY == "lfp") {
        return bms.isDataValid() ? (float)bms.getSOC() : 100.0f;
    }
    return coulomb.getSocPercent();
}

// Разбалансировка ячеек (мВ) — только для LFP с валидными данными с BMS, иначе 0 (чтобы индикатор
// на главной странице оставался скрытым). Вынесено в отдельную функцию, так как
// в /api/solar-data есть локальный JsonObject bms, который перекрывает глобальный JKBMSInterface bms.
float getLfpCellDeltaMv() {
    if (BATTERY_CHEMISTRY == "lfp" && bms.isDataValid()) {
        return bms.getCellVoltageDelta() * 1000.0f;
    }
    return 0;
}

// Периодическая пересинхронизация времени с NTP-сервером
uint32_t lastNtpResync = 0;
const uint32_t NTP_RESYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL; // 6 часов

float voltageToSocWrapper(float v) {
    return getBatteryLevel(v);
}

// --- Отправка данных на свой сервер каждую минуту ---
const char* SERVER_BASE_URL = "https://kontro03.web01.net";
const unsigned long SERVER_SEND_INTERVAL_MS = 1UL * 60UL * 1000UL; // 1 минута

// Сегодняшняя дата в формате YYYY-MM-DD (по времени, синхронизированному по NTP)
String getTodayDateString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return "";
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
    return String(buf);
}

void sendToServer() {
    if (WiFi.status() != WL_CONNECTED) return;

    WiFiClientSecure client;
    client.setInsecure(); // без проверки сертификата

    HTTPClient http;
    String url = String(SERVER_BASE_URL) + "/api/solar-log";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");

    JsonDocument doc;
    doc["date"] = getTodayDateString();
    doc["mppt1_wh"] = dailyMpptWh[0];
    doc["mppt2_wh"] = dailyMpptWh[1];
    doc["mppt3_wh"] = dailyMpptWh[2];
    doc["mppt4_wh"] = dailyMpptWh[3];
    doc["consumed_wh"] = dailyConsumedWh;
    doc["battery_soc"] = getBatterySocPercent();
    doc["battery_charged_wh"] = dailyBatteryChargedWh;
    doc["battery_discharged_wh"] = dailyBatteryDischargedWh;

    String body;
    serializeJson(doc, body);

    int httpCode = http.POST(body);
    if (httpCode > 0) {
        Serial.printf("Server: отправлено, код %d\n", httpCode);
        if (httpCode >= 400) {
            Serial.println(http.getString()); // текст ошибки валидации от сервера, если есть
        }
    } else {
        Serial.printf("Server: ошибка отправки (%s)\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}

void taskServerLog(void *pvParameters) {
    while (true) {
        sendToServer();
        vTaskDelay(SERVER_SEND_INTERVAL_MS / portTICK_PERIOD_MS);
    }
}

// --- Одноразовая синхронизация при старте в отдельной задаче (чтобы не блокировать loopTask на время запроса)
volatile bool serverSyncDone = false;
ServerState serverSyncResult;

void taskInitialServerSync(void *pvParameters) {
    serverSyncResult = fetchStateFromServer();
    serverSyncDone = true;
    vTaskDelete(NULL);
}

// Получить данные за сегодня со своего сервера (для восстановления после сброса)
ServerState fetchStateFromServer() {
    ServerState state;
    if (WiFi.status() != WL_CONNECTED) return state;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(SERVER_BASE_URL) + "/api/solar-log/today";
    http.begin(client, url);
    http.addHeader("Accept", "application/json");
    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            state.found = doc["found"] | false;
            if (state.found) {
                state.soc = doc["battery_soc"] | 0.0f;
                state.mppt1Wh = doc["mppt1_wh"] | 0.0f;
                state.mppt2Wh = doc["mppt2_wh"] | 0.0f;
                state.mppt3Wh = doc["mppt3_wh"] | 0.0f;
                state.mppt4Wh = doc["mppt4_wh"] | 0.0f;
                state.consumedWh = doc["consumed_wh"] | 0.0f;
                state.batteryChargedWh = doc["battery_charged_wh"] | 0.0f;
                state.batteryDischargedWh = doc["battery_discharged_wh"] | 0.0f;
            }
        } else {
            Serial.printf("Server: ошибка разбора JSON (%s)\n", err.c_str());
        }
    } else {
        Serial.printf("Server: не удалось получить данные за сегодня, код %d\n", httpCode);
    }
    http.end();
    return state;
}

//*******************************************
//          Сканер MPPT и BMS              //
//*******************************************
void taskScanner(void *pvParameters) {

  scanner.begin();

  while (true) {

    scanner.update();

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// Опрос JK-BMS по UART. bms.update() сама шлёт запрос раз в 5 секунд и обрабатывает
// пришедшие байты — вызывать часто, чтобы не терять байты ответа и не переполнять UART-буфер.
void taskBMS(void *pvParameters) {
  while (true) {
    bms.update();

    int8_t cmd = bmsChargeCommand;
    if (cmd != -1) {
      bmsChargeCommand = -1;
      bool ok = bms.setChargeMOS(cmd == 1);
      Serial.printf("JK-BMS: %s заряд — %s\n", cmd == 1 ? "разрешить" : "запретить", ok ? "команда подтверждена" : "нет подтверждения");
      bms.requestData(); // сразу перечитать статус, не дожидаясь 5-секундного цикла
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ---------- Задача на ядре 1 ----------
void taskCore1(void *pvParameters) {

  static uint32_t lastPrint = 0;
  long sum = 0;

  while (true) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    //Serial.printf("[CORE 1] Время: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Определяем смену суток для сброса счётчика выработки
    if (lastYday == -1) {
        lastYday = timeinfo.tm_yday;
        lastYear = timeinfo.tm_year;
    } else if (timeinfo.tm_yday != lastYday || timeinfo.tm_year != lastYear) {
        dailyMpptWh[0] = 0;
        dailyMpptWh[1] = 0;
        dailyMpptWh[2] = 0;
        dailyMpptWh[3] = 0;
        dailyEnergyWh = 0;
        dailyConsumedWh = 0;
        dailyBatteryChargedWh = 0;
        dailyBatteryDischargedWh = 0;
        lastYday = timeinfo.tm_yday;
        lastYear = timeinfo.tm_year;
        Serial.println("Новые сутки: счётчики выработки и потребления сброшены");
    }

    // Периодическая пересинхронизация времени с NTP
    syncTimeIfNeeded();

    // Генерация по каждому MPPT отдельно (Вт) и интегрирование в Втч за сутки
    uint32_t nowMsEnergy = millis();
    if (lastEnergyUpdate != 0) {
        float dtHours = (nowMsEnergy - lastEnergyUpdate) / 3600000.0f;
        for (int i = 0; i < 4; i++) {
            dailyMpptWh[i] += scanner.getData(i).pvP * dtHours;
        }
    }
    lastEnergyUpdate = nowMsEnergy;
    dailyEnergyWh = dailyMpptWh[0] + dailyMpptWh[1] + dailyMpptWh[2] + dailyMpptWh[3];

    if (millis() - lastPrint > 5000) {

        lastPrint = millis();
            
        //scanner.dataPrint(0);

    }

    // Update BMS data (call this regularly)
    /*bms.update();
    
    // Check if we have valid data
    if (bms.isDataValid()) {
        // Access battery data
        float voltage = bms.getVoltage();
        uint8_t soc = bms.getSOC();
        float current = bms.getCurrent();
        
        Serial.print("Voltage: ");
        Serial.print(voltage, 2);
        Serial.print("V, SOC: ");
        Serial.print(soc);
        Serial.print("%, Current: ");
        Serial.print(current, 2);
        Serial.print("A, CellVoltage: ");
        Serial.print(bms.getCellVoltage(0), 2);
        Serial.print("V ");
        Serial.print(bms.getCellVoltage(1), 2);
        Serial.print("V ");
        Serial.print(bms.getCellVoltage(2), 2);
        Serial.print("V, CellVoltDelta: ");
        Serial.print(bms.getCellVoltageDelta(), 2);
        Serial.print("V, BatteryTemp: ");
        Serial.print(bms.getBatteryTemp(), 2);
        Serial.print("C, PowerTemp: ");
        Serial.print(bms.getPowerTemp(), 2);
        Serial.print("C, BoxTemp: ");
        Serial.print(bms.getBoxTemp(), 2);
        Serial.println("C");
    }*/

    inputACVoltage = filterMainsVoltage(voltageSensor.getRmsVoltage());
  
    // Усредняем значения для фильтрации шумов
 //   for (int i = 0; i < 400; i++) {
 //     sum += analogRead(34);
 //   }
 //   solarMainVoltage = (float)sum / 400 / 0.0714;
 //   sum = 0;

    if(inaError == false){
       if (BATTERY_CHEMISTRY == "lfp") {
           // LFP: напрямую с JK-BMS. При отсутствии связи не затираем последние валидные
           // значения нулями, чтобы кратковременный сбой связи не выглядел как резкий разряд.
           if (bms.isDataValid()) {
               batteryVoltage = bms.getVoltage();
               batteryCurrent = bms.getCurrent();
           }
       } else {
           batteryVoltage = battery.getVoltage(2);
           batteryCurrent = battery.getCurrentAmps(2);
       }
       loadDC_Current = battery.getCurrentAmps(1);
       loadMainCurrent = battery.getCurrentAmps(0);

       // Кулонометр актуален только для гелевого АКБ — для LFP его не трогаем, чтобы не портить
       // внутреннее состояние через таблицу для свинца, применяя её к напряжению LFP
       if (BATTERY_CHEMISTRY != "lfp") {
           coulomb.update(batteryCurrent, batteryVoltage, voltageToSocWrapper);
       }

       // Энергия батареи: при заряде (+) копится в dailyBatteryChargedWh, при разряде (-) — в dailyBatteryDischargedWh
       float batteryPowerW = batteryVoltage * batteryCurrent;
       uint32_t nowMsBatt = millis();
       if (lastBatteryEnergyUpdate != 0) {
           float dtHoursBatt = (nowMsBatt - lastBatteryEnergyUpdate) / 3600000.0f;
           if (batteryPowerW > 0) {
               dailyBatteryChargedWh += batteryPowerW * dtHoursBatt;
           } else {
               dailyBatteryDischargedWh += -batteryPowerW * dtHoursBatt;
           }
       }
       lastBatteryEnergyUpdate = nowMsBatt;

       // Потребление: канал 0 (12В квартира) + канал 1 (5В нагрузка), интегрирование в Втч за сутки
       float loadPowerW = fabs(battery.getVoltage(0) * loadMainCurrent) + fabs(battery.getVoltage(1) * loadDC_Current);
       uint32_t nowMsLoad = millis();
       if (lastLoadEnergyUpdate != 0) {
           float dtHoursLoad = (nowMsLoad - lastLoadEnergyUpdate) / 3600000.0f;
           dailyConsumedWh += loadPowerW * dtHoursLoad;
       }
       lastLoadEnergyUpdate = nowMsLoad;

       // Автоматическая подзарядка от сети при разряженной батарее
       updateMainsCharger();
    }else{
      if (battery.begin()) inaError = false;
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

struct BatteryStep {
    float voltage;
    int percentage;
};

// Таблица для гелевого АКБ 12В
const BatteryStep batteryTable[] = {
    {12.70, 100},
    {12.50, 90},
    {12.42, 80},
    {12.32, 70},
    {12.20, 60},
    {12.06, 50},
    {11.90, 40},
    {11.75, 30},
    {11.58, 20},
    {11.31, 10},
    {10.50, 0}
};

const int tableSize = sizeof(batteryTable) / sizeof(BatteryStep);

int getBatteryLevel(float volts) {
    // 1. Проверки на крайние значения
    if (volts >= batteryTable[0].voltage) return 100;
    if (volts <= batteryTable[tableSize - 1].voltage) return 0;

    // 2. Поиск нужного интервала в таблице
    for (int i = 0; i < tableSize - 1; i++) {
        if (volts <= batteryTable[i].voltage && volts > batteryTable[i+1].voltage) {
            
            // 3. Линейная интерполяция между двумя точками
            float vHigh = batteryTable[i].voltage;
            float vLow = batteryTable[i+1].voltage;
            int pHigh = batteryTable[i].percentage;
            int pLow = batteryTable[i+1].percentage;

            return pLow + (int)((volts - vLow) * (pHigh - pLow) / (vHigh - vLow));
        }
    }
    return 0;
}

// Обратная задача к getBatteryLevel(): по проценту заряда находит соответствующее напряжение
// по таблице batteryTable. Используется автоматикой подзарядки, чтобы не хранить
// отдельную константу напряжения, а вычислять её из настраиваемого порога по SOC.
float getVoltageForLevel(float percent) {
    if (percent >= batteryTable[0].percentage) return batteryTable[0].voltage;
    if (percent <= batteryTable[tableSize - 1].percentage) return batteryTable[tableSize - 1].voltage;

    for (int i = 0; i < tableSize - 1; i++) {
        if (percent <= batteryTable[i].percentage && percent > batteryTable[i+1].percentage) {
            float pHigh = batteryTable[i].percentage;
            float pLow = batteryTable[i+1].percentage;
            float vHigh = batteryTable[i].voltage;
            float vLow = batteryTable[i+1].voltage;

            return vLow + (percent - pLow) * (vHigh - vLow) / (pHigh - pLow);
        }
    }
    return batteryTable[tableSize - 1].voltage;
}


// ---------- HTTP ----------

void setupServer() {

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
    request->send(200, "text/html", htmlPage);  
  });

  server.on("/m", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", mobilePage);
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", settingsPage);
  });

  // Чтение/изменение настроек автоподзарядки. Без параметров — просто отдаёт текущие значения.
  // При передаче любого из параметров — обновляет соответствующее значение и сохраняет всё в NVS.
  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    bool changed = false;

    if (request->hasParam("chemistry")) {
        String v = request->getParam("chemistry")->value();
        if ((v == "gel" || v == "lfp") && v != BATTERY_CHEMISTRY) {
            BATTERY_CHEMISTRY = v;
            saveBatteryChemistry();
            loadChemistrySettings(BATTERY_CHEMISTRY); // подтягиваем сохранённые (или дефолтные) значения для новой химии
            Serial.printf("Тип аккумулятора переключён на: %s\n", BATTERY_CHEMISTRY.c_str());
        }
    }

    if (request->hasParam("socLow")) {
        float v = request->getParam("socLow")->value().toFloat();
        if (v >= 0 && v <= 100) { BATTERY_LOW_SOC_THRESHOLD = v; changed = true; }
    }
    if (request->hasParam("loadSoc")) {
        float v = request->getParam("loadSoc")->value().toFloat();
        if (v >= 0 && v <= 100) { HIGH_LOAD_SOC_THRESHOLD = v; changed = true; }
    }
    if (request->hasParam("mainsV")) {
        int v = request->getParam("mainsV")->value().toInt();
        if (v >= 0 && v <= 400) { MAINS_VOLTAGE_ON_THRESHOLD = v; changed = true; }
    }
    if (request->hasParam("solarNeg")) {
        float v = request->getParam("solarNeg")->value().toFloat();
        if (v >= 0) { SOLAR_POWER_NEGLIGIBLE_W = v; changed = true; }
    }
    if (request->hasParam("loadOn")) {
        float v = request->getParam("loadOn")->value().toFloat();
        if (v >= 0) { LOAD_CURRENT_ON_THRESHOLD = v; changed = true; }
    }
    if (request->hasParam("loadOff")) {
        float v = request->getParam("loadOff")->value().toFloat();
        if (v >= 0) { LOAD_CURRENT_OFF_THRESHOLD = v; changed = true; }
    }
    if (request->hasParam("loadOffDelay")) {
        float v = request->getParam("loadOffDelay")->value().toFloat();
        if (v >= 0) { LOAD_CURRENT_OFF_DELAY_S = v; changed = true; }
    }
    if (request->hasParam("offSuppMin")) {
        long v = request->getParam("offSuppMin")->value().toInt();
        if (v >= 0) { MANUAL_OFF_SUPPRESS_MS = (uint32_t)v * 60000UL; changed = true; }
    }
    if (request->hasParam("lfpCapAh")) {
        float v = request->getParam("lfpCapAh")->value().toFloat();
        if (v > 0) {
            LFP_CAPACITY_AH = v;
            saveLfpCapacity();
        }
    }
    if (request->hasParam("autoEnabled")) {
        String v = request->getParam("autoEnabled")->value();
        bool newVal = (v == "1" || v == "true");
        if (newVal != AUTOMATION_ENABLED) {
            AUTOMATION_ENABLED = newVal;
            saveAutomationEnabled();
            Serial.printf("Автоматика подзарядки: %s\n", AUTOMATION_ENABLED ? "ВКЛЮЧЕНА" : "ВЫКЛЮЧЕНА");
            updateMainsCharger(); // применить сразу (например, погасить реле, если оно было включено автоматикой)
        }
    }
    if (request->hasParam("mainsPrio")) {
        String v = request->getParam("mainsPrio")->value();
        bool newVal = (v == "1" || v == "true");
        if (newVal != MAINS_PRIORITY_ENABLED) {
            MAINS_PRIORITY_ENABLED = newVal;
            saveMainsPriority();
            Serial.printf("Приоритет сети: %s\n", MAINS_PRIORITY_ENABLED ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН");
        }
    }
    if (request->hasParam("mainsPrioTargetSoc")) {
        float v = request->getParam("mainsPrioTargetSoc")->value().toFloat();
        if (v >= 0 && v <= 100 && v != MAINS_PRIORITY_TARGET_SOC) {
            MAINS_PRIORITY_TARGET_SOC = v;
            saveMainsPriorityParams();
        }
    }
    if (request->hasParam("mainsPrioMemoryH")) {
        float v = request->getParam("mainsPrioMemoryH")->value().toFloat();
        if (v >= 0 && v != MAINS_PRIORITY_MEMORY_HOURS) {
            MAINS_PRIORITY_MEMORY_HOURS = v;
            saveMainsPriorityParams();
        }
    }
    if (request->hasParam("cellWarnMv")) {
        float v = request->getParam("cellWarnMv")->value().toFloat();
        if (v >= 0) {
            CELL_IMBALANCE_WARN_MV = v;
            saveCellImbalanceThresholds();
        }
    }
    if (request->hasParam("cellCritMv")) {
        float v = request->getParam("cellCritMv")->value().toFloat();
        if (v >= 0) {
            CELL_IMBALANCE_CRIT_MV = v;
            saveCellImbalanceThresholds();
        }
    }

    if (changed) {
        if (LOAD_CURRENT_ON_THRESHOLD < LOAD_CURRENT_OFF_THRESHOLD) {
            request->send(400, "application/json", "{\"error\":\"порог включения по току должен быть не меньше порога выключения\"}");
            return;
        }
        saveChemistrySettings(BATTERY_CHEMISTRY);
        Serial.println("Настройки автоподзарядки обновлены через /api/settings");
    }

    JsonDocument doc;
    doc["chemistry"] = BATTERY_CHEMISTRY;
    doc["socLow"] = BATTERY_LOW_SOC_THRESHOLD;
    if (BATTERY_CHEMISTRY == "gel") {
        doc["socLowVoltage"] = getVoltageForLevel(BATTERY_LOW_SOC_THRESHOLD);
    }
    doc["loadSoc"] = HIGH_LOAD_SOC_THRESHOLD;
    doc["mainsV"] = MAINS_VOLTAGE_ON_THRESHOLD;
    doc["solarNeg"] = SOLAR_POWER_NEGLIGIBLE_W;
    doc["loadOn"] = LOAD_CURRENT_ON_THRESHOLD;
    doc["loadOff"] = LOAD_CURRENT_OFF_THRESHOLD;
    doc["loadOffDelay"] = LOAD_CURRENT_OFF_DELAY_S;
    doc["offSuppMin"] = MANUAL_OFF_SUPPRESS_MS / 60000UL;
    doc["lfpCapAh"] = LFP_CAPACITY_AH;
    doc["autoEnabled"] = AUTOMATION_ENABLED;
    doc["mainsPrio"] = MAINS_PRIORITY_ENABLED;
    doc["mainsPrioTargetSoc"] = MAINS_PRIORITY_TARGET_SOC;
    doc["mainsPrioMemoryH"] = MAINS_PRIORITY_MEMORY_HOURS;
    doc["mainsPrioActive"] = mainsPriorityActive;
    doc["cellWarnMv"] = CELL_IMBALANCE_WARN_MV;
    doc["cellCritMv"] = CELL_IMBALANCE_CRIT_MV;
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  server.on("/api/charger", HTTP_GET, [](AsyncWebServerRequest *request){ 
    request->send(200, "text/html", chargeToggle());  
  });

  server.on("/bms", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", bmsPage);
  });

  server.on("/api/bms-data", HTTP_GET, [](AsyncWebServerRequest *request){
    JsonDocument doc;
    doc["valid"] = bms.isDataValid();
    doc["voltage"] = bms.getVoltage();
    doc["current"] = bms.getCurrent();
    doc["soc"] = bms.getSOC();
    doc["cycles"] = bms.getCycles();
    doc["powerTemp"] = bms.getPowerTemp();
    doc["boxTemp"] = bms.getBoxTemp();
    doc["batteryTemp"] = bms.getBatteryTemp();
    doc["numCells"] = bms.getNumCells();
    doc["cellDelta"] = bms.getCellVoltageDelta();
    doc["lowestCell"] = bms.getLowestCellVoltage();
    doc["highestCell"] = bms.getHighestCellVoltage();
    doc["chargingEnabled"] = bms.isChargingEnabled();
    doc["dischargingEnabled"] = bms.isDischargingEnabled();
    doc["isCharging"] = bms.isCharging();
    doc["isDischarging"] = bms.isDischarging();
    doc["isBalancing"] = bms.isBalancing();
    doc["swVersion"] = bms.getSoftwareVersion();
    doc["deviceInfo"] = bms.getDeviceInfo();

    // Статус одним словом: Idle/Charge/Discharge
    if (bms.isCharging()) {
        doc["status"] = "charge";
    } else if (bms.isDischarging()) {
        doc["status"] = "discharge";
    } else {
        doc["status"] = "idle";
    }

    // Мощность (Вт) — положительная при заряде, отрицательная при разряде (та же конвенция, что и у тока)
    doc["power"] = bms.getVoltage() * bms.getCurrent();

    // Оставшаяся ёмкость и время до полного заряда/разряда — считаем из SOC и вручную заданной
    // ёмкости пакета (LFP_CAPACITY_AH на /settings), так как в протоколе нет достоверно
    // подтверждённого поля для "остатка в Ач" напрямую.
    float remainingAh = LFP_CAPACITY_AH * bms.getSOC() / 100.0f;
    doc["remainingAh"] = remainingAh;
    doc["capacityAh"] = LFP_CAPACITY_AH;

    float current = bms.getCurrent();
    if (bms.isDataValid() && fabs(current) > 0.05f) {
        float hours;
        if (current < 0) {
            // разряд: время до пустого
            hours = remainingAh / fabs(current);
        } else {
            // заряд: время до полной ёмкости
            hours = (LFP_CAPACITY_AH - remainingAh) / current;
        }
        doc["timeLeftMinutes"] = (int)(hours * 60.0f);
    } else {
        doc["timeLeftMinutes"] = nullptr; // ток около нуля или нет связи — время не считается
    }

    // Тревоги: битовое поле alarmStatus без достоверно подтверждённой расшифровки битов —
    // показываем факт наличия/отсутствия и сырое значение для справки
    uint16_t alarm = bms.getAlarmStatus();
    doc["hasAlarm"] = (alarm != 0);
    doc["alarmRaw"] = alarm;

    JsonArray cells = doc["cells"].to<JsonArray>();
    uint8_t n = bms.getNumCells();
    for (uint8_t i = 0; i < n; i++) {
        cells.add(bms.getCellVoltage(i));
    }

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // Разрешение/запрет заряда в JK-BMS (MOS заряда) по UART: /api/bms/charge?enable=1|0
  server.on("/api/bms/charge", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasParam("enable")) {
      request->send(400, "application/json", "{\"error\":\"укажите ?enable=1 или ?enable=0\"}");
      return;
    }
    if (!bms.isDataValid()) {
      request->send(503, "application/json", "{\"error\":\"нет связи с BMS\"}");
      return;
    }
    String v = request->getParam("enable")->value();
    bool enable = (v == "1" || v == "true");
    bmsChargeCommand = enable ? 1 : 0;
    request->send(200, "application/json", String("{\"requested\":") + (enable ? "true" : "false") + "}");
  });

  // Ручная калибровка кулонометра, например: /api/battery/calibrate?soc=80
  server.on("/api/battery/calibrate", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasParam("soc")) {
      request->send(400, "text/plain", "укажите ?soc=<0-100>");
      return;
    }
    float soc = request->getParam("soc")->value().toFloat();
    coulomb.setSocPercent(soc);
    request->send(200, "text/plain", "OK: SOC установлен в " + String(soc, 1) + "%");
  });

  // API эндпоинт
  server.on("/api/solar-data", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    
    // Секция SOLAR
    JsonObject solar = doc["solar"].to<JsonObject>();
    const char* mppts[] = {"mppt-1", "mppt-2", "mppt-3", "mppt-4"};
    
    VictronData data;
    for (int i = 0; i < 4; i++) {
      data = scanner.getData(i);
      JsonObject mppt = solar[mppts[i]].to<JsonObject>();
      mppt["voltage"] = data.pvV;
      mppt["power"] = data.pvP;
      mppt["current"] = data.batteryI;
      mppt["status"] = (i == 3) ? "on" : "off";
      mppt["generation"] = data.pvV > 12 ? "on" : "off";
      mppt["state"] = data.state;
    }

    // Секция BMS
    JsonObject bms = doc["bms"].to<JsonObject>();
    JsonObject bms1 = bms["bms-1"].to<JsonObject>();
    bms1["voltage"] = String(batteryVoltage, 2);
    bms1["current"] = String(batteryCurrent, 2);
    bms1["level_percent"] = String(getBatterySocPercent(), 1);
    bms1["state"] = "charge";
    bms1["status"] = "normal";
    // Разбалансировка ячеек имеет смысл только для LFP с валидными данными с BMS — иначе отдаём 0,
    // чтобы индикатор на главной странице оставался скрытым
    bms1["cellDeltaMv"] = getLfpCellDeltaMv();
    bms1["cellWarnMv"] = CELL_IMBALANCE_WARN_MV;
    bms1["cellCritMv"] = CELL_IMBALANCE_CRIT_MV;

    // Секция BUS
    JsonObject bus = doc.createNestedObject("bus");
    bus["current"] = String(data.batteryI, 2);
    bus["voltage"] = String(data.batteryV, 2);

    // Секция Charger
    JsonObject charger = doc.createNestedObject("charger");
    charger["current"] = data.batteryI;
    charger["state"] = chargerState;
    charger["autoChargeNeeded"] = chargeNeeded;
    charger["mainsPresent"] = mainsPresent;
    charger["solarNegligible"] = solarNegligible;
    charger["manualRequested"] = manualChargeRequested;
    charger["manualOffSuppressed"] = (manualOverrideOffUntil != 0);
    charger["highLoadCharge"] = highLoadChargeActive;
    charger["automationEnabled"] = AUTOMATION_ENABLED;
    charger["mainsPriorityEnabled"] = MAINS_PRIORITY_ENABLED;
    charger["mainsPriorityActive"] = mainsPriorityActive;
    // Какая причина включения реле сейчас: реле включается только если wantCharge && есть сеть, а wantCharge =
    // ручной запрос ИЛИ автоматика — поэтому при включённом реле без ручного запроса причина всегда автоматика
    charger["source"] = (chargerState == "on") ? (manualChargeRequested ? "manual" : "auto") : "none";

    // Секция Load
    JsonObject load = doc.createNestedObject("load");
    load["dc"] = String(loadDC_Current, 3);
    load["main"] = String(loadMainCurrent, 2);
    load["mainsVoltage"] = inputACVoltage > 30 ? inputACVoltage : 0 ;
    load["mainsStatus"] = inputACVoltage > 150 ? "on" : "off";

    // Секция выработки/потребления за сутки
    JsonObject energy = doc.createNestedObject("energy");
    energy["today_wh"] = String(dailyEnergyWh, 1);
    energy["consumed_today_wh"] = String(dailyConsumedWh, 1);
    energy["battery_charged_today_wh"] = String(dailyBatteryChargedWh, 1);
    energy["battery_discharged_today_wh"] = String(dailyBatteryDischargedWh, 1);
    energy["day"] = getTodayDateString();

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  ElegantOTA.begin(&server);

  server.begin();
  Serial.println("HTTP сервер запущен");
}

// ---------- WiFi ----------
void setupWiFi() {
  const char* ssid = "nastenka";
  const char* pass = "19141914";

  Serial.println("Подключение к WiFi...");
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi подключен!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 2 * 3600;  // Киев +2 часа относительно UTC зимой
const int   daylightOffset_sec = 3600; // Летнее время +1 час

void setupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nВремя синхронизировано!");
  lastNtpResync = millis();
}

// Периодически повторно синхронизирует часы с NTP-сервером, чтобы граница суток определялась точно
void syncTimeIfNeeded() {
    if (millis() - lastNtpResync > NTP_RESYNC_INTERVAL_MS) {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        lastNtpResync = millis();
        Serial.println("Время пересинхронизировано с NTP");
    }
}

// Initialize BMS communication (RX=16, TX=17 for ESP32 Serial2 Port)
void setupBMS() {
    Serial2.setRxBufferSize(512); // Важно для длинных пакетов JK
    Serial2.begin(115200, SERIAL_8N1, 16, 17);
    bms.begin(115200);
    Serial.println("JK-BMS сканер запущен!");
}

void setupDNS(){
    if (!MDNS.begin("home")) { 
       Serial.println("Ошибка запуска mDNS");
    } else {
       Serial.println("mDNS запущен! Заходи на http://home.local/");
    }
}

//*************************************************
//            Начальная инициализация            //
//*************************************************
void setup() {

  Serial.begin(115200);

  // Загрузка типа аккумулятора и настроек автоподзарядки из энергонезависимой памяти
  loadBatteryChemistry();
  loadChemistrySettings(BATTERY_CHEMISTRY);

  // Зарядка
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  setupWiFi();
  setupTime();
  setupServer();
  setupDNS();
  setupBMS();

  voltageSensor.setSensitivity(SENSITIVITY);
  analogSetAttenuation(ADC_6db);

Serial.println("Adafruit INA3221 simple test");

  // Initialize the INA3221
  if (!battery.begin()) {
        Serial.println("Ошибка: INA3221 не обнаружен!");
        inaError = true;
    }
  Serial.println("Мониторинг батарей запущен.");

  // Стартуем кулонометр и счётчики выработки/потребления: сначала пробуем синхронизироваться
  // со своим сервером (в отдельной задаче, чтобы не блокировать loopTask), при неудаче/таймауте —
  // кулонометр стартует от табличной оценки по напряжению, счётчики — с нуля
  float startVoltage = inaError ? 12.0 : battery.getVoltage(2);

  xTaskCreatePinnedToCore(taskInitialServerSync, "TaskServerSync", 16384, NULL, 1, NULL, 0);
  Serial.println("Ожидание синхронизации с сервером...");
  uint32_t syncWaitStart = millis();
  while (!serverSyncDone && millis() - syncWaitStart < 10000) { // не более 10 секунд ожидания, чтобы не зависнуть при недоступной сети
      delay(100);
  }

  if (serverSyncDone && serverSyncResult.found) {
      Serial.printf("Синхронизировано с сервером: SOC %.2f%%\n", serverSyncResult.soc);
      coulomb.begin(serverSyncResult.soc);
      dailyMpptWh[0] = serverSyncResult.mppt1Wh;
      dailyMpptWh[1] = serverSyncResult.mppt2Wh;
      dailyMpptWh[2] = serverSyncResult.mppt3Wh;
      dailyMpptWh[3] = serverSyncResult.mppt4Wh;
      dailyEnergyWh = dailyMpptWh[0] + dailyMpptWh[1] + dailyMpptWh[2] + dailyMpptWh[3];
      dailyConsumedWh = serverSyncResult.consumedWh;
      dailyBatteryChargedWh = serverSyncResult.batteryChargedWh;
      dailyBatteryDischargedWh = serverSyncResult.batteryDischargedWh;
      Serial.printf("Восстановлено: выработка %.1f Втч, потребление %.1f Втч\n", dailyEnergyWh, dailyConsumedWh);
  } else {
      Serial.println(serverSyncDone ? "Нет данных за сегодня на сервере, стартуем с нуля/по таблице напряжения" : "Таймаут ожидания сервера, стартуем по таблице напряжения");
      coulomb.begin(getBatteryLevel(startVoltage));
  }

  // ------- Запуск задач -------
  xTaskCreatePinnedToCore(taskScanner, "TaskScanner", 8192, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskCore1, "TaskCore1", 8192, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskBMS, "TaskBMS", 6144, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskServerLog, "TaskServerLog", 12288, NULL, 1, NULL, 0);
}

void loop() {
  //server.handleClient();
  ElegantOTA.loop();          // Обязательно! Для обновлений
}

//************************************************
//         Ручное управление зарядкой           //
//************************************************
String chargeToggle(){
  if (chargerState == "on") {
      // Пользователь гасит текущий заряд — подавляем автовключение на 1 час
      manualChargeRequested = false;
      manualOverrideOffUntil = millis() + MANUAL_OFF_SUPPRESS_MS;
      Serial.println("Ручное выключение заряда от сети: автоматика приостановлена на 1 час");
  } else {
      // Пользователь запрашивает принудительный заряд — идёт до float, независимо от SOC/солнца
      manualChargeRequested = true;
      manualOverrideOffUntil = 0; // снимаем возможное прежнее подавление
      Serial.println("Ручное включение заряда от сети: будет идти до достижения float");
  }
  updateMainsCharger(); // применить немедленно, не дожидаясь очередного тика taskCore1
  return chargerState;
}

// Автоматическое управление подзарядкой от сети.
// Вызывается из taskCore1 каждые 2 секунды, а также сразу после нажатия кнопки на странице.
void updateMainsCharger() {
    // 0. Наличие сети и уровень солнечной генерации — считаем ЗАРАНЕЕ решений о запуске,
    // потому что зарядное устройство включено параллельно панели на вход MPPT —
    // если его включить, MPPT начнёт показывать это как "солнце". Если проверять solarNegligible
    // на каждом тике и пока зарядка уже идёт, получится самовыключение: включили → MPPT увидел
    // "солнце" от зарядного → solarNegligible стал false → тут же выключили обратно.
    // Поэтому solarNegligible дальше используется ТОЛЬКО в момент СТАРТА зарядки, не при её продолжении.
    mainsPresent = inputACVoltage > MAINS_VOLTAGE_ON_THRESHOLD;

    // Фиксируем момент пропадания сети (переход present→absent) — это и есть “блекаут”, который
    // запускает окно повышенного приоритета (если он включён в настройках).
    static bool prevMainsPresent = true; // при первом запуске не считаем это блекаутом
    if (prevMainsPresent && !mainsPresent) {
        lastMainsLossTime = millis();
        Serial.println("Сеть пропала — зафиксирован момент блекаута для окна повышенного приоритета");
    }
    prevMainsPresent = mainsPresent;

    // Окно повышенного приоритета: активно, если функция включена и с момента последнего зафиксированного
    // пропадания прошло меньше заданного времени памяти
    mainsPriorityActive = MAINS_PRIORITY_ENABLED && lastMainsLossTime != 0 &&
        (millis() - lastMainsLossTime) < (uint32_t)(MAINS_PRIORITY_MEMORY_HOURS * 3600000.0f);

    long totalPvPower = 0;
    for (int i = 0; i < 4; i++) {
        totalPvPower += scanner.getData(i).pvP;
    }
    solarNegligible = totalPvPower < SOLAR_POWER_NEGLIGIBLE_W;

    // Эффективный порог активации подзарядки: обычный экономный, либо повышенный целевой
    // в окне приоритета сети
    float effectiveLowSocThreshold = mainsPriorityActive ? MAINS_PRIORITY_TARGET_SOC : BATTERY_LOW_SOC_THRESHOLD;

    // 1. Активируем потребность в подзарядке, если батарея разряжена И солнце сейчас мизерное.
    // Дальше chargeNeeded держится независимо от текущего solarNegligible — см. комментарий выше.
    // При выключенной автоматике новые срабатывания не заводятся вообще.
    if (AUTOMATION_ENABLED && !chargeNeeded) {
        bool batteryDischarged;
        if (BATTERY_CHEMISTRY == "lfp") {
            // У LFP плоская кривая заряда — таблица для свинца тут бесполезна, ориентируемся
            // только на SOC от самого JK-BMS
            batteryDischarged = getBatterySocPercent() < effectiveLowSocThreshold;
        } else {
            batteryDischarged = getBatterySocPercent() < effectiveLowSocThreshold || batteryVoltage < getVoltageForLevel(effectiveLowSocThreshold);
        }
        if (batteryDischarged && solarNegligible) {
            chargeNeeded = true;
            Serial.println(mainsPriorityActive ? "Окно приоритета сети: батарея ниже повышенного целевого порога и солнца мало — требуется подзарядка от сети" : "Батарея разряжена и солнца мало — требуется подзарядка от сети");
        }
    }

    // 2. Проверяем float-статус любого из 4 MPPT — признак полного заряда
    bool anyFloat = false;
    for (int i = 0; i < 4; i++) {
        if (scanner.getData(i).state == 5) { // 5 = Float у Victron
            anyFloat = true;
            break;
        }
    }

    static bool wasFloat = false;
    if (anyFloat) {
        // Автокалибровка кулонометра на 100% по float-статусу контроллера
        coulomb.setSocPercent(100.0);
        if (!wasFloat) {
            Serial.println("MPPT в режиме float: кулонометр синхронизирован на 100%");
            wasFloat = true;
        }
        if (chargeNeeded || manualChargeRequested) {
            chargeNeeded = false;
            manualChargeRequested = false;
            Serial.println("Батарея полностью заряжена, подзарядка от сети по этим причинам отключена");
            // highLoadChargeActive НЕ трогаем: пока ток нагрузки высокий, подзарядка от сети
            // продолжается, чтобы не гонять батарею циклами заряд/разряд — MPPT сам следит за перезарядом.
        }
    } else {
        wasFloat = false;
    }

    // 3. Гистерезис по току нагрузки — включает подзарядку при высоком потреблении, только
    // если батарея разряжена до порога и солнце мизерное (как и выше — solarNegligible проверяется
    // только при СТАРТЕ). ВЫКЛЮЧАЕТСЯ ТОЛЬКО по спаду тока, выдержанному не менее
    // LOAD_CURRENT_OFF_DELAY_S секунд подряд — восстановление SOC выше HIGH_LOAD_SOC_THRESHOLD в расчёт
    // больше НЕ берётся (именно это раньше давало мгновенное выключение через пару секунд после
    // начала зарядки). Выключается такая подзарядка теперь только по спаду тока, по float
    // (выше) или по кнопке ручного отключения.
    float totalLoadCurrent = fabs(loadMainCurrent) + fabs(loadDC_Current);
    bool batteryLow = getBatterySocPercent() < HIGH_LOAD_SOC_THRESHOLD;

    if (AUTOMATION_ENABLED) {
        if (!highLoadChargeActive) {
            if (totalLoadCurrent >= LOAD_CURRENT_ON_THRESHOLD && batteryLow && solarNegligible) {
                highLoadChargeActive = true;
                loadCurrentOffSince = 0;
                Serial.println("Высокий ток нагрузки при разряженной батарее и мизерном солнце — включаем подзарядку от сети");
            }
        } else {
            if (totalLoadCurrent <= LOAD_CURRENT_OFF_THRESHOLD) {
                if (loadCurrentOffSince == 0) {
                    loadCurrentOffSince = millis();
                } else if ((millis() - loadCurrentOffSince) >= (uint32_t)(LOAD_CURRENT_OFF_DELAY_S * 1000.0f)) {
                    highLoadChargeActive = false;
                    loadCurrentOffSince = 0;
                    Serial.println("Подзарядка от сети по высокой нагрузке отключена (ток ниже порога достаточно долго)");
                }
            } else {
                loadCurrentOffSince = 0; // ток снова выше порога выключения — сбрасываем таймер задержки
            }
        }
    }

    // 4. Подавление автовключения после ручного выключения — истекает через час
    bool manualOffActive = (manualOverrideOffUntil != 0);
    if (manualOffActive && (int32_t)(millis() - manualOverrideOffUntil) >= 0) {
        manualOffActive = false;
        manualOverrideOffUntil = 0;
        Serial.println("Истёк час после ручного выключения — автоматика подзарядки снова активна");
    }

    // 5. Итоговое решение: ручной запрос форсирует заряд (игнорируя SOC/солнце/ток),
    // автоматические причины (chargeNeeded или highLoadChargeActive) — только если не подавлены
    // ручным выключением; solarNegligible тут уже НЕ проверяется повторно (оно уже учтено при
    // активации выше); в обоих случаях нужна сеть
    bool autoWant = AUTOMATION_ENABLED && (highLoadChargeActive || chargeNeeded);
    bool wantCharge = manualChargeRequested || (autoWant && !manualOffActive);
    bool shouldBeOn = wantCharge && mainsPresent;
    bool isOn = (chargerState == "on");

    if (shouldBeOn != isOn) {
        chargerState = shouldBeOn ? "on" : "off";
        digitalWrite(13, shouldBeOn ? HIGH : LOW);
        Serial.printf("Заряд от сети: %s%s\n", chargerState.c_str(), manualChargeRequested ? " (ручной запрос)" : "");
    }
}
