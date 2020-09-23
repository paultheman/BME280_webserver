## BME280 webserver
 
- Arduino mega 2560 or other development board with enough RAM and flash
- Ethernet shield
- BME280 using I2C
- OLED 128x64 screen using I2C
- Data is uploaded to ThingSpeak using MQTT protocol

### Highlights
 
- The webserver uses Base64 authentication, this is a basic security for your account.  
**Base64 encoding is reversible. Do not show your encoding online.**  
**Arduino mega does not support TLS  (https). If you are connecting from public networks your webserver credentials may be exposed.**
- The code uses milis() instead of delay()
- MQTT protocol should be faster, more precise and use less energy than Rest API
- The index htm file is developed by [neih10](https://github.com/neilh10/thingspeakmultichannel-highcharts). I just updated the ThingSpeak api. 

 ### Images
 [1](https://user-images.githubusercontent.com/38941045/94047099-475a0080-fdda-11ea-8e4f-b57cfae7c721.JPG) [2](https://user-images.githubusercontent.com/38941045/94045345-019c3880-fdd8-11ea-8207-fe2e44a35cfb.JPG) [3](https://user-images.githubusercontent.com/38941045/94047186-68225600-fdda-11ea-987d-9ecd89f9da20.JPG) [4](https://user-images.githubusercontent.com/38941045/94045875-a3238a00-fdd8-11ea-9e04-b4bbd852f6b1.JPG)

### Installation instructions
- Connect the pins as in the following table (you will need to join together the VCC with VIN and both SCL and SDA)

Arduino    |   3.3 V  | GND |  20 | 21
---------- | -------- | --- | --- | ---
**BME280** |   VIN    | GND | SDA | SCL
**OLED**   |   VCC    | GND | SDA | SCL

- Download the **index.htm** file and place it on the SD card outside of any folder. The card must be formatted as FAT16 or FAT32
    - Edit the following lines in the index.htm file:
    ```javascript
    channelKeys.push({channelNumber:871658, name:'Your channel name',key:'NP1I3KHAW44KY0Q3',
                  fieldList:[{field:1,axis:'T'},{field:2,axis:'H'},{field:3,axis:'P'},{field:4,axis:'I'}]});
    ```
    The key is your **Read API** key. 

- Install the required libraries:
    - [Adafruit BME280 Library](https://github.com/adafruit/Adafruit_BME280_Library.git)
    - [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor.git)
    - [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306.git)
    - [Base64](https://github.com/agdl/Base64.git)
    - [Ethernet](https://github.com/PaulStoffregen/Ethernet.git)
    - [PubSubClient](https://github.com/knolleary/pubsubclient.git)
    - [SD](https://github.com/adafruit/SD.git)

- If you use **PlatformIO** place the following lines in your platformio.ini file and all required libraries will be installed.
```ini
lib_deps = 
    Ethernet
    Adafruit BME280 Library
    161 ; SD library
    PubSubClient
    agdl/Base64 @ ^1.0.0
    Adafruit GFX Library
	Adafruit SSD1306
    adafruit/Adafruit BusIO @ ^1.5.0
```
- Edit the arduino code with your login details and channel information:
```cpp
char mqttUserName[] = "ArduinoMQTT";           // Use any name.
char mqttPass[] = "PNFR1X3LA5D1M8XU";      // Change to your MQTT API Key from Account > MyProfile.   
char writeAPIKey[] = "DEKBK8CCMXMS4STX";    // Change to your channel write API key.
long channelID = 871658;                    // Change to your channel ID.
```
```cpp
char userAndPassword[] = "user:password";
```

### The Ethernet shield
My W5100 ethernet shield replica had a problem that it would not turn on when plugged in to a wall socket (5v or 9v). This issue seems to be included also in the [arduino ethernet shield documentation](https://www.arduino.cc/en/Main/ArduinoEthernetShieldV1) -> *Previous revisions of the shield were not compatible with the Mega and need to be manually reset after power-up.*   
I read online that a 0.1&mu;F (100nF) capacitor soldered on the reset button will make the ethernet shield turn on when connected to a power socket.  
It did but at a cost, now the ethernet shield is no longer resettable from the computer's USB port, meaning I have to push the reset button exactly when a new sketch is uploaded. ¯\_(ツ)_/¯
![IMG_2430](https://user-images.githubusercontent.com/38941045/94045182-c69a0500-fdd7-11ea-9268-f0b5e38fe168.JPG)