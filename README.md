# Smart-Blinds

[![Build with PlatformIO](https://img.shields.io/badge/Build%20with-PlatformIO-orange)](https://platformio.org/)

[![ESP8266](https://img.shields.io/badge/ESP-8266-000000.svg?longCache=true&style=flat&colorA=AA101F)](https://www.espressif.com/en/products/socs/esp8266)

[![ESP32](https://img.shields.io/badge/ESP-32-000000.svg?longCache=true&style=flat&colorA=AA101F)](https://www.espressif.com/en/products/socs/esp32)

[![License: MIT](https://img.shields.io/badge/License-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

Motorized Smart Roller Blinds Controller with MQTT control and WEB configuration portal for WiFi connection.

Open this project in [PlatforIO](https://platformio.org/)

All settings (motor and MQTT) are hardcoded in ``/lib/defs/def.h``

Also you need to create ``/lib/defs/secrets.h`` file and paste in there:

```cpp
#ifndef SECRETS_h
#define SECRETS_h

#define MQTT_LOGIN     "your_login"
#define MQTT_PASSWORD  "your_password"
#define OTA_PASS       "your_OTA_password"


#endif /* SECRETS_h */
```
:warning:Don't forget to write your own credentials!:warning:

Build this project and upload to yours ESPx module.

Project was created 07.08.2019 in Arduino IDE and rebuild in PlatformIO 01.09.2021




## Dependencies
Stepper Motor library v.1.61 https://github.com/waspinator/AccelStepper

MQTT library v2.8 https://github.com/knolleary/pubsubclient

## Copyright

Copyright (c) 2021 Sen Morgan. Licenced under the MIT licence, see LICENSE.md