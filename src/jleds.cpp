// jleds.cpp

// Control a RGB LED strip with an ESP8266 talking MQTT.

// Written by Jakob Ruhe (jakob.ruhe@gmail.com) in November 2018.

// Rewritten October 2019 with inspiration from:
// https://github.com/mertenats/Open-Home-Automation/blob/master/ha_mqtt_rgb_light/ha_mqtt_rgb_light.ino

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "config.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void callbackForMQTT(char* topic, byte* bytes, unsigned int length);
static void setComponent(int c, float value);

const int PIN_RGBW[] = { PIN_R, PIN_G, PIN_B, PIN_W };

struct Effect {
  virtual const char* name() = 0;
  virtual void start() { };
  virtual void loop() = 0;
};

struct RainbowEffect : Effect {
  virtual const char* name() {
    return "rainbow";
  }
  virtual void loop();
};

// WiFi
static WiFiClient s_espClient;

// MQTT
static PubSubClient s_mqttClient(s_espClient);
static uint32_t s_timeLastConnect;
static bool s_hasTriedToConnect;
static bool s_mqttConnected;
static char s_buf[64];

// State
static uint32_t s_lastBlinkAt;
static bool s_state;
static uint8_t s_red;
static uint8_t s_green;
static uint8_t s_blue;
static uint8_t s_white;
static Effect* s_effect;

static RainbowEffect s_rainbowEffect = RainbowEffect();
static Effect* s_effects[] = { &s_rainbowEffect };

static bool timeAtOrAfter(uint32_t t, uint32_t now)
{
  return (int32_t)(now - t) >= 0;
}

static void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(WIFI_HOSTNAME);
  Serial.printf("Connecting to %s...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

static void setupOTA() {
  ArduinoOTA.onStart([]() {
    Serial.println("OTA started");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA completed");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("\nOTA Error: #%u: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    else Serial.println("Unknown error");
  });
  ArduinoOTA.begin();
}

static void publishStatus()
{
  s_mqttClient.publish(MQTT_STATUS_TOPIC, "online", true);
}

static void publishLightState()
{
  s_mqttClient.publish(MQTT_LIGHT_STATE_TOPIC, s_state ? "ON" : "OFF", true);
}

static void setLightState(bool state)
{
  s_state = state;
}

static void publishRGB()
{
  snprintf(s_buf, sizeof(s_buf), "%d,%d,%d", s_red, s_green, s_blue);
  s_mqttClient.publish(MQTT_RGB_STATE_TOPIC, s_buf, true);
}

static void setRGB(uint8_t red, uint8_t green, uint8_t blue)
{
  s_red = red;
  s_green = green;
  s_blue = blue;
}

static void setWhite(uint8_t white)
{
  s_white = white;
}

static void publishWhite()
{
  snprintf(s_buf, sizeof(s_buf), "%d", s_white);
  s_mqttClient.publish(MQTT_WHITE_STATE_TOPIC, s_buf, true);
}

static void setEffect(const String& name)
{
  s_effect = NULL;
  for (size_t i = 0; i < ARRAY_SIZE(s_effects); i++) {
    if (name == s_effects[i]->name()) {
      s_effect = s_effects[i];
      s_effect->start();
      break;
    }
  }
}

static void publishEffect()
{
  s_mqttClient.publish(MQTT_EFFECT_STATE_TOPIC, s_effect ? s_effect->name() : "", true);
}

static void setupMQTT() {
  s_mqttClient.setServer(MQTT_SERVER, MQTT_SERVER_PORT);
  s_mqttClient.setCallback(callbackForMQTT);
}

static bool connectMQTT() {
  Serial.printf("Connecting to mqtt %s:%d...\n",
    MQTT_SERVER, MQTT_SERVER_PORT);
  if (!s_mqttClient.connect(
    MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD,
    MQTT_STATUS_TOPIC, 0, 1, "online"))
  {
    Serial.print("Failed to connect to MQTT server: ");
    Serial.println(s_mqttClient.state());
    return false;
  }
  s_mqttConnected = true;
  Serial.println("Connected to MQTT server");
  publishStatus();
  publishLightState();
  publishRGB();
  publishWhite();
  publishEffect();
  s_mqttClient.subscribe(MQTT_LIGHT_COMMAND_TOPIC);
  s_mqttClient.subscribe(MQTT_RGB_COMMAND_TOPIC);
  s_mqttClient.subscribe(MQTT_WHITE_COMMAND_TOPIC);
  s_mqttClient.subscribe(MQTT_EFFECT_COMMAND_TOPIC);

  return true;
}

static void loopMQTT()
{
  uint32_t now = millis();
  s_mqttClient.loop();
  if (!s_mqttClient.connected()) {
    if (s_mqttConnected) {
      Serial.println("Lost connection with MQTT server");
      s_mqttConnected = false;
    }
    if (!s_hasTriedToConnect || timeAtOrAfter(s_timeLastConnect + 60000, now)) {
      connectMQTT();
      s_hasTriedToConnect = true;
      s_timeLastConnect = now;
    }
    return;
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  for (size_t i = 0; i < ARRAY_SIZE(PIN_RGBW); i++) {
    int pin = PIN_RGBW[i];
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }

  Serial.begin(115200);
  Serial.println("Booting");

  setupWiFi();
  setupOTA();
  setupMQTT();
}

static void setStatusLed(bool state)
{
  digitalWrite(PIN_LED, state ? LOW : HIGH);
}

static float clamp(float value, float min, float max)
{
  if (value < min) {
    return min;
  }
  else if (value > max) {
    return max;
  }
  else {
    return value;
  }
}

static void setComponent(int c, float value)
{
  int pin = PIN_RGBW[c];
  float clamped = clamp(value, 0, 1);
  int pwm = (int)(PWMRANGE * clamped + 0.5f);
  analogWrite(pin, pwm);
}

static void setAll(float value)
{
  for (size_t c = 0; c < ARRAY_SIZE(PIN_RGBW); c++) {
    setComponent(c, value);
  }
}

static void callbackForMQTT(char* topic, byte* bytes, unsigned int length)
{
  String payload;
  for (size_t i = 0; i < length; i++) {
    payload += (char)bytes[i];
  }
  Serial.printf("Topic: %s, payload: %s\n", topic, payload.c_str());
  if (String(MQTT_LIGHT_COMMAND_TOPIC).equals(topic)) {
    setLightState(payload == "ON");
    publishLightState();
  }
  else if (String(MQTT_RGB_COMMAND_TOPIC).equals(topic)) {
    uint8_t firstIndex = payload.indexOf(',');
    uint8_t lastIndex = payload.lastIndexOf(',');

    int rgb_red = payload.substring(0, firstIndex).toInt();
    if (rgb_red < 0 || rgb_red > 255) {
      return;
    }

    int rgb_green = payload.substring(firstIndex + 1, lastIndex).toInt();
    if (rgb_green < 0 || rgb_green > 255) {
      return;
    }

    int rgb_blue = payload.substring(lastIndex + 1).toInt();
    if (rgb_blue < 0 || rgb_blue > 255) {
      return;
    }

    setRGB(rgb_red, rgb_green, rgb_blue);
    publishRGB();
  }
  else if (String(MQTT_WHITE_STATE_TOPIC).equals(topic)) {
    int white = payload.toInt();
    if (white < 0 || white > 255) {
      return;
    }
    setWhite(white);
    publishWhite();
  }
  else if (String(MQTT_EFFECT_COMMAND_TOPIC).equals(topic)) {
    setEffect(payload);
    publishEffect();
  }
}

void RainbowEffect::loop()
{
  const float w = 0.001f;
  const uint32_t t = millis();
  for (size_t i = 0; i < 3; i++) {
    float val = 0.1 + 0.9 * (1 + sin(w * t + i * PI / 2)) / 2;
    setComponent(i, val);
  }
  setComponent(3, 0);
}

static void controlLeds()
{
  if (s_state) {
    if (s_effect) {
      s_effect->loop();
    }
    else {
      setComponent(0, s_red / 255.0f);
      setComponent(1, s_green / 255.0f);
      setComponent(2, s_blue / 255.0f);
      setComponent(3, s_white / 255.0f);
    }
  }
  else {
    setAll(0);
  }
}

void loop()
{
  ArduinoOTA.handle();
  loopMQTT();

  uint32_t t = millis();
  if (timeAtOrAfter(s_lastBlinkAt + 2000, t)) {
    setStatusLed(true);
  }
  if (timeAtOrAfter(s_lastBlinkAt + 2200, t)) {
    s_lastBlinkAt = t;
    setStatusLed(false);
  }

  controlLeds();

  /*
  static float value = 0;
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'u') {
      value += 0.01;
    }
    else if (c == 'd') {
      value -= 0.01;
    }
    else if (c == ' ') {
      value = 0;
    }
    else if (c == 'f') {
      value = 1;
    }
    value = clamp(value, 0, 1);
    setComponent(0, value);
  }
  */
}
