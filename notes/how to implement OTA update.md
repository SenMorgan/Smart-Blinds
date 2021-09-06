Add into sketch:
    void setup(void)
    {
        ArduinoOTA.setHostname("Smart Blinds");
        ArduinoOTA.begin();
    }
    void loop(void )
    {
        ArduinoOTA.handle();
    }


In PlatformIO paste in terminal:
    pio run -t upload --upload-port 192.168.0.255

OR

In PlatformIO in platformio.ini paste:
    upload_protocol = espota
    upload_port = *ip or hostname*
    upload_flags =
     --port=8266
     --auth=*OTA password*