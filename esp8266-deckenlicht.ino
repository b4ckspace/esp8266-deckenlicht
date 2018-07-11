#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include "settings.h"
#include <Wire.h>
#include <PCA9685.h>

WiFiClient wifiClient;
PubSubClient mqttClient;
uint8_t mqttRetryCounter = 0;
unsigned long lastMs;
char sprintfHelper[16] = {0};

PCA9685 driver = PCA9685(0x00, PCA9685_MODE_N_DRIVER, PCA9685_MIN_FREQUENCY);

void setLight(uint8_t light, uint8_t value) {
  light = constrain(light, 0, 9);
  value = constrain(value, 0, 100);
  
  driver
    .getPin(light)
    .setValueAndWrite(map(value, 0, 100, PCA9685_FULL_OFF, PCA9685_FULL_ON));
}

void mqttConnect() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect(HOSTNAME, MQTT_TOPIC_STATE, 1, true, "disconnected")) {
      mqttClient.subscribe(MQTT_TOPIC_BASE);
      mqttClient.publish(MQTT_TOPIC_STATE, "connected", true);
      mqttRetryCounter = 0;
      
    } else {
      Serial.println("MQTT connect failed!");
      
      if (mqttRetryCounter++ > MQTT_MAX_CONNECT_RETRY) {
        ESP.restart();
      }
      
      delay(2000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  const char* identifier = topic + strlen(MQTT_TOPIC_BASE) - 1;

  Serial.print("MQTT ");
  Serial.print(topic);
  Serial.print(" : ");
  Serial.print(identifier);

  if (identifier[0] >= '0' && identifier[0] <= '9') {
    uint8_t light = atoi(identifier);
    uint8_t brightness = atoi((char*) payload);
    
    Serial.print(" -> ");
    Serial.println(brightness);

    setLight(light, brightness);
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SDA, I2C_SCL);
  driver.setup();

  for (uint8_t i = 0; i < 10; i++) {
    setLight(i, 0);
  }

  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  mqttClient.setClient(wifiClient);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();

}

void loop() {
  mqttConnect();

  if (lastMs + TEMPERATURE_POLL_INTERVAL_MS < millis()) {
    lastMs = millis();

    int analogValue = analogRead(TEMPERATURE_PIN);
    float voltage = ((analogValue / 1024.0) * 3.3);
    float celsius = (voltage - 0.5) * 100.0;

    dtostrf(celsius, 4, 2, sprintfHelper);
    mqttClient.publish(MQTT_TOPIC_TEMPERATURE, sprintfHelper, true);
  }

  mqttClient.loop();
  ArduinoOTA.handle();
}
