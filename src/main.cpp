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

WiFiManager WiFiMan;
AccelStepper stepper(1, PIN_STEP, PIN_DIR);

uint32_t publishTimer = 0;
bool saveToEEPROMflag = 0;

struct
{
  uint32_t targetPos = 0;
  char hostname[32] = "";
  char ota_pass[32] = "";
  char mqtt_login[32] = "";
  char mqtt_pass[32] = "";

  char mqtt_server[40] = "";
  char mqtt_port[6] = "";
} params;

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
      params.targetPos = stepper.currentPosition();
      EEPROM.put(0, params);
      EEPROM.commit();
      stepperMode = 0;
    }
    break;
  }
}

//callback notifying us of the need to save config
void saveConfigCallback()
{
  Serial.println("Saving to EEPROM...");
  saveToEEPROMflag = true;
}

void wifiInfo()
{
  WiFi.printDiag(Serial);
  Serial.println("SAVED: " + (String)WiFiMan.getWiFiIsSaved() ? "YES" : "NO");
  Serial.println("SSID: " + (String)WiFiMan.getWiFiSSID());
  Serial.println("PASS: " + (String)WiFiMan.getWiFiPass());
}

void saveWifiCallback()
{
  Serial.println("[CALLBACK] saveCallback fired");
}

//gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("[CALLBACK] configModeCallback fired");
  // myWiFiManager->setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  // Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  // Serial.println(myWiFiManager->getConfigPortalSSID());
  //
  // esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
}

void saveParamCallback()
{
  Serial.println("[CALLBACK] saveParamCallback fired");
  // WiFiMan.stopConfigPortal();
}

void handleRoute()
{
  Serial.println("[HTTP] handle route");
  WiFiMan.server->send(200, "text/plain", "hello from user code");
}

void bindServerCallback()
{
  WiFiMan.server->on("/custom", handleRoute);
  // WiFiMan.server->on("/info",handleRoute); // you can override wm!
}

void setup(void)
{
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  Serial.println("\n Starting");

  EEPROM.begin(128);

  EEPROM.get(0, params);
  stepper.setCurrentPosition(params.targetPos);

  Serial.println("Found: " + String(params.targetPos) + "," +
                 String(params.mqtt_server) + "," + String(params.mqtt_port));

  // setup some parameters
  WiFiManagerParameter custom_device_settings("<p>Device Settings</p>");
  WiFiManagerParameter custom_hostname("hostname", "Device Hostname", params.hostname, 32);
  WiFiManagerParameter custom_custom_ota_pass("ota_pass", "OTA Password", params.ota_pass, 32);

  WiFiManagerParameter custom_mqtt_settings("<p>MQTT Settings</p>"); // only custom html
  WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT Server", "", 40);
  //WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT Server", params.mqtt_server, 15, "pattern='\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}'");
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "MQTT Port", params.mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_login("mqtt_login", "MQTT Login", params.mqtt_login, 32);
  WiFiManagerParameter custom_mqtt_pass("mqtt_pass", "MQTT Password", params.mqtt_pass, 32);

  // callbacks
  WiFiMan.setSaveConfigCallback(saveConfigCallback);
 // WiFiMan.setConfigPortalBlocking(false);

  // add all your parameters here
  WiFiMan.addParameter(&custom_device_settings);
  WiFiMan.addParameter(&custom_hostname);
  WiFiMan.addParameter(&custom_custom_ota_pass);
  WiFiMan.addParameter(&custom_mqtt_settings);
  WiFiMan.addParameter(&custom_mqtt_server);
  WiFiMan.addParameter(&custom_mqtt_port);
  WiFiMan.addParameter(&custom_mqtt_login);
  WiFiMan.addParameter(&custom_mqtt_pass);

  // invert theme, dark
  WiFiMan.setDarkMode(true);

  // show scan RSSI as percentage, instead of signal stength graphic
  WiFiMan.setScanDispPerc(true);

  // std::vector<const char *> menu = {"wifi", "wifinoscan", "info", "param", "close", "sep", "erase", "update", "restart", "exit"};
  // WiFiMan.setMenu(menu); // custom menu, pass vector

  WiFiMan.setHostname(params.hostname);
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep in seconds
  WiFiMan.setConfigPortalTimeout(120);
  // connect after portal save toggle
  WiFiMan.setSaveConnect(false); // do not connect, only save

  WiFiMan.setBreakAfterConfig(true); // needed to use saveWifiCallback

  wifiInfo();

  if (!WiFiMan.autoConnect("WM_AutoConnectAP", "12345678"))
  {
    Serial.println("failed to connect and hit timeout");
  }

  wifiInfo();

  if (saveToEEPROMflag)
  {
    strcpy(params.hostname, custom_hostname.getValue());
    strcpy(params.ota_pass, custom_custom_ota_pass.getValue());
    strcpy(params.mqtt_server, custom_mqtt_server.getValue());
    strcpy(params.mqtt_port, custom_mqtt_port.getValue());
    strcpy(params.mqtt_login, custom_mqtt_login.getValue());
    strcpy(params.mqtt_pass, custom_mqtt_pass.getValue());

    //replace values in EEPROM
    EEPROM.put(0, params);
    EEPROM.commit();
    Serial.println("Saved!");
  }

  // if false then the configportal will be in non blocking loop
  // WiFiMan.setConfigPortalBlocking(false);
  // WiFiMan.autoConnect(HOSTNAME);

  init_MQTT(params.mqtt_server, atoi(params.mqtt_port));
  ArduinoOTA.setHostname(params.hostname);
  ArduinoOTA.begin();
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onProgress([](uint16_t progress, uint16_t total)
                        { digitalWrite(STATUS_LED, !digitalRead(STATUS_LED)); });
  ArduinoOTA.onEnd([]()
                   { digitalWrite(STATUS_LED, 1); });

  stepper.setEnablePin(PIN_ENABLE);
  stepper.setPinsInverted(false, false, false, false, true);
  stepper.setMaxSpeed(STEPPER_MAX_SPEED);
  stepper.setAcceleration(STEPPER_ACCELERATION);

  // make some sound noize to know that blinds are ready to work
  stepper.enableOutputs();
  delay(1000);
  stepper.disableOutputs();
}

void loop(void)
{
  // for testing
  static uint32_t tim = 0;
  if (millis() > tim)
  {
    tim = millis() + 200;
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
  }
  // for testing

  ArduinoOTA.handle();
  // if stepper isn't moving than we can process configportal
  if (!stepper.distanceToGo())
    WiFiMan.process();

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
    // we can try to reconnect only if stepper isn't moving
    else if (!stepper.distanceToGo())
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