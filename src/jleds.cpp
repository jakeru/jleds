// jleds.cpp

// Written by Jakob Ruhe (jakob.ruhe@gmail.com) in November 2018.

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void setAllComponents(float value);

// Pins
const int PIN_WEMOS_D1 = 5;
const int PIN_WEMOS_D2 = 4;
const int PIN_WEMOS_D4 = 2;
const int PIN_WEMOS_D5 = 14;
const int PIN_WEMOS_D6 = 12;

const int PIN_LED = PIN_WEMOS_D4;
const int PIN_R = PIN_WEMOS_D5;
const int PIN_G = PIN_WEMOS_D6;
const int PIN_B = PIN_WEMOS_D1;
const int PIN_W = PIN_WEMOS_D2;

const int PIN_RGBW[] = { PIN_R, PIN_G, PIN_B, PIN_W };

// WiFi settings
const char* ssid = "";
const char* password = "";

// MQTT settings
const char* mqtt_server = "atom.home";
const int mqtt_server_port = 1883;
const char* mqtt_client_name = "led";
const char* mqtt_topic_out = "leds/state";
const char* mqtt_topic_in = "leds"

// WiFi
WiFiClient espClient;

// MQTT
PubSubClient client(espClient);
uint32_t s_timeLastMessage;
uint32_t s_timeLastConnect;
bool s_hasTriedToConnect;
char s_buf[64];

static uint32_t s_lastBlinkAt;

static bool timeAtOrAfter(uint32_t t, uint32_t now)
{
  return (int32_t)(now - t) >= 0;
}

static int32_t timeDiff(uint32_t t1, uint32_t t2)
{
  return t2 - t1;
}

static void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
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
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

static void setupMQTT() {
  client.setServer(mqtt_server, mqtt_server_port);
  client.setCallback(callbackForMQTT);
}

static bool connectMQTT() {
  Serial.printf("Connecting to mqtt %s:%d as %s...\n", mqtt_server,
    mqtt_server_port, mqtt_client_name);
  if (!client.connect(mqtt_client_name)) {
    Serial.print("Failed to connect to MQTT server: ");
    Serial.println(client.state());
    return false;
  }
  Serial.println("Connected to MQTT server");
  client.publish(mqtt_topic_out, "connected");
  client.subscribe(mqtt_topic_in);
  return true;
}

static void loopMQTT()
{
  uint32_t now = millis();
  client.loop();
  if (!client.connect()) {
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
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  Serial.begin(115200);
  Serial.println("Booting");

  setupWiFi();
  setupOTA();
  setupMQTT();
}

void setLed(bool state)
{
  digitalWrite(PIN_LED, state ? LOW : HIGH);
}

float clamp(float value, float min, float max)
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

void setComponent(int c, float value)
{
  int pin = PIN_RGBW[c];
  float clamped = clamp(value, 0, 1);
  int pwm = (int)(PWMRANGE * value + 0.5f);
  analogWrite(pin, pwm);
  //Serial.printf("c: %d: value: %.3f, clamped: %.3f, pwm: %d\r\n",
  //  c, value, clamped, pwm);
}

static void setAllComponents(float value)
{
  for (size_t i = 0; i < ARRAY_SIZE(PIN_RGBW); i++) {
    setComponent(i, value);
  }
}

static void void callbackForMQTT(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
}

void loop() {
  ArduinoOTA.handle();
  loopMQTT();

  // Test
  uint32_t t = millis();
  if (timeAtOrAfter(s_lastBlinkAt + 1000, t)) {
    setLed(true);
  }
  if (timeAtOrAfter(s_lastBlinkAt + 3000, t)) {
    s_lastBlinkAt = t;
    setLed(false);
  }

  float w = 0.001f;
  for (size_t i = 0; i < 3; i++) {
    float val = 0.1 + 0.9 * (1 + sin(w * t + i * PI / 2)) / 2;
    setComponent(i, val);
  }
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
