/**************************************************************
 *  main.cpp
 *
 *  Created on: 01 September 2021
 *  Author: SenMorgan https://github.com/SenMorgan
 *  
 ***************************************************************/

#include "def.h"
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <AccelStepper.h>
#include <PubSubClient.h>

AccelStepper stepper(1, PIN_STEP, PIN_DIR);

void setup()
{

  Serial.begin(115200);

  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the "Smart Blinds" name
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("Smart Blinds");
  // continue if conected

  stepper.setMaxSpeed(4000.0);
  stepper.setAcceleration(2000.0); //Make the acc quick
}

void loop()
{
}