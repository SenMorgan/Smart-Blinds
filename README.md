# Smart Blinds Controller

Motorized Smart Roller Blinds Controller with MQTT control and WEB configuration portal for WiFi connection. <br>
The project was created on 07.08.2019 in Arduino IDE and rebuild in PlatformIO 01.09.2021

<br>


[![Build with PlatformIO](https://img.shields.io/badge/Build%20with-PlatformIO-orange)](https://platformio.org/)

[![ESP8266](https://img.shields.io/badge/ESP-8266-000000.svg?longCache=true&style=flat&colorA=AA101F)](https://www.espressif.com/en/products/socs/esp8266)

[![ESP32](https://img.shields.io/badge/ESP-32-000000.svg?longCache=true&style=flat&colorA=AA101F)](https://www.espressif.com/en/products/socs/esp32)

[![License: MIT](https://img.shields.io/badge/License-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

<br>

![](Smart-blinds.gif)

<br>

## Wiring Schematic

![](Smart-Blinds-Breadboard-Scheme.png)

<br>

## Getting Started 

- Upload last relase to your ESPx module.
- When your ESP starts up, it sets it up in Station mode and tries to connect to a previously saved Access Point.
- If this is unsuccessful (or no previous network saved) it moves the ESP into Access Point mode and spins up a DNS and WebServer.
- Using any WiFi enabled device with a browser (computer, phone, tablet) connect to the newly created Access Point.
- Because of the Captive Portal and the DNS server you will either get a 'Join to network' type of popup or get any domain you try to access redirected to the configuration portal. If not, type *192.168.4.1* in your browser.
- Choose one of the access points scanned and enter password.
- Set up your MQTT and stepper motor setting.
- Click save.
- ESP now will try to connect. If successful, it relinquishes control back to your app. If not, reconnect to AP and reconfigure.
- When the ESP has connected to WiFi and MQTT, you can start sending commands by MQTT.

<br>

## MQTT:
 - MQTT and all other default settings you can find in `lib/defs/def.h`
 - State report is provided every `PUBLISH_STEP_LONG` (default **30**) seconds in idle and `PUBLISH_STEP_SHORT` (default **0.5**) seconds when moving.
 - Default command topic to set blinds position (using **0~100%** values) is:
  `"/blinds/set_position"`
 - You can control blinds with *Open*, *Close* and *Stop* commands by using defult topic `"/blinds/set"` and payloads:
 ```yaml
    #define MQTT_CMD_OPEN               "OPEN"
    #define MQTT_CMD_CLOSE              "CLOSE"
    #define MQTT_CMD_STOP               "STOP"
 ```
 - Default state report topic returns the value of actual blinds position from **0%** to **100%**
 ```yaml
    #define MQTT_PUBLISH_TOPIC          "/blinds/position"
 ```


<br>

## Home Assistant YAML configuration
```yaml
cover:
  - platform: mqtt
    name: "Bedroom"
    device_class: shade
    command_topic: "/blinds/set"
    position_topic: "/blinds/position"
    availability:
      - topic: "/blinds/availability"
    set_position_topic: "/blinds/set_position"
    payload_open: "OPEN"
    payload_close: "CLOSE"
    payload_stop: "STOP"
    position_open: 100
    position_closed: 0
    optimistic: false
    position_template: "{{ value }}"
```

<br>

## Dependencies
WiFiManager library v2.0.4-beta https://github.com/tzapu/WiFiManager

Stepper Motor library v1.61 https://github.com/waspinator/AccelStepper

MQTT library v2.8 https://github.com/knolleary/pubsubclient

<br>

## Copyright

Copyright (c) 2021 Sen Morgan. Licensed under the MIT license, see LICENSE.md