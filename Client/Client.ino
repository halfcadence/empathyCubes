#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <Light.h>

// 4DC4 is the current prod build
const char* ssid = "ESP8266 Thing 43C9";
const char* password = "sparkfun";

// cube num
const int cubeIndex = 0;

enum Color {busy, feedback, avail, white};

// colors
const long COLOR_BUSY = 0xff0000;
const long COLOR_FEEDBACK = 0xffff00;
const long COLOR_AVAILABLE= 0x0000ff;
const long COLOR_WHITE = 0xffffff;

// rgb led
const int RED_PIN = 14;
const int GREEN_PIN = 12;
const int BLUE_PIN = 13;
const int MAX_GPIO = 1023;
const bool COMMON_ANODE = false;
Light light = Light(RED_PIN, GREEN_PIN, BLUE_PIN, MAX_GPIO, COMMON_ANODE);

void setup () {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  light.setHexColor(COLOR_WHITE);
  while (WiFi.status() != WL_CONNECTED) {
 
    delay(1000);
    Serial.print("Connecting..");
 
  }

  light.setHexColor(COLOR_AVAILABLE);
  Serial.println("Connected, motherfucker");
  
}

void loop()
{
  setColor(busy);
  delay(6000);    //Send a request every 6 seconds
  setColor(feedback);
  delay(6000);    //Send a request every 6 seconds
  setColor(avail);
  delay(6000);    //Send a request every 6 seconds
}

void setColor(Color color) {
  String httpString = "http://192.168.4.1/status/";
  httpString += cubeIndex;
  switch (color) {
     case busy:
      light.setHexColor(COLOR_BUSY);  // red
      httpString += "/busy";
      break;
    case feedback:
      light.setHexColor(COLOR_FEEDBACK);  // yellow
      httpString += "/feedback";
      break;
    case avail:
      light.setHexColor(COLOR_AVAILABLE);  // blue
      httpString += "/available";
      break;
    case white:
      light.setHexColor(COLOR_WHITE);  // blue
      httpString += "/white";
      break;
    default: light.setHexColor(COLOR_WHITE);
      break;
  }
  httpRequest(httpString);
}

bool httpRequest(String url) {
  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
    HTTPClient http;  //Declare an object of class HTTPClient
    http.begin(url);  //Specify request destination
    http.addHeader("Content-Type", "text/html");
    int httpCode = http.GET();      
    //Send the request
    if (httpCode > 0) { //Check the returning code
      String payload = http.getString();   //Get the request response payload
      Serial.println(payload);                     //Print the response payload
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();   //Close connection
  } else
  {
    Serial.println("reconnecting...");
    WiFi.disconnect(true);
    WiFi.begin(ssid, password);
  }
}

