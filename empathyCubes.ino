#include <WiFiClientSecure.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>
#include <ESP8266WiFiType.h>
#include <ESP8266WiFiAP.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiSTA.h>


//////////////////////
// WiFi Definitions //
//////////////////////
const char WiFiAPPSK[] = "sparkfun";

/////////////////////
// Pin Definitions //
/////////////////////
const int RED_PIN = 0;
const int GREEN_PIN = 4;
const int BLUE_PIN = 5; 
const int MAX_GPIO = 1023;
#define COMMON_ANODE

WiFiServer server(80);

// cube value
enum Color {red, yellow, blue};
Color cubeStatus = red;

void setup() 
{
  initHardware();
  setupWiFi();
  server.begin();
}

void loop() 
{
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.println(req);
  client.flush();

  // Match the request
  int val = -1; // We'll use 'val' to keep track of both the
                // request type (read/set) and value if set.
  if (req.indexOf("/status/0/red") != -1) {
    cubeStatus = red;
    val = 0;
  }
  if (req.indexOf("/status/0/yellow") != -1) {
    cubeStatus = yellow;
    val = 1;
  }
  if (req.indexOf("/status/0/blue") != -1) {
    cubeStatus = blue;
    val = 2;
  }
  // Otherwise request will be invalid. We'll say as much in HTML
  client.flush();

  // change color of led
  switch (cubeStatus) {
     case red: setColor(MAX_GPIO, 0, 0);  // red
      break;
    case yellow: setColor(MAX_GPIO, MAX_GPIO, 0);  // yellow
      break;
    case blue: setColor(0, 0, MAX_GPIO);  // blue
      break;
    default: setColor(0, 0, 0);
      break;
  }
  
  // Prepare the response. Start with the common header:
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n\r\n";
  s += "<!DOCTYPE HTML>\r\n<html>\r\n";
  
  // show new status of cube
  if (val >= 0)
  {
    s += "Cube Status is now ";
    switch (cubeStatus) {
       case red: s += "red";
        break;
      case yellow: s += "yellow";
        break;
      case blue: s += "blue";
        break;
      default: s += "unkown...";
        break;
    }
  }
  else
  {
    s += "Invalid Request.<br> Try /status/0/red or /status/0/yellow";
  }
  s += "</html>\n";

  // Send the response to the client
  client.print(s);
  delay(1);
  Serial.println("Client disonnected");

  // The client will actually be disconnected 
  // when the function returns and 'client' object is detroyed
}

void setupWiFi()
{
  WiFi.mode(WIFI_AP);

  // Do a little work to get a unique-ish name. Append the
  // last two bytes of the MAC (HEX'd) to "Thing-":
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  String AP_NameString = "ESP8266 Thing " + macID;

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i=0; i<AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);

  WiFi.softAP(AP_NameChar, WiFiAPPSK);
}

void initHardware()
{
  Serial.begin(115200);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT); 
}

void setColor(int red, int green, int blue)
{
  #ifdef COMMON_ANODE
    red = MAX_GPIO - red;
    green = MAX_GPIO - green;
    blue = MAX_GPIO - blue;
  #endif
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue); 
}
