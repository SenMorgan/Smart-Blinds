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

// Returns 1 if successfully reconnected to MQTT server with credentials specified in def.h
bool reconnect(void)
{
    if (mqttClient.connect(MQTT_DEVICE_NAME, MQTT_LOGIN, MQTT_PASSWORD, MQTT_TOPIC, MQTT_QOS, MQTT_RETAIN, MQTT_WILL_MESSAGE))
    {
        mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC);
        return 1;
    }
    return 0;
}

// Sending keepAlive message
// Returns 1 if connected to MQTT server. Need to be called in main loop
bool mqttLoop(void)
{
    return mqttClient.loop();
}

// Saving all received data
void callback(String topic, byte *payload, uint16_t length)
{
    msgFlag = 1;        // set flag if new message came
    msgTopic = topic;
    msgString = "";
    for (uint16_t i = 0; i < length; i++)
        msgString += (char)payload[i];
}

// Initializing MQTT connection
void init_MQTT(const char *server, uint16_t port)
{
    mqttClient.setServer(server, port);
    mqttClient.setCallback(callback);
    reconnect();
}

// Publish incoming data
void publish_data(uint32_t timeNow, uint32_t data)
{
    static char buff[20];

    sprintf(buff, "%d", data);
    mqttClient.publish(MQTT_PUBLISH_TOPIC, buff, true);
    mqttClient.publish(MQTT_AVAILABILITY_TOPIC, MQTT_AVAILABILITY_MESSAGE);
    sprintf(buff, "%d sec", timeNow / 1000);
    mqttClient.publish(MQTT_UPTIME_TOPIC, buff);
}
