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

uint32_t publishTimer = 0, stepperLastTriggered = 0;
bool saveToEEPROMflag = 0;

class FloatParameter : public WiFiManagerParameter
{
public:
  FloatParameter(const char *id, const char *placeholder, float value, const uint8_t length = 10)
      : WiFiManagerParameter("")
  {
    init(id, placeholder, String(value).c_str(), length, "", WFM_LABEL_BEFORE);
  }

  float getValue()
  {
    return String(WiFiManagerParameter::getValue()).toFloat();
  }
};

class IntParameter : public WiFiManagerParameter
{
public:
  IntParameter(const char *id, const char *placeholder, long value, const uint8_t length = 10)
      : WiFiManagerParameter("")
  {
    init(id, placeholder, String(value).c_str(), length, "", WFM_LABEL_BEFORE);
  }

  long getValue()
  {
    return String(WiFiManagerParameter::getValue()).toInt();
  }
};

// EEPROM memory structure
struct
{
  uint32_t savedPos = 0;
  char hostname[32] = "";
  char otaPass[32] = "";
  char mqttServer[32] = "";
  char mqttPort[6] = "";
  char mqttTopic[32] = "";
  char mqttLogin[32] = "";
  char mqttPass[32] = "";
  uint32_t resetCounter = 0;
  uint32_t resetCounterComp = 0;
  long maxPosition = 0;
  float maxSpeed = 0;
  float acceleration = 0;
} params;

// headers to be appended with topics
char willTopic[128] = "";
char subscribeTopic[128] = "";
char cmdTopic[128] = "";
char setPosTopic[128] = "";
char publishTopic[128] = "";
char availabililtyTopic[128] = "";
char uptimeTopic[128] = "";

// Returns 1 if successfully reconnected to MQTT server
bool reconnect()
{
  // connecting to MQTT server
  if (mqttClient.connect(params.hostname, params.mqttLogin, params.mqttPass,
                         willTopic, MQTT_QOS, MQTT_RETAIN, MQTT_WILL_MESSAGE))
  {
    mqttClient.subscribe(subscribeTopic);
    Serial.println("Successfully connected to " +
                   String(params.mqttServer) + ":" + String(params.mqttPort));
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
  if (topic == cmdTopic)
  {
    if (msgString == MQTT_CMD_OPEN)
    {
      stepper.moveTo(params.maxPosition);
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
      // save actual positon, correct driver's zero point and return to saved position
      long actualPosition = stepper.currentPosition();
      stepper.setCurrentPosition(actualPosition - CORRECTION_OFFSET);
      stepper.moveTo(actualPosition);
      Serial.println("Received command CORRECT UP");
    }

    else if (msgString == MQTT_CMD_CORRECT_DOWN)
    {
      // save actual positon, correct driver's zero point and return to saved position
      long actualPosition = stepper.currentPosition();
      stepper.setCurrentPosition(actualPosition + CORRECTION_OFFSET);
      stepper.moveTo(actualPosition);
      Serial.println("Received command CORRECT DOWN");
    }
  }
  // position topic
  else if (topic == setPosTopic)
  {
    stepper.moveTo(map(constrain(msgString.toInt(), 0, 100), 0, 100, 0, params.maxPosition));
    Serial.println("Received go to " + msgString);
  }
}

// Publish data to server
void publish_data()
{
  static char buff[20];

  // publish current position to server in 0~100% range
  sprintf(buff, "%ld", map(stepper.currentPosition(), 0, params.maxPosition, 0, 100));
  mqttClient.publish(publishTopic, buff, true);

  mqttClient.publish(availabililtyTopic, MQTT_AVAILABILITY_MESSAGE);
  sprintf(buff, "%ld sec", millis() / 1000);
  mqttClient.publish(uptimeTopic, buff);
  Serial.println("Data was sended, time from start: " + String(buff));
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
        stepper.moveTo(params.maxPosition);
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
      stepperLastTriggered = millis();
      stepperMode++;
    }
    break;
  case 2: // antidrift mode - holding motor for some time to prevent drifting
    // if target position changed - go back to mode 1
    if (stepper.distanceToGo() != 0)
      stepperMode = 1;
    // when timer ended - stop stepper holding and save new position
    else if (millis() - stepperLastTriggered > AFTER_STOP_DELAY)
    {
      stepper.disableOutputs();
      params.savedPos = stepper.currentPosition();
      EEPROM.put(0, params);
      EEPROM.commit();
      stepperMode = 0;
      stepperLastTriggered = millis();
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
    strcpy(params.mqttTopic, DEFAULT_TOPIC);
    params.maxPosition = DEFAULT_MAX_POSITION;
    params.maxSpeed = DEFAULT_STEPPER_MAX_SPEED;
    params.acceleration = DEFAULT_STEPPER_ACCELERATION;
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
  stepper.setCurrentPosition(params.savedPos);
  // print saved data
  Serial.println("Reading from EEPROM...");
  Serial.println("Hostname: " + String(params.hostname));
  Serial.println("OTA password: " + String(params.otaPass));
  Serial.println("MQTT server: " + String(params.mqttServer));
  Serial.println("MQTT port: " + String(params.mqttPort));
  Serial.println("MQTT topic: " + String(params.mqttTopic));
  Serial.println("MQTT login: " + String(params.mqttLogin));
  Serial.println("MQTT password: " + String(params.mqttPass));
  Serial.println("Resets count: " + String(params.resetCounter));
  Serial.println("Resets count compare: " + String(params.resetCounterComp));
  Serial.println("Maximum position: " + String(params.maxPosition));
  Serial.println("Maximum speed: " + String(params.maxSpeed));
  Serial.println("Acceleration: " + String(params.acceleration));

  // setup some web configuration parameters
  WiFiManagerParameter custom_device_settings("<p>Device Settings</p>");
  WiFiManagerParameter custom_hostname("hostname", "Device Hostname", params.hostname, 32);
  WiFiManagerParameter custom_ota_pass("otaPass", "OTA Password", params.otaPass, 32);

  WiFiManagerParameter custom_mqtt_settings("<p>MQTT Settings</p>");
  WiFiManagerParameter custom_mqtt_server("mqttServer", "MQTT Server", params.mqttServer, 32);
  WiFiManagerParameter custom_mqtt_port("mqttPort", "MQTT Port", params.mqttPort, 6);
  WiFiManagerParameter custom_mqtt_topic("mqttTopic", "MQTT Topic", params.mqttTopic, 32);
  WiFiManagerParameter custom_mqtt_login("mqttLogin", "MQTT Login", params.mqttLogin, 32);
  WiFiManagerParameter custom_mqtt_pass("mqttPass", "MQTT Password", params.mqttPass, 32);

  WiFiManagerParameter custom_stepper_settings("<p>Stepper Settings</p>");
  IntParameter custom_max_position("max_poss", "Maximum Stepper Position", params.maxPosition);
  FloatParameter custom_max_speed("max_speed", "Maximum Stepper Speed", params.maxSpeed);
  FloatParameter custom_acceleration("acceleration", "Stepper Acceleration", params.acceleration);

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
  WiFiMan.addParameter(&custom_stepper_settings);
  WiFiMan.addParameter(&custom_max_position);
  WiFiMan.addParameter(&custom_max_speed);
  WiFiMan.addParameter(&custom_acceleration);

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
  // if new params in config portal was chenged - save them to EEPROM
  if (saveToEEPROMflag)
  {
    strcpy(params.hostname, custom_hostname.getValue());
    strcpy(params.otaPass, custom_ota_pass.getValue());
    strcpy(params.mqttServer, custom_mqtt_server.getValue());
    strcpy(params.mqttPort, custom_mqtt_port.getValue());
    strcpy(params.mqttTopic, custom_mqtt_topic.getValue());
    strcpy(params.mqttLogin, custom_mqtt_login.getValue());
    strcpy(params.mqttPass, custom_mqtt_pass.getValue());

    params.maxPosition = custom_max_position.getValue();
    params.maxSpeed = custom_max_speed.getValue();
    params.acceleration = custom_acceleration.getValue();

    EEPROM.put(0, params);
    EEPROM.commit();
    Serial.println("Saved!");
  }

  // appending topics
  strcpy(willTopic, params.mqttTopic);
  strcpy(subscribeTopic, params.mqttTopic);
  strcpy(cmdTopic, params.mqttTopic);
  strcpy(setPosTopic, params.mqttTopic);
  strcpy(publishTopic, params.mqttTopic);
  strcpy(availabililtyTopic, params.mqttTopic);
  strcpy(uptimeTopic, params.mqttTopic);

  strcat(willTopic, MQTT_WILL_TOPIC);
  strcat(subscribeTopic, MQTT_SUBSCRIBE_TOPIC);
  strcat(cmdTopic, MQTT_CMD_TOPIC);
  strcat(setPosTopic, MQTT_SET_POSITION_TOPIC);
  strcat(publishTopic, MQTT_PUBLISH_TOPIC);
  strcat(availabililtyTopic, MQTT_AVAILABILITY_TOPIC);
  strcat(uptimeTopic, MQTT_UPTIME_TOPIC);

// MQTT initializing
  mqttClient.setServer(params.mqttServer, atoi(params.mqttPort));
  mqttClient.setCallback(callback);
  Serial.println("Connecting to MQTT server...");
  reconnect();

  // Arduino OTA initializing
  ArduinoOTA.setHostname(params.hostname);
  ArduinoOTA.setPassword(params.otaPass);
  ArduinoOTA.begin();
  ArduinoOTA.onProgress([](uint16_t progress, uint16_t total)
                        { digitalWrite(STATUS_LED, !digitalRead(STATUS_LED)); });
  ArduinoOTA.onEnd([]()
                   { digitalWrite(STATUS_LED, 1); });

  // stepper driver initializing
  stepper.setEnablePin(PIN_ENABLE);
  stepper.setPinsInverted(false, false, false, false, true);
  stepper.setMaxSpeed(params.maxSpeed);
  stepper.setAcceleration(params.acceleration);

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
      digitalWrite(STATUS_LED, 0); // blink buildin LED
      publish_data();              // publish data
      digitalWrite(STATUS_LED, 1); // switch off buildin LED

      publishTimer = // save next publish time depending on actual stepper state
          timeNow + ((stepper.distanceToGo() != 0) ? PUBLISH_STEP_SHORT : PUBLISH_STEP_LONG);
    }
    // we can try to reconnect only if stepper isn't moving
    else if (!stepper.distanceToGo() && timeNow - stepperLastTriggered > RECONNECT_DELAY)
    {
      reconnect();
      // increasing timer period during reconnecting
      publishTimer = timeNow + PUBLISH_STEP_LONG;
    }
  }
  buttons_read();
  stepper_prog();
  yield();
}