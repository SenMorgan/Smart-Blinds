/**************************************************************
 *  mqtt.cpp
 *
 *  Created on: 02 September 2021
 *  Author: SenMorgan https://github.com/SenMorgan
 *  
 ***************************************************************/

#include "mqtt.h"

String msgTopic = "";
String msgString = "";
bool msgFlag = 0;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

bool reconnect(void)
{
    if (mqttClient.connect("Cover", "login", "password", "/Cover/availability", 1, true, "offline"))
    {
        mqttClient.subscribe("/Cover/#");
        return 1;
    }
    return 0;
}

bool mqttServerConneted(void)
{
    return mqttClient.connected();
}

void mqttLoop(void)
{
    mqttClient.loop();
}

void callback(String topic, byte *payload, uint16_t length)
{
    msgFlag = 1;
    msgTopic = topic;
    msgString = "";
    for (uint16_t i = 0; i < length; i++)
        msgString += (char)payload[i];
}

void init_MQTT(const char *server, uint16_t port)
{
    mqttClient.setServer(server, port);
    mqttClient.setCallback(callback);
    reconnect();
}

void publish_data(uint32_t timeNow, uint32_t data)
{
    static char buff[20];

    sprintf(buff, "%d", data);
    mqttClient.publish("/Cover/position", buff, true);
    mqttClient.publish("/Cover/availability", "online");
    sprintf(buff, "%d sec", timeNow / 1000);
    mqttClient.publish("/Cover/uptime", buff);
}
