#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include "Adafruit_SH1106.h"

#define BLUE_LED        (1)
#define DEVICE_MODE_PIN (3)

const char *ssid = "wifi_name";
const char *password = "password";

float temperature, pressure, humidity;
volatile bool server_mode = true;

Adafruit_BME280 bme280;
Adafruit_SH1106 display(-1);
ESP8266WebServer server(80);

void handleRoot() 
{
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  char temp[450];
  
  snprintf(temp, 450,

"<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP8266 Sensor</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Temperature: %3.2f *C <br> Pressure: %3.2f hPa <br> Humidity: %3.2f %%<br></h1>\
    <p>Uptime: %d:%02d:%02d</p>\    
  </body>\
</html>",
  temperature, pressure, humidity, hr, min % 60, sec % 60);

  server.send(200, "text/html", temp);
}

void handleNotFound() 
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) 
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

void logToDisplay(const char * text)
{
  char temp[200];
  snprintf(temp, 200, "%s", text);
  display.write(temp);
  display.display();
}

void oledDisplayData()
{
  const uint8_t x = 116; //for *C
  const uint8_t y = 0;   //for *C

  static float _temperature = temperature;
  static float _pressure = pressure;
  static float _humidity = humidity;
  
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  
  char temp[200];

  //update BME280 readings after some calls of this function 
  static volatile uint8_t counter = 0;
  if(counter > 5)
  {
    counter = 0;
    _temperature = temperature;
    _pressure = pressure;
    _humidity = humidity;
  }
  else
  {
    ++counter;
  }

  display.clearDisplay();
  display.setCursor(0,0); 
  snprintf(temp, 200, "Temperature: %3.2f  C\r\n\r\nPressure: %3.2f hPa\r\n\r\nHumidity: %3.2f %%\r\n\r\n\r\nUptime: %d:%02d:%02d",
            _temperature, _pressure, _humidity, hr, min % 60, sec % 60);
  display.write(temp);
  display.drawPixel(x, y, WHITE); display.drawPixel(x - 1, y + 1, WHITE); display.drawPixel(x + 1, y + 1, WHITE); display.drawPixel(x, y + 2, WHITE); //*C
  display.display();
}

void setup(void) 
{
  yield();
  pinMode(BLUE_LED, OUTPUT);
  pinMode(DEVICE_MODE_PIN, INPUT);
  digitalWrite(BLUE_LED, HIGH);
  if(LOW == digitalRead(DEVICE_MODE_PIN))
  {
    server_mode = false;
  }
  else
  {
    server_mode = true;
  }
  
  Wire.begin(0, 2);
  
  display.begin(SH1106_SWITCHCAPVCC, 0x3C);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.display();

  bool status = bme280.begin(0x76);  
  if (!status) 
  {
    logToDisplay("Cannot find a valid\r\nBME280 sensor");
    while(1);
  }

  if(server_mode)
  {  
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
  
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) 
    {
      delay(500);
      logToDisplay(".");
    }
  
    logToDisplay("\r\nConnected to\r\n");
    logToDisplay(ssid);
    logToDisplay("\r\nIP address:\r\n");
    logToDisplay(WiFi.localIP().toString().c_str());
  
    if (MDNS.begin("esp8266")) 
    {
      logToDisplay("\r\nMDNS responder\r\nstarted");
    }
  
    server.on("/", handleRoot);
    server.on("/inline", []() {
      server.send(200, "text/plain", "this works as well");
    });
    server.onNotFound(handleNotFound);
    server.begin();
    logToDisplay("\r\nHTTP server started");
  }
  else
  {
    logToDisplay("No webserver mode");
  }
  delay(1000);
}

void loop(void) 
{  
  yield();
  temperature = bme280.readTemperature();
  pressure = bme280.readPressure()/100.0;
  humidity = bme280.readHumidity();
  
  if(server_mode)
  {
    server.handleClient();
    MDNS.update();
  }

  oledDisplayData();

  digitalWrite(BLUE_LED, !digitalRead(BLUE_LED)); //toggle led
}
