/**************************************************************
 *  def.h
 *
 *  Created on: 01 September 2021
 *  Author: SenMorgan https://github.com/SenMorgan
 *  
 ***************************************************************/

#ifndef DEF_H
#define DEF_H

#define HOSTNAME                    "Smart_Blinds"

// MQTT definitions
#define MQTT_SERVER                 "192.168.1.100"
#define MQTT_PORT                   1883
#define MQTT_DEVICE_NAME            "Cover"
#define MQTT_TOPIC                  "/Cover/availability"
#define MQTT_QOS                    1
#define MQTT_RETAIN                 1
#define MQTT_WILL_MESSAGE           "offline"
#define MQTT_SUBSCRIBE_TOPIC        "/Cover/#"
#define MQTT_PUBLISH_TOPIC          "/Cover/position"
#define MQTT_AVAILABILITY_TOPIC     "/Cover/availability"
#define MQTT_AVAILABILITY_MESSAGE   "online"
#define MQTT_UPTIME_TOPIC           "/Cover/uptime"
#define MQTT_CMD_TOPIC              "/Cover/set"
#define MQTT_CMD_OPEN               "OPEN"
#define MQTT_CMD_CLOSE              "CLOSE"
#define MQTT_CMD_STOP               "STOP"
#define MQTT_SET_POSITION_TOPIC     "/Cover/set_position"

// timer period in ms to check buttons states
#define READ_BUTTONS_STEP           50
// period in ms after toggling buttons in which changing direction is detected
// needs if you have a button with only 2-positions
#define CHANGE_DIRRECTION_DELAY     1000

// delay in ms between reporting states to MQTT server in idle mode
#define PUBLISH_STEP_LONG   30000
// delay in ms between reporting states to MQTT server in moving mode
#define PUBLISH_STEP_SHORT  500
// maximum steps value (calculated manually)
#define MAX_POSITION        104000

// stepper motor driver pins
#define PIN_STEP        D1
#define PIN_DIR         D2
#define PIN_ENABLE      D3

#define STATUS_LED      D4
#define BUTTON_1        D6
#define BUTTON_2        D7

#endif /* DEF_H */