/*------------------------------------------------------------------------------
  Program:      BME280_webserver 

  Description:  periodic sensor (BME280) readings using an Arduino
                development board and an Ethernet shield. Sensor readings (temperature,
                humidity, pressure, etc.) are posted to a ThingSpeak channel using MQTT API. 

  Hardware:     Arduino Mega 2560, Arduino Ethernet shield (WT5100), BME280 sensor and 
                0,96 128x64 OLED screen
                Should work with other development boards that are built on Arduino environment

  Software:     Developed using PlatformIO

  Libraries:  - Adafruit BME280 Library
                https://github.com/adafruit/Adafruit_BME280_Library.git
              - Adafruit Unified Sensor
                https://github.com/adafruit/Adafruit_Sensor.git
              - Adafruit SSD1306
                https://github.com/adafruit/Adafruit_SSD1306.git
              - Base64
                https://github.com/agdl/Base64.git
              - Ethernet
                https://github.com/PaulStoffregen/Ethernet.git
              - MQTT
                https://github.com/256dpi/arduino-mqtt
              - SD 
                https://github.com/arduino-libraries/SD
              - SPI, and Wire libraries (included with the Arduino IDE)

  References: - BME280 datasheet: 
                https://ae-bst.resource.bosch.com/media/_tech/media/datasheets/BST-BME280-DS002.pdf
              - Arduino Ethernet shield (W5100) datasheet: 
                http://www.mouser.com/catalog/specsheets/A000056_DATASHEET.pdf
              - 0,96 OLED screen datasheet:
                http://web.eng.fiu.edu/watsonh/IntroMicros/M14-I2C/ER-OLED0.96-1_Series_Datasheet.pdf
              - ThingSpeak MQTT API: 
                https://www.mathworks.com/help/thingspeak/mqtt-api.html?s_tid=CRUX_lftnav

  Date:         April 16, 2022

  Author:       P. Dinu
  ------------------------------------------------------------------------------*/

#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <Base64.h>
#include <Ethernet.h>
#include <MQTT.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <Fonts/FreeSans9pt7b.h>
#include <mqtt_secrets.h>

// BME280
// ------
Adafruit_BME280 bme;

// Ethernet shield (W5100) settings (MAC and IP address)
// -----------------------------------------------------
byte mac[] = {  0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
//IPAddress ip(192, 168, 1, 25);
EthernetClient ethClient;

// ThingSpeak settings
// ------------------
char mqttClientID[] = SECRET_MQTT_CLIENT_ID;
char mqttUserName[] = SECRET_MQTT_USERNAME;     
char mqttPass[] = SECRET_MQTT_PASSWORD;         
long channelID = 871658;                        // Change to your channel ID.

MQTTClient mqttClient;
const char* mqttServer = "mqtt3.thingspeak.com"; 
unsigned long lastConnectionTime = 0L; 
const unsigned long postingInterval = 20L * 1000L; // Post data every 20 seconds
unsigned long mqttLoopTime = 0L;
const unsigned long mqttLoopDelay = 500L;

// OLED print settings
// -------------------
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
//  Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
unsigned long displayNow = 0L;
const unsigned long displayDelay = 8000L;

// Serial print settings
// --------------------
unsigned long serial_time_now = 0L;
const unsigned long serial_delay = 2000L; // Serial print delay;
bool published = false;

// mqtt reconnect function
// -----------------------
unsigned long mqttReconnectTime = 0L;
const unsigned long mqttReconnectInterval = 30000L;

//timer for switching displayed info on screen (oled burn prevention...)
unsigned long switchPosition = 0L;
const unsigned long switchPositionDelay = 5000L;

void reconnectMqtt() 
{
  Serial.print("Attempting MQTT connection...");
  // Connect to the MQTT broker.
  if (mqttClient.connect(mqttClientID, mqttUserName, mqttPass))
  {
    Serial.println("connected");
  }
  else
  {
    Serial.print("failed, rc=");
    // Print reason the connection failed.
    Serial.print(mqttClient.returnCode());
    Serial.print(" try again in ");
    Serial.print(mqttReconnectInterval/1000);
    Serial.println(" seconds.");
  }
}

// Base64 encoding for user:password
// ---------------------------------
String password()
{
  char userAndPassword[] = "admin:admin";      //change these!!
  int inputStringLength = strlen(userAndPassword);
  char encodedString[Base64.encodedLength(inputStringLength)];
  Base64.encode(encodedString, userAndPassword, inputStringLength);
  return(encodedString);
}
String myPass = password();

const unsigned long wrongPassTimeout = 15000L;
unsigned long wrongPassTimestamp = 0L;
int authAttempt = 0;
bool wrongPassFlag = false;

// Web server
// ---------
EthernetClient webClient;
EthernetServer ethServer(80);
File webPage;
String header;

int first_row, second_row, third_row = 0;

void setup()
{
  Serial.begin(115200);

  // Initialize Ethernet
  Ethernet.begin(mac, 5000UL, 2000UL);
  ethServer.begin();
  
  // Initialize mqtt
  mqttClient.begin(mqttServer, 1883, ethClient);    // Set the MQTT broker details.

  // Initialize SD Card
  Serial.println("Initializing SD card...");
  if (!SD.begin(4)) {
    Serial.println("ERROR - SD card initialization failed!");
    return;
  }
  Serial.println("SUCCESS - SD card initialized.");
  if (!SD.exists("index.htm")) {
    Serial.println("ERROR - Can't find index.htm file!");
  }
  Serial.println("SUCCESS - Found index.htm file.");
  Serial.println();

  // Initialize sensors
  bme.begin(BME280_ADDRESS_ALTERNATE);  //Alternate address means 0x76, if addres is 0x77 use BME280_ADDRESS
  bme.setSampling(Adafruit_BME280::MODE_NORMAL,  
                  Adafruit_BME280::SAMPLING_X16, // temperature
                  Adafruit_BME280::SAMPLING_X16, // pressure
                  Adafruit_BME280::SAMPLING_X16, // humidity
                  Adafruit_BME280::FILTER_OFF,
                  Adafruit_BME280::STANDBY_MS_1000);  //measuring at 1 sample/sec to avoid sensor heating
  
  // Initialize OLED screen
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setFont(&FreeSans9pt7b);
}

void loop()
{
  float tempC = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure() / 100; 
  float hIndex = (tempC * 1.8 + 32) - (0.55 - 0.0055 * humidity) * ((tempC * 1.8 + 32) - 58);

  String tempCStr = String(tempC, 2); // temperature (Celsius)
  String humidityStr = String(humidity, 2); // humidity
  String pressureStr = String(pressure, 2); //pressure in mBar
  String hIndexCStr = String(hIndex, 1); // heat index

  // OLED instructions
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  if (millis() <= switchPosition + switchPositionDelay)
  {
    first_row = 28;
    second_row = 45;
    third_row = 62;
  }
  else if (millis() > switchPosition + switchPositionDelay && millis() <= switchPosition + switchPositionDelay*2)
  {
    first_row = 45;
    second_row = 62;
    third_row = 28;
  }
  else if (millis() > switchPosition + switchPositionDelay*2 && millis() <= switchPosition + switchPositionDelay*3)
  {
    first_row = 62;
    second_row = 28;
    third_row = 45;
  }
  else
    switchPosition = millis();
  
  //Temperature
  display.setCursor(0, first_row);
  display.print("Temp: ");
  display.setCursor(64, first_row);
  display.print(tempCStr);
  display.setCursor(116,first_row);
  display.print("C");
  //Humidity
  display.setCursor(0, second_row);
  display.print("Humid: ");
  display.setCursor(64, second_row);
  display.print(humidityStr);
  display.setCursor(111, second_row);
  display.print("%");
  //Pressure
  display.setCursor(0, third_row);
  display.print("Press: ");
  display.setCursor(64, third_row);
  display.print(pressureStr);
  // Display ThingSpeak status and IP Address on OLED screen
  if (millis() <= displayNow + displayDelay)
  {
    display.setCursor(0, 12);
    mqttClient.connected() ? display.print("ThingSpeak OK") : display.print("ThingSpeakN/A");  
  } 
  else
  {
    display.setCursor(0, 12);
    display.print("IP:");
    display.print(Ethernet.localIP());
    if (millis() > displayNow + displayDelay*2)
      displayNow = millis();
  }
  
  display.display();

  // Reconnect if MQTT client is not connected.
  if (!mqttClient.connected() && millis() > mqttReconnectTime + mqttReconnectInterval) 
  {   
    reconnectMqtt();
    mqttReconnectTime = millis();
  }

  // Send loop() to ThingSpeak
  if (millis() > mqttLoopTime + mqttLoopDelay)
  {
    mqttClient.loop();   
    mqttLoopTime = millis();
  }

  // Send data to thingspeak
  if (millis() > lastConnectionTime + postingInterval)
  {
    // Create a topic string and publish data to ThingSpeak channel feed. 
    String topic = "channels/" + String( channelID ) + "/publish";
    Serial.println();
    Serial.println(topic);
    
    // Create data string to send to ThingSpeak.
    String data = String("field1=") + tempCStr + "&field2=" + humidityStr + "&field3=" + pressureStr + "&field4=" + hIndexCStr + "&status=MQTTPUBLISH";
    Serial.println(data);

    if (mqttClient.publish( topic, data ))
      {
        Serial.println("Publish OK");
        published = true;
      }
    else
      {
        Serial.println("Pulbish Failed");
        published = false;
      }

    Serial.println();
    lastConnectionTime = millis();
  }

  // Serial print
  if (millis() > serial_time_now + serial_delay)
  {
    Serial.println("Temperature [C]: " + tempCStr);
    Serial.println("Humidity [%]: " + humidityStr);
    Serial.println("Pressure [mBar]: " + pressureStr);
    Serial.println("Heat Index: " + hIndexCStr);
    Serial.print("Local server IP is: ");
    Serial.println(Ethernet.localIP());
    Serial.println();
    serial_time_now = millis();
  }
  
  wrongPassFlag = millis() - wrongPassTimestamp < wrongPassTimeout;

  webClient = ethServer.available();

  if (webClient && wrongPassFlag == false)
  { 
    Serial.println("New client");
    boolean currentLineIsBlank = true;
    while (webClient.connected()) 
    {
      if (webClient.available()) 
      { 
        char c = webClient.read(); 
        header += c;

        if (c == '\n' && currentLineIsBlank) 
        {
          Serial.print(header);
          
          //21 : “Authorization: Basic “.length()
          int index = header.indexOf("Authorization: Basic ") + 21;
          if ((index >= 21) && (header.substring(index, header.indexOf('\n', index) - 1) == myPass))
          {
            webClient.println("HTTP/1.1 200 OK");
            webClient.println("Content-Type: text/html");
            webClient.println("Connection: close");
            webClient.println();
            authAttempt = 0;
            webPage = SD.open("index.htm");        
            if (webPage) 
            {
              while (webPage.available()) 
              {
                webClient.write(webPage.read());
              }
              webPage.close();
            }
          } 
          else 
          {
            // wrong user/pass
            //client.println("HTTP/1.0 401 Authorization Required");
            webClient.println("HTTP/1.1 401 Unauthorized");
            webClient.println("WWW-Authenticate: Basic realm=\"Secure\"");
            webClient.println("Content-Type: text/html");
            webClient.println();
            webClient.println("<html><h1>No authorization.</h1>"); // really need this for the popup!
            authAttempt += 1;
            if (authAttempt >= 5)
            {
              wrongPassFlag = true;
              wrongPassTimestamp = millis();
              authAttempt = 0;
              webClient.println("Login timeout: ");
              webClient.print(wrongPassTimeout);
            }
            webClient.println("</html>");
          }
          header = "";
          break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          currentLineIsBlank = false;
        }
      } 
    } 
    unsigned long pageDelay = millis();
    while (millis() - pageDelay <= 1) {}  //instead of using delay (1)
    webClient.stop(); 
  }
}