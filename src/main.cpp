/**************************************************************
 *  main.cpp
 *
 *  Created on: 07 August 2019
 *  Rebuild in PlatformIO: 01 September 2021
 *  Author: SenMorgan https://github.com/SenMorgan
 *  
 ***************************************************************/

#include "def.h"
#include "mqtt.h"
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <AccelStepper.h>
#include <EEPROM.h>

AccelStepper stepper(1, PIN_STEP, PIN_DIR);

byte currentPos = 0, targetPos = 0;
boolean holdPos = 0, stepperEnabled = 0;
uint32_t publishTimer = 0;

void stepper_prog()
{
  static byte oldTargetPos;
  static unsigned long antiDriftTimer;

  currentPos = map(stepper.currentPosition(), 0, MAX_POSITION, 0, 100);
  if (oldTargetPos != targetPos)
  {
    oldTargetPos = targetPos;
    if (!stepperEnabled)
    {
      stepperEnabled = true;
      stepper.enableOutputs();
      publishTimer = 0;
    }
    uint32_t targetPosInSteps = map(targetPos, 0, 100, 0, MAX_POSITION);
    stepper.moveTo(targetPosInSteps);
  }
  else if (holdPos)
  {
    holdPos = false;
    targetPos = currentPos;
  }
  else if (stepperEnabled && !stepper.isRunning() && antiDriftTimer == 0)
    antiDriftTimer = millis() + 2000;
  else if (antiDriftTimer > 0 && millis() > antiDriftTimer)
  {
    antiDriftTimer = 0;
    stepperEnabled = false;
    stepper.disableOutputs();
    EEPROM.put(0, currentPos);
    EEPROM.commit();
  }
  stepper.run();
}

void button_read()
{
#define readButtonStep 50
#define buttonDirStep 1000
  static unsigned long readButtonTimer, buttonDirTimer;
  static boolean Button_Up_released, Button_Down_released;

  if (millis() > readButtonTimer)
  {
    readButtonTimer = millis() + readButtonStep;

    if (digitalRead(Button_1_PIN) && Button_Up_released)
    {
      Button_Up_released = false;
      Serial.println("Button UP pressed");
      if (!stepperEnabled || millis() < buttonDirTimer)
      {
        targetPos = 100;
        buttonDirTimer = millis() + buttonDirStep;
      }
      else
        holdPos = true;
    }
    else if (!digitalRead(Button_1_PIN) && !Button_Up_released)
    {
      Button_Up_released = true;
    }

    else if (digitalRead(Button_2_PIN) && Button_Down_released)
    {
      Button_Down_released = false;
      Serial.println("Button DOWN pressed");
      if (!stepperEnabled || millis() < buttonDirTimer)
      {
        targetPos = 0;
        buttonDirTimer = millis() + buttonDirStep;
      }
      else
        holdPos = true;
    }
    else if (!digitalRead(Button_2_PIN) && !Button_Down_released)
    {
      Button_Down_released = true;
    }
  }
}

void setup()
{
  pinMode(BuiltinLED, OUTPUT);

  EEPROM.begin(4);
  EEPROM.get(0, currentPos);
  targetPos = currentPos;
  uint32_t targetPosInSteps = map(targetPos, 0, 100, 0, MAX_POSITION);
  stepper.setCurrentPosition(targetPosInSteps);

  Serial.begin(115200);

  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the "Smart Blinds" name
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("Smart Blinds");
  // continue if conected

  init_MQTT(MQTT_SERVER, MQTT_PORT);

  stepper.setEnablePin(PIN_ENABLE);
  stepper.setPinsInverted(false, false, false, false, true);
  stepper.setMaxSpeed(2000.0);
  stepper.setAcceleration(1300.0);
}

//void mqtt_parse_message(bool *msgFlag, String *msgTopic, String *msgString)
void mqtt_parse_message()
{
  if (msgFlag)
  {
    msgFlag = 0;

    if (msgTopic == "/Cover/set")
    {
      if (msgString == "OPEN")
        targetPos = 100;
      else if (msgString == "CLOSE")
        targetPos = 0;
      else if (msgString == "STOP")
        holdPos = 1;
    }
    else if (msgTopic == "/Cover/set_position")
    {
      targetPos = constrain(msgString.toInt(), 0, 100);
      //Serial.print("Mqtt set position: "); Serial.println(targetPos);
    }
  }
}

void loop()
{
  uint32_t timeNow = millis();

  if (timeNow > publishTimer)
  {
    if (mqttServerConneted())
    {
      digitalWrite(BuiltinLED, LOW);
      publish_data(millis(), currentPos);
      digitalWrite(BuiltinLED, HIGH);
      publishTimer = timeNow + (stepperEnabled ? PUBLISH_STEP_SHORT : PUBLISH_STEP_LONG);
    }
    else
    {
      reconnect();
      publishTimer = PUBLISH_STEP_LONG;
    }
  }
  mqttLoop();
  //mqtt_parse_message(&msgFlag, &msgTopic, &msgString);
  mqtt_parse_message();
  button_read();
  stepper_prog();
  yield();
}