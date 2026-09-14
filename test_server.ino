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

AsyncWebServer server(80);

JKBMSInterface bms(&Serial2);

String chargerState = "off";

const int pins[] = {25, 32, 33, 26};
VictronScanner scanner(Serial1, pins, 4);

#define SENSITIVITY 500.0f
ZMPT101B voltageSensor(35, 50.0);

int inputACVoltage = 0;
float solarMainVoltage = 0;
bool inaError = false;

// Создаем объект с шунтами: R050, R100, R010
BatteryMonitor battery(0.05, 0.1, 0.01);

float batteryVoltage = 0;
float batteryCurrent = 0;
float loadDC_Current = 0;
float loadMainCurrent = 0;

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

    inputACVoltage = voltageSensor.getRmsVoltage();
  
    // Усредняем значения для фильтрации шумов
 //   for (int i = 0; i < 400; i++) {
 //     sum += analogRead(34);
 //   }
 //   solarMainVoltage = (float)sum / 400 / 0.0714;
 //   sum = 0;

    if(inaError == false){
       batteryVoltage = battery.getVoltage(2);
       batteryCurrent = battery.getCurrentAmps(2);
       loadDC_Current = battery.getCurrentAmps(1);
       loadMainCurrent = battery.getCurrentAmps(0);
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


// ---------- HTTP ----------

void setupServer() {

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
    request->send(200, "text/html", htmlPage);  
  });

  server.on("/api/charger", HTTP_GET, [](AsyncWebServerRequest *request){ 
    request->send(200, "text/html", chargeToggle());  
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
    bms1["voltage"] = String(data.batteryV);//String(batteryVoltage, 2);  //data.batteryV;
    bms1["current"] = String(batteryCurrent, 2);
    bms1["level_percent"] = getBatteryLevel(data.batteryV);//batteryVoltage);
    bms1["state"] = "charge";
    bms1["status"] = "normal";

    // Секция BUS
    JsonObject bus = doc.createNestedObject("bus");
    bus["current"] = String(data.batteryI, 2);
    bus["voltage"] = String(data.batteryV, 2);

    // Секция Charger
    JsonObject charger = doc.createNestedObject("charger");
    charger["current"] = data.batteryI;
    charger["state"] = chargerState;

    // Секция Load
    JsonObject load = doc.createNestedObject("load");
    load["dc"] = String(loadDC_Current, 3);
    load["main"] = String(loadMainCurrent, 2);
    load["mainsVoltage"] = inputACVoltage > 30 ? inputACVoltage : 0 ;
    load["mainsStatus"] = inputACVoltage > 150 ? "on" : "off";

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

  // ------- Запуск задач -------
  xTaskCreatePinnedToCore(taskScanner, "TaskScanner", 8192, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskCore1, "TaskCore1", 8192, NULL, 1, NULL, 1);
}

void loop() {
  //server.handleClient();
  ElegantOTA.loop();          // Обязательно! Для обновлений
}

//************************************************
//         Ручное управление зарядкой           //
//************************************************
String chargeToggle(){
  if(chargerState == "on"){
      chargerState = "off";
      digitalWrite(13, LOW);
  }else{
      chargerState = "on"; 
      digitalWrite(13, HIGH);
  }
  return chargerState;
}
