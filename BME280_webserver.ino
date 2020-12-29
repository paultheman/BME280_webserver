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
              - PubSubClient
                https://github.com/knolleary/pubsubclient.git
              - SD 
                https://github.com/adafruit/SD.git
              - SPI, and Wire libraries (included with the Arduino IDE)

  References: - BME280 datasheet: 
                https://ae-bst.resource.bosch.com/media/_tech/media/datasheets/BST-BME280-DS002.pdf
              - Arduino Ethernet shield (W5100) datasheet: 
                http://www.mouser.com/catalog/specsheets/A000056_DATASHEET.pdf
              - 0,96 OLED screen datasheet:
                http://web.eng.fiu.edu/watsonh/IntroMicros/M14-I2C/ER-OLED0.96-1_Series_Datasheet.pdf
              - ThingSpeak MQTT API: 
                https://www.mathworks.com/help/thingspeak/use-arduino-client-to-publish-to-a-channel.html

  Date:         September 21, 2020

  Author:       P. Dinu
  ------------------------------------------------------------------------------*/

#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <Base64.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <Fonts/FreeSans9pt7b.h>

// BME280
// ------
Adafruit_BME280 bme;

// Ethernet shield (W5100) settings (MAC and IP address)
// -----------------------------------------------------
byte mac[] = {  0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
//IPAddress ip(192, 168, 1, 25);
EthernetClient ethClient;

// ThingSpeak settings
// -------------------
char mqttUserName[] = "ArduinoMQTT";           // Use any name.
char mqttPass[] = "DEKBK8CCMXMS4STX";      // Change to your MQTT API Key from Account > MyProfile.   
char writeAPIKey[] = "PNFR1X3LA5D1M8XU";    // Change to your channel write API key.
long channelID = 871658;                    // Change to your channel ID.
static const char alphanum[] ="0123456789"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz";  // For random generation of client ID.
PubSubClient mqttClient(ethClient); // Initialize the PubSubClient library.
const char* mqttServer = "mqtt.thingspeak.com"; 
unsigned long lastConnectionTime = 0L; 
const unsigned long postingInterval = 20L * 1000L; // Post data every 20 seconds
unsigned long mqttLoopTime = 0L;
unsigned long mqttLoopDelay = 1000L;

// OLED print settings
// -------------------
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
//  Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
unsigned long displayNow = 0L;
unsigned long displayDelay = 8000L;

// Serial print settings
// --------------------
unsigned long serial_time_now = 0L;
unsigned long serial_delay = 2000L; // Serial print delay;

// mqtt reconnect function
// -----------------------
unsigned long mqttReconnectTime = 0L;
unsigned long mqttReconnectInterval = 5000L;

void reconnectMqtt() 
{
  char clientID[9];
  Serial.print("Attempting MQTT connection...");
  // Generate ClientID
  for (int i = 0; i < 8; i++) 
  {
    clientID[i] = alphanum[random(51)];
  }
  clientID[8]='\0';

  // Connect to the MQTT broker.
  if (mqttClient.connect(clientID,mqttUserName,mqttPass))
  {
    Serial.println("connected");
  }
  else
  {
    Serial.print("failed, rc=");
    // Print reason the connection failed.
    // See https://pubsubclient.knolleary.net/api.html#state for the failure code explanation.
    Serial.print(mqttClient.state());
    Serial.print(" try again in ");
    Serial.print(mqttReconnectInterval/1000);
    Serial.println(" seconds.");
  }
}

// Base64 encoding for user:password
// ---------------------------------
String password()
{
  char userAndPassword[] = "user:password";
  int inputStringLength = strlen(userAndPassword);
  int encodedLength = Base64.encodedLength(inputStringLength);
  char encodedString[encodedLength];
  Base64.encode(encodedString, userAndPassword, inputStringLength);
  return(encodedString);
}
String myPass = password();

// Web server
// ---------
EthernetClient webClient;
EthernetServer ethServer(80);
File webPage;
String header;

void webServer()
{
  webClient = ethServer.available();
  if (webClient) 
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
          
          //21 : "Authorization: Basic ".length()
          int index = header.indexOf("Authorization: Basic ") + 21;
          if ((index >= 21) && (header.substring(index, header.indexOf('\n', index) - 1) == myPass))
          {
            webClient.println("HTTP/1.1 200 OK");
            webClient.println("Content-Type: text/html");
            webClient.println("Connection: close");
            webClient.println();
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
            webClient.println("<html>No authorization.</html>"); // really need this for the popup!
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

void setup()
{
  randomSeed(analogRead(0));   //initializes the pseudo-random number generator based on noise from A0 analog pin
  Serial.begin(115200);

  // Initialize Ethernet
  Ethernet.begin(mac);
  ethServer.begin();

  // Initialize mqtt
  mqttClient.setServer(mqttServer, 1883);   // Set the MQTT broker details.

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
  //Temperature
  display.setCursor(0, 28);
  display.print("Temp: ");
  display.setCursor(64, 28);
  display.print(tempCStr);
  display.setCursor(116,28);
  display.print("C");
  //Humidity
  display.setCursor(0, 45);
  display.print("Humid: ");
  display.setCursor(64, 45);
  display.print(humidityStr);
  display.setCursor(111, 45);
  display.print("%");
  //Pressure
  display.setCursor(0, 62);
  display.print("Press: ");
  display.setCursor(64, 62);
  display.print(pressureStr);
  // Display ThingSpeak status and IP Address on OLED screen
  if (millis() <= displayNow + displayDelay)
  {
    display.setCursor(0, 12);
    mqttClient.connected() ? display.print("ThingSpeak OK") : display.print("ThingSpeakN/A");  
  } 
  else if (millis() > displayNow + displayDelay)
  {
    display.setCursor(0, 12);
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
    // Create data string to send to ThingSpeak.
    String data = String("field1=") + tempCStr + "&field2=" + humidityStr + "&field3=" + pressureStr + "&field4=" + hIndexCStr;
    int length = data.length();
    const char *msgBuffer;
    msgBuffer=data.c_str();
    Serial.println();
    Serial.println(msgBuffer);
    Serial.println();

    // Create a topic string and publish data to ThingSpeak channel feed. 
    String topicString = "channels/" + String( channelID ) + "/publish/"+String(writeAPIKey);
    length = topicString.length();
    const char *topicBuffer;
    topicBuffer = topicString.c_str();
    mqttClient.publish( topicBuffer, msgBuffer );
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
  
  webServer();

}