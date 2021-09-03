/**************************************************************
 *  mqtt.h
 *
 *  Created on: 02 September 2021
 *  Author: SenMorgan https://github.com/SenMorgan
 *  
 ***************************************************************/

#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

extern String msgTopic;
extern String msgString;
extern bool msgFlag;

void init_MQTT(const char *server, uint16_t port);
bool reconnect(void);
bool mqttServerConneted(void);
void mqttLoop(void);
void publish_data(uint32_t timeNow, uint32_t data);

#endif /* MQTT_H */