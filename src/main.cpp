/**************************************************************
 *  main.cpp
 *
 *  Created on: 07 August 2019
 *  Rebuild in PlatformIO: 01 September 2021
 *  Author: SenMorgan https://github.com/SenMorgan
 *  
 ***************************************************************/

#include "def.h"
//#include "mqtt.h"
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <AccelStepper.h>
#include <PubSubClient.h>
#include <EEPROM.h>


AccelStepper stepper(1, PIN_STEP, PIN_DIR);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long reconnectTimer = 0, publishTimer = 0;
byte currentPos = 0, targetPos = 0;
boolean holdPos = 0, stepperEnabled = 0;

void publish_data(void)
{
  static char buff[20];

  if (millis() > publishTimer)
  {
    if (!stepperEnabled)
      publishTimer = millis() + PUBLISH_STEP;
    else
      publishTimer = millis() + 1000;

    digitalWrite(BuiltinLED, LOW);
    sprintf(buff, "%d", currentPos);
    client.publish("/Cover/position", buff, true);
    client.publish("/Cover/availability", "online");
    sprintf(buff, "%ld sec", millis() / 1000);
    client.publish("/Cover/uptime", buff);
    digitalWrite(BuiltinLED, HIGH);
  }
}

void callback(String topic, byte *payload, uint16_t length)
{
  String payloadString = "";
  //Serial.println("Topic: [" + topic + "]");
  for (uint16_t i = 0; i < length; i++)
    payloadString += (char)payload[i];
  //Serial.println("Payload: [" + payloadString + "]\n");

  if (topic == "/Cover/set")
  {
    if (payloadString == "OPEN")
      targetPos = 100;
    else if (payloadString == "CLOSE")
      targetPos = 0;
    else if (payloadString == "STOP")
      holdPos = true;
  }
  else if (topic == "/Cover/set_position")
  {
    targetPos = constrain(payloadString.toInt(), 0, 100);
    //Serial.print("Mqtt set position: "); Serial.println(targetPos);
  }
}

void reconnect(void)
{
  if (client.connect("Cover", "login", "password", "/Cover/availability", 1, true, "offline"))
    client.subscribe("/Cover/#");
  else
    reconnectTimer = millis() + 5000;
}


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

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  reconnect();

  stepper.setEnablePin(PIN_ENABLE);
  stepper.setPinsInverted(false, false, false, false, true);
  stepper.setMaxSpeed(2000.0);
  stepper.setAcceleration(1300.0);
}

void loop()
{
  if (millis() > reconnectTimer && !stepperEnabled)
  {
    if (!client.connected())
      reconnect();
  }
  client.loop();

  button_read();
  stepper_prog();
  
  publish_data();
  yield();
}