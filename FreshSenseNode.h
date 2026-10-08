#pragma once

// Firmware del dispositivo FreshSense.
// Lo que hace sale del informe, rama develop:
//   4.2.7 IoT Monitoring: la senal fisica es temperatura, humedad y etileno.
//   4.2.7.1 SensorReading: deviceId, timestamp, temperatureC, humidityPct,
//           ethylenePpm y meta. isValid() revisa que las tres magnitudes
//           esten en un rango fisicamente posible.
//   4.2.7.2 POST /api/v1/sensor-readings. El servidor valida el token (TS41).
//   4.2.4.2 POST /api/v1/devices/{id}/heartbeat para no pasar a OFFLINE (US06).
// El servidor calcula FRESH, AT_RISK y SPOILED.
// En la placa, HIGH_ETHYLENE enciende el LED rojo y la bocina.
// TEMP_RISK enciende el ventilador a traves de un rele.

#include <ArduinoJson.h>
#include <DHTesp.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

// DHT22: temperatura y humedad. Dato en GPIO 12, alimentacion en 3V3.
// El sensor de gas: etileno en ppm. Salida analogica en GPIO 34 (ADC1).
static const int CLIMATE_PIN = 12;
static const int ETHYLENE_PIN = 34;
static const int LED_ALERT_PIN = 27;
static const int BUZZER_PIN = 25;
static const int FAN_PIN = 26;

// Identificador que el servidor asigna al vincular el dispositivo (US31).
// Hay que reemplazarlo por el UUID real del alta.
static const char *DEVICE_ID = "00000000-0000-0000-0000-000000000001";

// Token que DeviceTokenValidator revisa antes de aceptar la lectura (TS41).
static const char *DEVICE_TOKEN = "replace-with-device-token";

static const char *FIRMWARE_VERSION = "1.0";

// Rutas tal como estan en el informe. El host es el backend publicado del equipo.
static const char *READINGS_URL = "https://35-224-123-160.sslip.io/api/v1/sensor-readings";
static const char *HEARTBEAT_URL = "https://35-224-123-160.sslip.io/api/v1/devices/00000000-0000-0000-0000-000000000001/heartbeat";

// En Wokwi la red abierta es Wokwi-GUEST, canal 6.
static const char *WIFI_SSID = "Wokwi-GUEST";
static const char *WIFI_PASSWORD = "";

// America/Lima, sin horario de verano.
static const long GMT_OFFSET_SEC = -5L * 3600L;
static const int DAYLIGHT_OFFSET_SEC = 0;

// El informe pide lectura periodica y heartbeat. En el simulador el periodo
// es 10 s para poder ver cada envio en el monitor.
static const unsigned long PERIOD_MS = 10000;

// En el simulador, la posicion inicial del sensor de gas lee cerca de 3628.
// Esa posicion se traduce a 35 ppm de etileno para llenar ethylenePpm.
static const float GAS_RAW_AT_REFERENCE = 3628.0f;
static const float GAS_PPM_AT_REFERENCE = 35.0f;

// Rangos fisicos que usa isValid(), el metodo de SensorReading en el informe.
static const float TEMP_MIN_C = -40.0f;
static const float TEMP_MAX_C = 80.0f;
static const float HUMIDITY_MIN = 0.0f;
static const float HUMIDITY_MAX = 100.0f;
static const float ETHYLENE_MIN_PPM = 0.0f;
static const float ETHYLENE_MAX_PPM = 10000.0f;

// Umbral local de HIGH_ETHYLENE. En el informe, maxEthylenePpm vive en la zona.
// Por encima de este valor el alimento esta por descomponerse y suenan los actuadores.
// En Wokwi el sensor de gas abre cerca de 35 ppm, asi que el LED y la bocina
// parten encendidos. Al bajar el control del gas, se apagan.
static const float ETHYLENE_ALERT_PPM = 20.0f;
static const unsigned long BEEP_HALF_MS = 400;

// Umbral local de TEMP_RISK. En el informe, maxTemperatureC vive en la zona.
// Por encima de 8 C la camara ya no esta fria y el rele enciende el ventilador.
// El DHT22 abre en 6.4 C, asi que el ventilador parte apagado.
static const float TEMP_FAN_C = 8.0f;

// Lee temperatureC y humidityPct.
class ClimateSensor {
 public:
  // El DHT22 necesita unos 2 s antes de la primera lectura valida.
  void begin() {
    sensor.setup(CLIMATE_PIN, DHTesp::DHT22);
    delay(sensor.getMinimumSamplingPeriod());
  }

  bool read(float &temperatureC, float &humidityPct) {
    TempAndHumidity sample = sensor.getTempAndHumidity();
    if (sensor.getStatus() != DHTesp::ERROR_NONE) {
      return false;
    }
    if (isnan(sample.temperature) || isnan(sample.humidity)) {
      return false;
    }
    temperatureC = sample.temperature;
    humidityPct = sample.humidity;
    return true;
  }

 private:
  DHTesp sensor;
};

// Lee ethylenePpm. El informe exige esa magnitud en cada lectura.
// Wokwi no trae un sensor de etileno, asi que el sensor de gas
// aporta el valor en ppm.
class EthyleneSensor {
 public:
  void begin() {
    analogReadResolution(12);
    analogSetPinAttenuation(ETHYLENE_PIN, ADC_11db);
  }

  float readPpm() {
    int raw = analogRead(ETHYLENE_PIN);
    if (raw < 0) {
      raw = 0;
    }
    return raw * (GAS_PPM_AT_REFERENCE / GAS_RAW_AT_REFERENCE);
  }
};

// Reloj de la lectura. El campo del informe se llama timestamp.
class ReadingClock {
 public:
  void begin() {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
  }

  // ISO-8601 con la hora de Lima. Si el NTP aun no responde, usa un reloj local.
  String stamp() {
    struct tm now;
    if (!getLocalTime(&now, 1500) || now.tm_year < 120) {
      time_t fallback = 1791459120L + (millis() / 1000L);
      gmtime_r(&fallback, &now);
    }
    char buffer[25];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &now);
    return String(buffer);
  }
};

// Agregado SensorReading del informe: guarda las tres magnitudes,
// descarta una lectura imposible y arma el JSON del recurso.
class SensorReading {
 public:
  String deviceId;
  String timestamp;
  float temperatureC;
  float humidityPct;
  float ethylenePpm;
  String meta;

  // isValid() del informe: las tres magnitudes tienen que ser fisicamente posibles.
  bool isValid() const {
    if (temperatureC < TEMP_MIN_C || temperatureC > TEMP_MAX_C) {
      return false;
    }
    if (humidityPct < HUMIDITY_MIN || humidityPct > HUMIDITY_MAX) {
      return false;
    }
    if (ethylenePpm < ETHYLENE_MIN_PPM || ethylenePpm > ETHYLENE_MAX_PPM) {
      return false;
    }
    return true;
  }

  // Campos de SensorReadingResource: deviceId, timestamp, temperatureC,
  // humidityPct, ethylenePpm, meta.
  String toJson() const {
    String temperatureText(temperatureC, 1);
    String humidityText(humidityPct, 1);
    String ethyleneText(ethylenePpm, 1);

    StaticJsonDocument<384> document;
    document["deviceId"] = deviceId;
    document["timestamp"] = timestamp;
    document["temperatureC"] = serialized(temperatureText);
    document["humidityPct"] = serialized(humidityText);
    document["ethylenePpm"] = serialized(ethyleneText);
    document["meta"] = meta;

    String payload;
    serializeJsonPretty(document, payload);
    return payload;
  }
};

// LED rojo y bocina. Se activan juntos cuando el etileno indica HIGH_ETHYLENE.
// El LED queda fijo. La bocina pita para que se oiga la alerta dentro de la camara.
class AlertActuators {
 public:
  void begin() {
    pinMode(LED_ALERT_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(LED_ALERT_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }

  void setAlert(bool active) {
    alerting = active;
    digitalWrite(LED_ALERT_PIN, active ? HIGH : LOW);
    if (!active) {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

  void tick() {
    if (!alerting) {
      return;
    }
    unsigned long now = millis();
    if (now - lastBeepAt < BEEP_HALF_MS) {
      return;
    }
    lastBeepAt = now;
    buzzerOn = !buzzerOn;
    digitalWrite(BUZZER_PIN, buzzerOn ? HIGH : LOW);
  }

  bool isAlerting() const {
    return alerting;
  }

 private:
  bool alerting = false;
  bool buzzerOn = false;
  unsigned long lastBeepAt = 0;
};

// Rele del ventilador. El ESP32 no mueve el motor: solo cierra el rele
// cuando la temperatura indica TEMP_RISK.
class ColdRoomFan {
 public:
  void begin() {
    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW);
  }

  void setRunning(bool running) {
    digitalWrite(FAN_PIN, running ? HIGH : LOW);
  }
};

// Envia la telemetria y el heartbeat.
// El estado de frescura lo calcula el servidor. Los actuadores solo avisan HIGH_ETHYLENE.
class DeviceLink {
 public:
  bool connectWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
    for (int attempt = 0; attempt < 40 && WiFi.status() != WL_CONNECTED; attempt++) {
      delay(250);
    }
    return WiFi.status() == WL_CONNECTED;
  }

  int rssi() const {
    if (WiFi.status() != WL_CONNECTED) {
      return 0;
    }
    return WiFi.RSSI();
  }

  // POST /api/v1/sensor-readings (TS41).
  int postReading(const String &payload) {
    return post(READINGS_URL, payload);
  }

  // POST /api/v1/devices/{id}/heartbeat (US06).
  int postHeartbeat(const String &payload) {
    return post(HEARTBEAT_URL, payload);
  }

 private:
  int post(const char *url, const String &payload) {
    if (WiFi.status() != WL_CONNECTED) {
      return -1;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, url)) {
      return -2;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Token", DEVICE_TOKEN);
    int code = http.POST(payload);
    http.end();
    return code;
  }
};

// Junta la medicion, la validacion y los dos envios del informe.
class FreshSenseNode {
 public:
  void begin() {
    Serial.begin(115200);
    climate.begin();
    ethylene.begin();
    actuators.begin();
    fan.begin();
    link.connectWifi();
    clock.begin();
    nextTickAt = millis();
    Serial.println("FreshSense listo. Envia lectura y heartbeat.");
  }

  void tick() {
    actuators.tick();
    if (millis() < nextTickAt) {
      return;
    }
    nextTickAt = millis() + PERIOD_MS;
    publish();
  }

 private:
  void publish() {
    float temperatureC = 0.0f;
    float humidityPct = 0.0f;
    if (!climate.read(temperatureC, humidityPct)) {
      Serial.println("Lectura de clima invalida. No se envia.");
      return;
    }

    SensorReading reading;
    reading.deviceId = DEVICE_ID;
    reading.timestamp = clock.stamp();
    reading.temperatureC = temperatureC;
    reading.humidityPct = humidityPct;
    reading.ethylenePpm = ethylene.readPpm();
    reading.meta = String("firmware=") + FIRMWARE_VERSION + ";rssi=" + String(link.rssi());

    Serial.println("SensorReading");
    Serial.println(reading.toJson());

    if (!reading.isValid()) {
      actuators.setAlert(false);
      fan.setRunning(false);
      Serial.println("isValid() rechazo la lectura. No se envia.");
      return;
    }

    bool highEthylene = reading.ethylenePpm > ETHYLENE_ALERT_PPM;
    actuators.setAlert(highEthylene);
    bool warmRoom = reading.temperatureC > TEMP_FAN_C;
    fan.setRunning(warmRoom);
    if (warmRoom) {
      Serial.println("TEMP_RISK: ventilador encendido.");
    } else {
      Serial.println("Temperatura en rango: ventilador apagado.");
    }
    if (highEthylene) {
      Serial.println("HIGH_ETHYLENE: LED rojo encendido y bocina sonando.");
    } else {
      Serial.println("Etileno en rango: LED rojo apagado y bocina en silencio.");
    }

    int readingCode = link.postReading(reading.toJson());
    Serial.print("POST /api/v1/sensor-readings -> ");
    Serial.println(readingCode);

    StaticJsonDocument<128> heartbeat;
    heartbeat["deviceId"] = DEVICE_ID;
    heartbeat["timestamp"] = reading.timestamp;
    String heartbeatPayload;
    serializeJsonPretty(heartbeat, heartbeatPayload);

    Serial.println("Heartbeat");
    Serial.println(heartbeatPayload);
    int heartbeatCode = link.postHeartbeat(heartbeatPayload);
    Serial.print("POST /api/v1/devices/{id}/heartbeat -> ");
    Serial.println(heartbeatCode);
  }

  ClimateSensor climate;
  EthyleneSensor ethylene;
  AlertActuators actuators;
  ColdRoomFan fan;
  ReadingClock clock;
  DeviceLink link;
  unsigned long nextTickAt;
};
