/**************************************************************
 *  def.h
 *
 *  Created on: 01 September 2021
 *  Author: SenMorgan https://github.com/SenMorgan
 *  
 ***************************************************************/

#ifndef __DEF_H_
#define __DEF_H_

#define MQTT_SERVER     "192.168.1.100"
#define MQTT_PORT       1883

// delay in ms between reporting states to MQTT server in idle mode
#define PUBLISH_STEP_SHORT  10000
// delay in ms between reporting states to MQTT server in moving mode
#define PUBLISH_STEP_LONG  500
// maximum steps value (calculated manually)
#define MAX_POSITION 30000

// stepper motor driver pins
#define PIN_STEP D1
#define PIN_DIR D2
#define PIN_ENABLE D3

#define BuiltinLED  D4
#define Button_1_PIN D6
#define Button_2_PIN D7

#endif /* __DEF_H_ */