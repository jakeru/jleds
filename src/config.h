// config.h

#pragma once

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

// WiFi settings
#define WIFI_SSID "PLEASE_SET_WIFI_SSID"
#define WIFI_PASSWORD "PLEASE_SET_WIFI_PASSWORD"
#define WIFI_HOSTNAME "outdoor_ledstrip"

// MQTT server
#define MQTT_SERVER "atom.home"
#define MQTT_SERVER_PORT 1883

// MQTT client name and account credentials
#define MQTT_CLIENT_ID "outdoor_ledstrip"
#define MQTT_USER ""
#define MQTT_PASSWORD ""

// MQTT topics
#define MQTT_BASE_TOPIC "outdoor_ledstrip/"
#define MQTT_STATUS_TOPIC MQTT_BASE_TOPIC "status"
#define MQTT_LIGHT_STATE_TOPIC MQTT_BASE_TOPIC "light/status"
#define MQTT_LIGHT_COMMAND_TOPIC MQTT_BASE_TOPIC "light/switch"
#define MQTT_RGB_STATE_TOPIC MQTT_BASE_TOPIC "rgb/status"
#define MQTT_RGB_COMMAND_TOPIC MQTT_BASE_TOPIC "rgb/set"
#define MQTT_WHITE_STATE_TOPIC MQTT_BASE_TOPIC "white/status"
#define MQTT_WHITE_COMMAND_TOPIC MQTT_BASE_TOPIC "white/set"
#define MQTT_EFFECT_STATE_TOPIC MQTT_BASE_TOPIC "effect/status"
#define MQTT_EFFECT_COMMAND_TOPIC MQTT_BASE_TOPIC "effect/set"
