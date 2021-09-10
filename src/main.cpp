/**************************************************************
 *  main.cpp
 *
 *  Created in Arduino IDE: 07 August 2019
 *  Rebuild in PlatformIO: 01 September 2021
 *  Author: SenMorgan https://github.com/SenMorgan
 * 
 *  All definitions are in /lib/defs/def.h
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
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>

AccelStepper stepper(1, PIN_STEP, PIN_DIR);

uint32_t publishTimer = 0;

// Checking if there is a new message and parsing it
void mqtt_parse_message(void)
{
  if (msgFlag)
  {
    //  reset flag
    msgFlag = 0;

    // if command topic received
    if (msgTopic == MQTT_CMD_TOPIC)
    {
      if (msgString == MQTT_CMD_OPEN)
        stepper.moveTo(MAX_POSITION);
      else if (msgString == MQTT_CMD_CLOSE)
        stepper.moveTo(0);
      else if (msgString == MQTT_CMD_STOP)
        stepper.stop();
      else if (msgString == MQTT_CMD_CORRECT_UP)
      {
        // save actual positon, correct zero point and return to saved position
        uint16_t actualPosition = stepper.currentPosition();
        stepper.setCurrentPosition(stepper.currentPosition() - CORRECTION_OFFSET);
        stepper.moveTo(actualPosition);
      }
      else if (msgString == MQTT_CMD_CORRECT_DOWN)
      {
        // save actual positon, correct zero point and return to saved position
        uint16_t actualPosition = stepper.currentPosition();
        stepper.setCurrentPosition(stepper.currentPosition() + CORRECTION_OFFSET);
        stepper.moveTo(actualPosition);
      }
    }
    // if position topic received
    else if (msgTopic == MQTT_SET_POSITION_TOPIC)
    {
      stepper.moveTo(map(constrain(msgString.toInt(), 0, 100), 0, 100, 0, MAX_POSITION));
    }
  }
}

// Reading buttons states
void buttons_read(void)
{
  static uint32_t lastReadTimeStamp, buttonPressedTimeStamp;
  // button state flag
  static bool buttonUpReleased, buttonDownReleased;

  // if it is time to read buttons states
  if (millis() - lastReadTimeStamp >= READ_BUTTONS_STEP)
  {
    lastReadTimeStamp = millis();
    // if button UP pressed
    if (digitalRead(BUTTON_1) && buttonUpReleased)
    {
      // set the flag to pevent entering more times
      buttonUpReleased = false;
      // if stepper isn't moving OR buttons was toggled in CHANGE_DIRRECTION_DELAY period
      if (!stepper.distanceToGo() || millis() - buttonPressedTimeStamp <= CHANGE_DIRRECTION_DELAY)
      {
        buttonPressedTimeStamp = millis();
        stepper.moveTo(MAX_POSITION);
      }
      else
        stepper.stop();
    }
    // reset flag if button released
    else if (!digitalRead(BUTTON_1) && !buttonUpReleased)
      buttonUpReleased = true;
    // if button DOWN pressed
    else if (digitalRead(BUTTON_2) && buttonDownReleased)
    {
      // set the flag to pevent entering more times
      buttonDownReleased = false;
      // if stepper isn't moving OR buttons was toggled in CHANGE_DIRRECTION_DELAY period
      if (!stepper.distanceToGo() || millis() - buttonPressedTimeStamp <= CHANGE_DIRRECTION_DELAY)
      {
        buttonPressedTimeStamp = millis();
        stepper.moveTo(0);
      }
      else
        stepper.stop();
    }
    // reset flag if button released
    else if (!digitalRead(BUTTON_2) && !buttonDownReleased)
      buttonDownReleased = true;
  }
}

// Controlling stepper modes and saving position to EEPROM
void stepper_prog(void)
{
  static uint32_t stoppedTimeStamp = 0;
  static uint8_t stepperMode = 0;

  switch (stepperMode)
  {
  case 0: // idle positon - not moving
    // waiting for target position to be changed
    if (stepper.distanceToGo() != 0)
    {
      stepper.enableOutputs();
      // reset timer to allow faster publishing
      publishTimer = 0;
      stepperMode++;
    }
    break;
  case 1: // active mode - stepper is running
    // making stepper to move and checking if the target position reached
    if (!stepper.run())
    {
      stoppedTimeStamp = millis();
      stepperMode++;
    }
    break;
  case 2: // antidrift mode - holding motor for some time to prevent drifting
    // if target position changed - go back to mode 1
    if (stepper.distanceToGo() != 0)
      stepperMode = 1;
    // when timer ended - stop stepper holding and save new position
    else if (millis() - stoppedTimeStamp > AFTER_STOP_DELAY)
    {
      stepper.disableOutputs();
      EEPROM.put(0, stepper.currentPosition());
      EEPROM.commit();
      stepperMode = 0;
    }
    break;
  }
}

void setup(void)
{
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  // read saved stepper position
  uint32_t targetPos = 0;
  EEPROM.begin(4);
  EEPROM.get(0, targetPos);
  stepper.setCurrentPosition(targetPos);

  Serial.begin(115200);

  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the "Smart Blinds" name
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect(HOSTNAME);
  // continue if conected

  init_MQTT(MQTT_SERVER, MQTT_PORT);
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.begin();
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onProgress([](uint16_t progress, uint16_t total)
                        { digitalWrite(STATUS_LED, !digitalRead(STATUS_LED)); });
  ArduinoOTA.onEnd([]()
                   { digitalWrite(STATUS_LED, 1); });

  stepper.setEnablePin(PIN_ENABLE);
  stepper.setPinsInverted(false, false, false, false, true);
  stepper.setMaxSpeed(2000.0);
  stepper.setAcceleration(1300.0);
}

void loop(void)
{
  ArduinoOTA.handle();
  uint32_t timeNow = millis();
  bool connectedToServer = mqttLoop();

  // if it is time to publish data
  if (timeNow > publishTimer)
  {
    // if we are connected to MQTT server
    if (connectedToServer)
    {
      digitalWrite(STATUS_LED, LOW); // blink buildin LED
      publish_data(millis(),         // publish current position to server in 0~100% range
                   map(stepper.currentPosition(), 0, MAX_POSITION, 0, 100));
      digitalWrite(STATUS_LED, HIGH); // switch off buildin LED

      publishTimer = // save next publish time depending on actual stepper state
          timeNow + ((stepper.distanceToGo() != 0) ? PUBLISH_STEP_SHORT : PUBLISH_STEP_LONG);
    }
    else
    {
      reconnect();
      // increasing timer period during reconnection process
      publishTimer = timeNow + PUBLISH_STEP_LONG;
    }
  }

  mqtt_parse_message();
  buttons_read();
  stepper_prog();
  yield();
}