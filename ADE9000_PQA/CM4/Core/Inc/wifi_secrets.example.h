/*
 * wifi_secrets.example.h
 *
 * Copy this file to wifi_secrets.h (same folder) and fill in your network.
 * wifi_secrets.h is git-ignored so your credentials never get committed.
 */
#ifndef WIFI_SECRETS_H
#define WIFI_SECRETS_H

#define WIFI_SSID         "your-wifi-ssid"
#define WIFI_PASSWORD     "your-wifi-password"

#define MQTT_CLIENT_ID    "stm32_meter"
#define MQTT_BROKER_IP    "192.168.1.10"     /* machine running Mosquitto or similar */
#define MQTT_BROKER_PORT  1883
#define MQTT_TOPIC        "meter/data"

#endif /* WIFI_SECRETS_H */
