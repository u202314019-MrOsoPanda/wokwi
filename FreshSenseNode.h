#ifndef FRESHSENSE_NODE_H
#define FRESHSENSE_NODE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHTesp.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

// FreshSense cold-room node. Wiring matches the report, section 5.6:
// DHT22 VCC -> 3V3, DHT22 GND -> GND, DHT22 SDA -> GPIO 12.

static const int DHT_PIN = 12;
static const int STATUS_LED_PIN = 2;
static const char *DEVICE_ID = "esp32-cocina-01";
static const char *WIFI_SSID = "Wokwi-GUEST";
static const char *WIFI_PASSWORD = "";
static const int WIFI_CHANNEL = 6;
static const char *EDGE_URL = "https://35-224-123-160.sslip.io/api/edge/readings";
static const char *DEVICE_KEY = "replace-with-device-key";
static const char *NTP_SERVER = "pool.ntp.org";
static const long GMT_OFFSET_SEC = -5 * 3600;
static const unsigned long SAMPLE_PERIOD_MS = 10000;
static const float FRESH_MAX_C = 8.0f;
static const float RISK_MAX_C = 12.0f;

class Dht22Sensor {
 public:
  void begin() {
    sensor.setup(DHT_PIN, DHTesp::DHT22);
    delay(sensor.getMinimumSamplingPeriod());
  }

  bool read(float &temperatureC, float &humidityPct) {
    TempAndHumidity sample = sensor.getTempAndHumidity();
    if (sensor.getStatus() != DHTesp::ERROR_NONE || isnan(sample.temperature) ||
        isnan(sample.humidity)) {
      return false;
    }
    temperatureC = sample.temperature;
    humidityPct = sample.humidity;
    return true;
  }

 private:
  DHTesp sensor;
};

class ReadingClock {
 public:
  void begin() { configTime(GMT_OFFSET_SEC, 0, NTP_SERVER); }

  String stamp() {
    struct tm timeInfo;
    if (!getLocalTime(&timeInfo, 1500) || timeInfo.tm_year < 120) {
      time_t fallback = 1791477120 + millis() / 1000;
      gmtime_r(&fallback, &timeInfo);
    }
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", &timeInfo);
    return String(buffer);
  }
};

class EdgePublisher {
 public:
  bool connectWifi() {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
    unsigned long started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < 8000) {
      delay(100);
    }
    return WiFi.status() == WL_CONNECTED;
  }

  int publish(const String &payload) {
    if (!connectWifi()) {
      return -1;
    }
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, EDGE_URL)) {
      return -2;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Key", DEVICE_KEY);
    int status = http.POST(payload);
    http.end();
    return status;
  }
};

class FreshSenseNode {
 public:
  void begin() {
    pinMode(STATUS_LED_PIN, OUTPUT);
    Serial.begin(115200);
    sensor.begin();
    Serial.println("FreshSense node");
    Serial.print("Device ID: ");
    Serial.println(DEVICE_ID);
    online = publisher.connectWifi();
    clock.begin();
    Serial.print("Connection: ");
    Serial.println(online ? "Connected" : "Disconnected");
    lastSampleMs = millis() - SAMPLE_PERIOD_MS;
  }

  void tick() {
    unsigned long now = millis();
    updateLed(now);
    if (now - lastSampleMs < SAMPLE_PERIOD_MS) {
      return;
    }
    lastSampleMs = now;
    sample();
  }

 private:
  Dht22Sensor sensor;
  ReadingClock clock;
  EdgePublisher publisher;
  unsigned long lastSampleMs = 0;
  unsigned long lastBlinkMs = 0;
  uint32_t sequence = 0;
  bool ledOn = false;
  bool alert = false;
  bool online = false;

  void sample() {
    float temperatureC = 0;
    float humidityPct = 0;
    online = publisher.connectWifi();

    Serial.println("---");
    Serial.print("Connection: ");
    Serial.println(online ? "Connected" : "Disconnected");

    if (!sensor.read(temperatureC, humidityPct)) {
      alert = true;
      Serial.println("Monitoring Status: Disconnected");
      Serial.println("Temperature: --");
      Serial.println("Humidity: --");
      return;
    }

    sequence++;
    String readingId = "rd-" + padded(sequence);
    String timestamp = clock.stamp();
    const char *status = monitoringStatus(temperatureC);
    alert = strcmp(status, "FRESH") != 0;

    String temperatureText = oneDecimal(temperatureC);
    String humidityText = oneDecimal(humidityPct);

    StaticJsonDocument<256> document;
    document["deviceId"] = DEVICE_ID;
    document["id"] = readingId;
    document["temperature"] = serialized(temperatureText);
    document["humidity"] = serialized(humidityText);
    document["time"] = timestamp;

    String payload;
    serializeJson(document, payload);

    Serial.print("Monitoring Status: ");
    Serial.println(status);
    Serial.print("Temperature: ");
    Serial.print(temperatureText);
    Serial.println(" C");
    Serial.print("Humidity: ");
    Serial.print(humidityText);
    Serial.println(" %");
    Serial.print("Last Reading: ");
    Serial.println(timestamp);
    Serial.println(payload);

    int httpStatus = publisher.publish(payload);
    Serial.print("POST /api/edge/readings: ");
    Serial.println(httpStatus);
  }

  void updateLed(unsigned long now) {
    unsigned long period = !online ? 1200 : (alert ? 150 : 700);
    if (now - lastBlinkMs < period) {
      return;
    }
    lastBlinkMs = now;
    ledOn = !ledOn;
    digitalWrite(STATUS_LED_PIN, ledOn ? HIGH : LOW);
  }

  static const char *monitoringStatus(float temperatureC) {
    if (temperatureC <= FRESH_MAX_C) {
      return "FRESH";
    }
    if (temperatureC <= RISK_MAX_C) {
      return "AT_RISK";
    }
    return "TEMP_RISK";
  }

  static String padded(uint32_t value) {
    char buffer[7];
    snprintf(buffer, sizeof(buffer), "%06lu", static_cast<unsigned long>(value));
    return String(buffer);
  }

  static String oneDecimal(float value) {
    char buffer[12];
    dtostrf(value, 0, 1, buffer);
    return String(buffer);
  }
};

#endif
