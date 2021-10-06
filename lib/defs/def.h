/**************************************************************
 *  def.h
 *
 *  Created on: 01 September 2021
 *  Author: SenMorgan https://github.com/SenMorgan
 *  
 ***************************************************************/

#ifndef DEF_H
#define DEF_H

#define DEFAULT_HOSTNAME            "Smart_Blinds"
// how many times should reset button be pressed to call setting erase
#define RESET_BUTTON_CYCLES         3
// maximum delay between pressing reset button to call setting erase
#define RESET_BUTTON_DELAY          500

// MQTT definitions
#define DEFAULT_TOPIC               "/blinds/"
#define MQTT_WILL_TOPIC             "availability"
#define MQTT_QOS                    1
#define MQTT_RETAIN                 1
#define MQTT_WILL_MESSAGE           "offline"
#define MQTT_SUBSCRIBE_TOPIC        "#"
#define MQTT_PUBLISH_TOPIC          "position"
#define MQTT_AVAILABILITY_TOPIC     "availability"
#define MQTT_AVAILABILITY_MESSAGE   "online"
#define MQTT_UPTIME_TOPIC           "uptime"
#define MQTT_CMD_TOPIC              "set"
#define MQTT_CMD_OPEN               "OPEN"
#define MQTT_CMD_CLOSE              "CLOSE"
#define MQTT_CMD_STOP               "STOP"
#define MQTT_SET_POSITION_TOPIC     "set_position"
#define MQTT_CMD_CORRECT_UP         "CORRECT_UP"
#define MQTT_CMD_CORRECT_DOWN       "CORRECT_DOWN"

// timer period in ms to check buttons states
#define READ_BUTTONS_STEP               50
// period in ms after toggling buttons in which changing direction is detected
// needs if you have a button with only 2-positions
#define CHANGE_DIRRECTION_DELAY         1000
// delay in ms to hold motor after the end of moving to prevent drifting
#define AFTER_STOP_DELAY                2000
// maximum steps value (calculated manually)
#define DEFAULT_MAX_POSITION            104000
// value of steps to be moved when using correction
#define CORRECTION_OFFSET               1000
// maximum permitted speed for stepper motor
#define DEFAULT_STEPPER_MAX_SPEED       2000.0
// acceleration/deceleration rate for stepper motor
#define DEFAULT_STEPPER_ACCELERATION    1800.0

// delay in ms between reporting states to MQTT server in idle mode
#define PUBLISH_STEP_LONG           30000
// delay in ms between reporting states to MQTT server in moving mode
#define PUBLISH_STEP_SHORT          500

// stepper motor driver pins
#define PIN_STEP        D1
#define PIN_DIR         D2
#define PIN_ENABLE      D3
// other IOs
#define STATUS_LED      D4
#define BUTTON_1        D6
#define BUTTON_2        D7

#endif /* DEF_H */