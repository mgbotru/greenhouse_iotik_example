#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiClient.h>
#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <AM2320.h>
#include <BH1750FVI.h>
#include <Adafruit_BMP085_U.h>
#include <ArduinoJson.h>

// Нахождение максимума из двух чисел
#undef max
#define max(a,b) ((a)>(b)?(a):(b))

// Датчики DS18B20
#define DS18B20_1 0
#define DS18B20_2 2
OneWire oneWire1(DS18B20_1);
OneWire oneWire2(DS18B20_2);
DallasTemperature ds_sensor1(&oneWire1);
DallasTemperature ds_sensor2(&oneWire2);

// Точка доступа Wi-Fi
char ssid[] = "IOTIK";
char pass[] = "Terminator812";

// Датчик AM2320
AM2320 am2320;

// Датчик BH1750
BH1750FVI bh1750;

// Датчик BMP085
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

// Датчики влажности почвы и освещенности
#define MOISTURE_1 A0

// Выходы управления устройствами
#define PUMP 12
#define LAMP 13
#define HEATER 14

// Сервомоторы
#define SERVO1_PWM 15
Servo servo_1;

// API key для Blynk
char auth[] = "8ee073f51255401490209506a54a29fd";
IPAddress blynk_ip(139, 59, 206, 133);

// Периоды для асинхронных таймеров
#define AM2320_UPDATE_TIME 1000
#define BMP085_UPDATE_TIME 1000
#define BH1750_UPDATE_TIME 1000
#define DS1_UPDATE_TIME 1000
#define DS2_UPDATE_TIME 1000
#define ANALOG_UPDATE_TIME 1000
#define IOT_UPDATE_TIME 5000
#define BLYNK_UPDATE_TIME 1000

// Счетчики для асинхронных таймеров
long timer_am2320 = 0;
long timer_bmp085 = 0;
long timer_bh1750 = 0;
long timer_analog = 0;
long timer_blynk = 0;
long timer_ds1 = 0;
long timer_ds2 = 0;
long timer_iot = 0;
long timer_auto = 0;

// Состояния управляющих устройств
int pump_state_ptc = 0;
int light_state_ptc = 0;
int window_state_ptc = 0;
int heater_state_ptc = 0;
int pump_state_blynk = 0;
int light_state_blynk = 0;
int window_state_blynk = 0;
int heater_state_blynk = 0;

// Параметры IoT сервера
char iot_server[] = "jrskillsiot.cloud.thingworx.com";
IPAddress iot_address(52, 203, 26, 63);
char appKey[] = "cedebb42-898d-4c47-a94f-fabc402e0b8a";
char thingName[] = "MGBot_Exhibition_Greenhouse";
char serviceName[] = "MGBot_Exhibition_SetParams";

// Параметры сенсоров для IoT сервера
#define sensorCount 9                                     // How many values you will be pushing to ThingWorx
char* sensorNames[] = {"dht11_temp", "dht11_hum", "sensor_light", "ds18b20_temp1", "ds18b20_temp2", "soil_moisture1", "soil_moisture2", "air_press", "gas_conc"};
float sensorValues[sensorCount];
// Номера датчиков
#define dht11_temp     0
#define dht11_hum      1
#define sensor_light   2
#define ds18b20_temp1  3
#define ds18b20_temp2  4
#define soil_moisture1 5
#define soil_moisture2 6
#define air_press      7
#define gas_conc       8

// Максимальное время ожидания ответа от сервера
#define IOT_TIMEOUT1 5000
#define IOT_TIMEOUT2 100

// Таймер ожидания прихода символов с сервера
long timer_iot_timeout = 0;

// Размер приемного буффера
#define BUFF_LENGTH 256

// Приемный буфер
char buff[BUFF_LENGTH] = "";

void setup()
{
  // Инициализация последовательного порта
  Serial.begin(115200);
  delay(1024);
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Инициализация Blynk и Wi-Fi
  Blynk.begin(auth, ssid, pass, blynk_ip, 8442);

  // Инициализация датчиков температуры DS18B20
  ds_sensor1.begin();
  ds_sensor2.begin();

  // Инициализация датчика AM2320
  am2320.begin();

  // Инициализация датчика BH1750
  bh1750.begin();
  bh1750.setMode(Continuously_High_Resolution_Mode);

  // Инициализация датчика BMP085
  if (!bmp.begin())
  {
    Serial.println("Could not find a valid BMP085 sensor!");
  }

  // Инициализация аналогового входа
  pinMode(A0, INPUT);

  // Инициализация выходов устройств
  pinMode(PUMP, OUTPUT);
  pinMode(LAMP, OUTPUT);
  pinMode(HEATER, OUTPUT);

  // Инициализация портов для управления сервомоторами
  servo_1.attach(SERVO1_PWM);
  servo_1.write(90);
  delay(1024);

  // Однократный опрос датчиков
  readSensorAM2320();
  readSensorBMP085();
  readSensorBH1750();
  readSensorDS1();
  readSensorDS2();
  readSensorAnalog();

  // Вывод в терминал данных с датчиков
  printAllSensors();
}

void loop()
{
  // Опрос датчика AM2320
  if (millis() > timer_am2320 + AM2320_UPDATE_TIME)
  {
    readSensorAM2320();
    timer_am2320 = millis();
  }

  // Опрос датчика BMP085
  if (millis() > timer_bmp085 + BMP085_UPDATE_TIME)
  {
    readSensorBMP085();
    timer_bmp085 = millis();
  }

  // Опрос датчика BH1750
  if (millis() > timer_bh1750 + BH1750_UPDATE_TIME)
  {
    readSensorBH1750();
    timer_bh1750 = millis();
  }

  // Опрос датчика DS18B20 №1
  if (millis() > timer_ds1 + DS1_UPDATE_TIME)
  {
    readSensorDS1();
    timer_ds1 = millis();
  }

  // Опрос датчика DS18B20 №2
  if (millis() > timer_ds2 + DS2_UPDATE_TIME)
  {
    readSensorDS2();
    timer_ds2 = millis();
  }

  // Опрос аналоговых датчиков
  if (millis() > timer_analog + ANALOG_UPDATE_TIME)
  {
    readSensorAnalog();
    timer_analog = millis();
  }

  // Вывод данных на сервер IoT
  if (millis() > timer_iot + IOT_UPDATE_TIME)
  {
    sendDataIot();
    timer_iot = millis();
  }

  // Отправка данных Blynk
  if (millis() > timer_blynk + BLYNK_UPDATE_TIME)
  {
    sendBlynk();
    timer_blynk = millis();
  }

  // Опрос сервера Blynk
  Blynk.run();
  delay(10);
}

// Чтение датчика AM2320
void readSensorAM2320()
{
  if (am2320.measure())
  {
    sensorValues[dht11_temp] = am2320.getTemperature();
    sensorValues[dht11_hum] = am2320.getHumidity();
  }
}

// Чтение датчика BH1750
void readSensorBH1750()
{
  sensorValues[sensor_light] = bh1750.getAmbientLight();
}

// Чтение датчика BMP085
void readSensorBMP085()
{
  float t = 0;
  float p = 0;
  sensors_event_t p_event;
  bmp.getEvent(&p_event);
  if (p_event.pressure)
  {
    p = p_event.pressure * 7.5006 / 10;
    bmp.getTemperature(&t);
  }
  sensorValues[air_press] = p;
}

// Чтение датчика DS18B20 №1
void readSensorDS1()
{
  ds_sensor1.requestTemperatures();
  sensorValues[ds18b20_temp1] = ds_sensor1.getTempCByIndex(0);
}

// Чтение датчика DS18B20 №2
void readSensorDS2()
{
  ds_sensor2.requestTemperatures();
  sensorValues[ds18b20_temp2] = ds_sensor2.getTempCByIndex(0);
}

// Чтение аналоговых датчиков
void readSensorAnalog()
{
  // Аналоговые датчики
  sensorValues[soil_moisture1] = analogRead(MOISTURE_1) / 1023.0 * 100.0;
  sensorValues[soil_moisture2] = 0;
  sensorValues[gas_conc] = 0;
}

// Отправка данных в приложение Blynk
void sendBlynk()
{
  Serial.println("Sending data to Blynk...");
  Blynk.virtualWrite(V0, sensorValues[dht11_temp]); delay(50);
  Blynk.virtualWrite(V1, sensorValues[dht11_hum]); delay(50);
  Blynk.virtualWrite(V2, sensorValues[ds18b20_temp1]); delay(50);
  Blynk.virtualWrite(V3, sensorValues[soil_moisture1]); delay(50);
  Blynk.virtualWrite(V4, sensorValues[ds18b20_temp2]); delay(50);
  Blynk.virtualWrite(V5, sensorValues[soil_moisture2]); delay(50);
  Blynk.virtualWrite(V6, sensorValues[sensor_light]); delay(50);
  Serial.println("Data successfully sent!");
}

// Управление освещением с Blynk
BLYNK_WRITE(V7)
{
  light_state_blynk = param.asInt();
  Serial.print("Light power: ");
  Serial.println(light_state_blynk);
  analogWrite(LAMP, max(light_state_ptc, light_state_blynk) * 2.55);
}

// Управление вентиляцией с Blynk
BLYNK_WRITE(V8)
{
  window_state_blynk = constrain((param.asInt() + 90), 90, 135);
  Serial.print("Window motor angle: ");
  Serial.println(window_state_blynk);
  servo_1.write(max(window_state_ptc, window_state_blynk));
  delay(512);
}

// Управление помпой с Blynk
BLYNK_WRITE(V9)
{
  pump_state_blynk = param.asInt();
  Serial.print("Pump power: ");
  Serial.println(pump_state_blynk);
  digitalWrite(PUMP, pump_state_ptc | pump_state_blynk);
}

// Подключение к серверу IoT ThingWorx
void sendDataIot()
{
  // Подключение к серверу
  WiFiClient client;
  Serial.println("Connecting to IoT server...");
  if (client.connect(iot_address, 80))
  {
    // Проверка установления соединения
    if (client.connected())
    {
      // Отправка заголовка сетевого пакета
      Serial.println("Sending data to IoT server...\n");
      Serial.print("POST /Thingworx/Things/");
      client.print("POST /Thingworx/Things/");
      Serial.print(thingName);
      client.print(thingName);
      Serial.print("/Services/");
      client.print("/Services/");
      Serial.print(serviceName);
      client.print(serviceName);
      Serial.print("?appKey=");
      client.print("?appKey=");
      Serial.print(appKey);
      client.print(appKey);
      Serial.print("&method=post&x-thingworx-session=true");
      client.print("&method=post&x-thingworx-session=true");
      // Отправка данных с датчиков
      for (int idx = 0; idx < sensorCount; idx ++)
      {
        Serial.print("&");
        client.print("&");
        Serial.print(sensorNames[idx]);
        client.print(sensorNames[idx]);
        Serial.print("=");
        client.print("=");
        Serial.print(sensorValues[idx]);
        client.print(sensorValues[idx]);
      }
      // Закрываем пакет
      Serial.println(" HTTP/1.1");
      client.println(" HTTP/1.1");
      Serial.println("Accept: application/json");
      client.println("Accept: application/json");
      Serial.print("Host: ");
      client.print("Host: ");
      Serial.println(iot_server);
      client.println(iot_server);
      Serial.println("Content-Type: application/json");
      client.println("Content-Type: application/json");
      Serial.println();
      client.println();

      // Ждем ответа от сервера
      timer_iot_timeout = millis();
      while ((client.available() == 0) && (millis() < timer_iot_timeout + IOT_TIMEOUT1));

      // Выводим ответ о сервера, и, если медленное соединение, ждем выход по таймауту
      int iii = 0;
      bool currentLineIsBlank = true;
      bool flagJSON = false;
      timer_iot_timeout = millis();
      while ((millis() < timer_iot_timeout + IOT_TIMEOUT2) && (client.connected()))
      {
        while (client.available() > 0)
        {
          char symb = client.read();
          Serial.print(symb);
          if (symb == '{')
          {
            flagJSON = true;
          }
          else if (symb == '}')
          {
            flagJSON = false;
          }
          if (flagJSON == true)
          {
            buff[iii] = symb;
            iii ++;
          }
          timer_iot_timeout = millis();
        }
      }
      buff[iii] = '}';
      buff[iii + 1] = '\0';
      Serial.println(buff);
      // Закрываем соединение
      client.stop();

      // Расшифровываем параметры
      StaticJsonBuffer<BUFF_LENGTH> jsonBuffer;
      JsonObject& json_array = jsonBuffer.parseObject(buff);
      if (json_array.success())
      {
        pump_state_ptc = json_array["pump_state"];
        window_state_ptc = json_array["window_state"];
        light_state_ptc = json_array["light_state"];
        if (window_state_ptc)
        {
          window_state_ptc = 120;
        }
        else
        {
          window_state_ptc = 90;
        }
      }
      Serial.println("Pump state:   " + String(pump_state_ptc));
      Serial.println("Light state:  " + String(light_state_ptc));
      Serial.println("Window state: " + String(window_state_ptc));
      Serial.println();

      // Делаем управление устройствами
      controlDevices();

      Serial.println("Packet successfully sent!");
      Serial.println();
    }
  }
}

// Управление исполнительными устройствами
void controlDevices()
{
  // Форточка
  servo_1.write(max(window_state_ptc, window_state_blynk));
  delay(512);
  // Освещение
  analogWrite(LAMP, max(light_state_ptc, light_state_blynk) * 2.55);
  // Помпа
  digitalWrite(PUMP, pump_state_ptc | pump_state_blynk);
}

// Print sensors data to terminal
void printAllSensors()
{
  for (int i = 0; i < sensorCount; i++)
  {
    Serial.print(sensorNames[i]);
    Serial.print(" = ");
    Serial.println(sensorValues[i]);
  }
  Serial.println();
}

