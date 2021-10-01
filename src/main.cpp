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
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <AccelStepper.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>

WiFiManager WiFiMan;
AccelStepper stepper(1, PIN_STEP, PIN_DIR);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

uint32_t publishTimer = 0;
bool saveToEEPROMflag = 0;

// EEPROM memory structure
struct
{
  uint32_t targetPos = 0;
  char hostname[32] = "";
  char ota_pass[32] = "";
  char mqtt_server[32] = "";
  char mqtt_port[6] = "";
  char mqtt_topic[32] = "";
  char mqtt_login[32] = "";
  char mqtt_pass[32] = "";
  uint32_t resetCounter = 0;
  uint32_t resetCounterComp = 0;
} params;

// Returns 1 if successfully reconnected to MQTT server
bool reconnect(const char *device, const char *id, const char *user)
{
  if (mqttClient.connect(device, id, user, MQTT_WILL_TOPIC, MQTT_QOS, MQTT_RETAIN, MQTT_WILL_MESSAGE))
  {
    mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC);
    Serial.println("Successfully connected to " +
                   String(params.mqtt_server) + ":" + String(params.mqtt_port));
    return 1;
  }
  Serial.println("Can't connect to MQTT server...");
  return 0;
}

// Saving all received data
void callback(String topic, byte *payload, uint16_t length)
{
  String msgString = "";
  for (uint16_t i = 0; i < length; i++)
    msgString += (char)payload[i];

  // commands topic
  if (topic == MQTT_CMD_TOPIC)
  {
    if (msgString == MQTT_CMD_OPEN)
    {
      stepper.moveTo(MAX_POSITION);
      Serial.println("Received command OPEN");
    }
    else if (msgString == MQTT_CMD_CLOSE)
    {
      stepper.moveTo(0);
      Serial.println("Received command CLOSE");
    }
    else if (msgString == MQTT_CMD_STOP)
    {
      stepper.stop();
      Serial.println("Received command STOP");
    }
    else if (msgString == MQTT_CMD_CORRECT_UP)
    {
      // save actual positon, correct zero point and return to saved position
      uint16_t actualPosition = stepper.currentPosition();
      stepper.setCurrentPosition(stepper.currentPosition() - CORRECTION_OFFSET);
      stepper.moveTo(actualPosition);
      Serial.println("Received command CORRECT UP");
    }

    else if (msgString == MQTT_CMD_CORRECT_DOWN)
    {
      // save actual positon, correct zero point and return to saved position
      uint16_t actualPosition = stepper.currentPosition();
      stepper.setCurrentPosition(stepper.currentPosition() + CORRECTION_OFFSET);
      stepper.moveTo(actualPosition);
      Serial.println("Received command CORRECT DOWN");
    }
  }
  // position topic
  else if (topic == MQTT_SET_POSITION_TOPIC)
  {
    stepper.moveTo(map(constrain(msgString.toInt(), 0, 100), 0, 100, 0, MAX_POSITION));
    Serial.println("Received go to " + msgString);
  }
}

// Publish data to server
void publish_data()
{
  static char buff[20];
  // publish current position to server in 0~100% range
  sprintf(buff, "%ld", map(stepper.currentPosition(), 0, MAX_POSITION, 0, 100));
  mqttClient.publish(MQTT_PUBLISH_TOPIC, buff, true);

  mqttClient.publish(MQTT_AVAILABILITY_TOPIC, MQTT_AVAILABILITY_MESSAGE);
  sprintf(buff, "%ld sec", millis() / 1000);
  mqttClient.publish(MQTT_UPTIME_TOPIC, buff);
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

// Callback notifying us of the need to save config
void saveConfigCallback()
{
  Serial.println("Saving to EEPROM...");
  saveToEEPROMflag = true;
  WiFiMan.stopConfigPortal();
}

// Clearing all saved values (WiFi, hostname, MQTT)
void clear_setting_memory()
{
  for (int i = 0; i < 512; i++)
  {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  delay(400);
  WiFiMan.resetSettings();
  delay(100);
}

void setup(void)
{
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, 0);

  EEPROM.begin(512);
  // read data from EEPROM
  EEPROM.get(0, params);

  // increment reset counter and save it to EEPROM
  params.resetCounter++;
  EEPROM.put(0, params);
  EEPROM.commit();

  // some delay for the user to be able to call EEPROM erase command
  // by pressing reset button a few times
  delay(RESET_BUTTON_DELAY);

  Serial.begin(115200);
  Serial.println("\n Starting");

  // if it is first boot or if reset counter is empty (cleaned EEPROM)
  if (params.resetCounter == 1)
  {
    Serial.println("Setting default values");
    params.resetCounterComp = params.resetCounter;
    // set and save default hostname
    strcpy(params.hostname, DEFAULT_HOSTNAME);
    strcpy(params.mqtt_topic, DEFAULT_TOPIC);
    EEPROM.put(0, params);
    EEPROM.commit();
    delay(100);
  }
  // if reset buton was pressed more than RESET_BUTTON_CYCLES times quickly
  else if ((params.resetCounter - params.resetCounterComp) >= RESET_BUTTON_CYCLES)
  {
    Serial.println("Erasing WiFi and MQTT settings!");
    clear_setting_memory();
    // blink buildin LED many times
    for (int i = 0; i < 50; i++)
    {
      digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
      delay(50);
    }
    ESP.restart();
  }
  // if reset button wasn't pressed enough quicly - save same value
  params.resetCounterComp = params.resetCounter;
  EEPROM.put(0, params);
  EEPROM.commit();

  // set actual stepper position as saved one
  stepper.setCurrentPosition(params.targetPos);
  Serial.println("Reading from EEPROM...");
  Serial.println("Hostname: " + String(params.hostname));
  Serial.println("OTA password: " + String(params.ota_pass));
  Serial.println("MQTT server: " + String(params.mqtt_server));
  Serial.println("MQTT port: " + String(params.mqtt_port));
  Serial.println("MQTT topic: " + String(params.mqtt_topic));
  Serial.println("MQTT login: " + String(params.mqtt_login));
  Serial.println("MQTT password: " + String(params.mqtt_pass));
  Serial.println("Resets count: " + String(params.resetCounter));
  Serial.println("Resets count compare: " + String(params.resetCounterComp));

  // setup some web configuration parameters
  WiFiManagerParameter custom_device_settings("<p>Device Settings</p>");
  WiFiManagerParameter custom_hostname("hostname", "Device Hostname", params.hostname, 32);
  WiFiManagerParameter custom_ota_pass("ota_pass", "OTA Password", params.ota_pass, 32);

  WiFiManagerParameter custom_mqtt_settings("<p>MQTT Settings</p>"); // only custom html
  WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT Server", params.mqtt_server, 32);
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "MQTT Port", params.mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_topic("mqtt_topic", "MQTT Topic", params.mqtt_topic, 32);
  WiFiManagerParameter custom_mqtt_login("mqtt_login", "MQTT Login", params.mqtt_login, 32);
  WiFiManagerParameter custom_mqtt_pass("mqtt_pass", "MQTT Password", params.mqtt_pass, 32);

  // callbacks
  WiFiMan.setSaveConfigCallback(saveConfigCallback);

  // add all parameters
  WiFiMan.addParameter(&custom_device_settings);
  WiFiMan.addParameter(&custom_hostname);
  WiFiMan.addParameter(&custom_ota_pass);
  WiFiMan.addParameter(&custom_mqtt_settings);
  WiFiMan.addParameter(&custom_mqtt_server);
  WiFiMan.addParameter(&custom_mqtt_port);
  WiFiMan.addParameter(&custom_mqtt_topic);
  WiFiMan.addParameter(&custom_mqtt_login);
  WiFiMan.addParameter(&custom_mqtt_pass);

  // invert theme, dark
  WiFiMan.setDarkMode(true);
  // show scan RSSI as percentage, instead of signal stength graphic
  WiFiMan.setScanDispPerc(true);
  // set hostname as saved one
  WiFiMan.setHostname(params.hostname);
  // sets timeout until configuration portal gets turned off
  WiFiMan.setConfigPortalTimeout(180);
  // needed to use saveWifiCallback
  WiFiMan.setBreakAfterConfig(true);
  // use autoconnect and start web portal
  if (!WiFiMan.autoConnect(params.hostname))
  {
    Serial.println("failed to connect and hit timeout");
  }
  // if new data was saved from config portal - parse them and save to EEPROM
  if (saveToEEPROMflag)
  {
    strcpy(params.hostname, custom_hostname.getValue());
    strcpy(params.ota_pass, custom_ota_pass.getValue());
    strcpy(params.mqtt_server, custom_mqtt_server.getValue());
    strcpy(params.mqtt_port, custom_mqtt_port.getValue());
    strcpy(params.mqtt_topic, custom_mqtt_topic.getValue());
    strcpy(params.mqtt_login, custom_mqtt_login.getValue());
    strcpy(params.mqtt_pass, custom_mqtt_pass.getValue());

    EEPROM.put(0, params);
    EEPROM.commit();
    Serial.println("Saved!");
  }

  mqttClient.setServer(params.mqtt_server, atoi(params.mqtt_port));
  mqttClient.setCallback(callback);
  Serial.println("Connecting to MQTT server...");
  reconnect(params.hostname, params.mqtt_login, params.mqtt_pass);

  // Arduino OTA initialization
  ArduinoOTA.setHostname(params.hostname);
  ArduinoOTA.setPassword(params.ota_pass);
  ArduinoOTA.begin();
  ArduinoOTA.onProgress([](uint16_t progress, uint16_t total)
                        { digitalWrite(STATUS_LED, !digitalRead(STATUS_LED)); });
  ArduinoOTA.onEnd([]()
                   { digitalWrite(STATUS_LED, 1); });

  // stepper driver initialization
  stepper.setEnablePin(PIN_ENABLE);
  stepper.setPinsInverted(false, false, false, false, true);
  stepper.setMaxSpeed(STEPPER_MAX_SPEED);
  stepper.setAcceleration(STEPPER_ACCELERATION);

  // make some sound and blink to know that blinds are ready to work
  stepper.enableOutputs();
  for (int i = 0; i < 10; i++)
  {
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
    delay(100);
  }
  digitalWrite(STATUS_LED, 1);
  stepper.disableOutputs();
}

void loop(void)
{
  ArduinoOTA.handle();

  uint32_t timeNow = millis();
  bool connectedToServer = mqttClient.loop();

  // if it is time to publish data
  if (timeNow > publishTimer)
  {
    // if we are connected to MQTT server
    if (connectedToServer)
    {
      digitalWrite(STATUS_LED, 0);  // blink buildin LED
      publish_data();                 // publish data
      digitalWrite(STATUS_LED, 1); // switch off buildin LED

      publishTimer = // save next publish time depending on actual stepper state
          timeNow + ((stepper.distanceToGo() != 0) ? PUBLISH_STEP_SHORT : PUBLISH_STEP_LONG);
    }
    // we can try to reconnect only if stepper isn't moving
    else if (!stepper.distanceToGo())
    {
 //     clear_setting_memory();
 //     ESP.restart();

      reconnect(params.hostname, params.mqtt_login, params.mqtt_pass);
      // increasing timer period during reconnection process
      publishTimer = timeNow + PUBLISH_STEP_LONG;
    }
  }
  buttons_read();
  stepper_prog();
  yield();
}