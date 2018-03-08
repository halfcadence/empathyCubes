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

#include <Light.h>

//////////////////////
// WiFi Definitions //
//////////////////////
const char WiFiAPPSK[] = "sparkfun";

// colors
const long COLOR_BUSY = 0xff0000;
const long COLOR_FEEDBACK = 0xffff00;
const long COLOR_AVAILABLE= 0x0000ff;
const long COLOR_WHITE = 0xffffff;

WiFiServer server(80);

// cube value
enum Color {busy, feedback, avail, white};

Color cubeStatus[2] = {busy,busy};

const int MAX_GPIO = 1023;
const bool COMMON_ANODE = false;

// rgb led 0
const int RED_PIN_0 = 15;
const int GREEN_PIN_0 = 13;
const int BLUE_PIN_0 = 12;
Light light0 = Light(RED_PIN_0, GREEN_PIN_0, BLUE_PIN_0, MAX_GPIO, COMMON_ANODE);

// rgb led 1
const int RED_PIN_1 = 14;
const int GREEN_PIN_1 = 2;
const int BLUE_PIN_1 = 0;
Light light1 = Light(RED_PIN_1, GREEN_PIN_1, BLUE_PIN_1, MAX_GPIO, COMMON_ANODE);

// rgb led 1
const int RED_PIN_2 = 4;
const int GREEN_PIN_2 = 5;
const int BLUE_PIN_2 = 16;
Light light2 = Light(RED_PIN_2, GREEN_PIN_2, BLUE_PIN_2, MAX_GPIO, COMMON_ANODE);

void setup() 
{
  light2.setHexColor(COLOR_WHITE);
  initHardware();
  setupWiFi();
  server.begin();
  light2.setHexColor(COLOR_BUSY);
  updateCubeColors();
}

void updateCubeColors() {
  switch (cubeStatus[0]) {
     case busy: light0.setHexColor(COLOR_BUSY);  // red
      break;
    case feedback: light0.setHexColor(COLOR_FEEDBACK);  // yellow
      break;
    case avail: light0.setHexColor(COLOR_AVAILABLE);  // blue
      break;
    case white: light0.setHexColor(COLOR_WHITE);  // blue
      break;
    default: light0.setHexColor(COLOR_WHITE);
      break;
  }

  switch (cubeStatus[1]) {
     case busy: light1.setHexColor(COLOR_BUSY);  // red
      break;
    case feedback: light1.setHexColor(COLOR_FEEDBACK);  // yellow
      break;
    case avail: light1.setHexColor(COLOR_AVAILABLE);  // blue
      break;
    case white: light1.setHexColor(COLOR_WHITE);  // blue
      break;
    default: light1.setHexColor(COLOR_WHITE);
      break;
  }

  light2.setHexColor(COLOR_BUSY);
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
  if (req.indexOf("/status/0/busy") != -1) {
    cubeStatus[0] = busy;
    val = 0;
  }
  else if (req.indexOf("/status/0/feedback") != -1) {
    cubeStatus[0] = feedback;
    val = 1;
  }
  else if (req.indexOf("/status/0/available") != -1) {
    cubeStatus[0] = avail;
    val = 2;
  }
  else if (req.indexOf("/status/1/busy") != -1) {
    cubeStatus[1] = busy;
    val = 3;
  }
  else if (req.indexOf("/status/1/feedback") != -1) {
    cubeStatus[1] = feedback;
    val = 4;
  }
  else if (req.indexOf("/status/1/available") != -1) {
    cubeStatus[1] = avail;
    val = 5;
  }

  else if (req.indexOf("/status/0/white") != -1) {
    cubeStatus[1] = avail;
    val = 6;
  }
  else if (req.indexOf("/status/1/white") != -1) {
    cubeStatus[1] = avail;
    val = 7;
  }
  client.flush();

  updateCubeColors();
  
  // Prepare the response. Start with the common header:
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n\r\n";
  s += "<!DOCTYPE HTML>\r\n<html>\r\n";
  
  // show new status of cube
  if (val >= 0)
  {
    switch (val) {
       case 0: s += "Cube 0 is now busy";
        break;
      case 1: s += "Cube 0 is now seeking feedback";
        break;
      case 2: s += "Cube 0 is now available";
        break;
      case 3: s += "Cube 1 is now busy";
        break;
      case 4: s += "Cube 1 is now seeking feedback";
        break;
      case 5: s += "Cube 1 is now available";
        break;
        
      case 6: s += "Cube 0 is now white";
        break;
      case 7: s += "Cube 1 is now white";
        break;
        
      default: s += "unkown...";
        break;
    }
  }
  else
  {
    s += "Invalid Request.<br> Try /status/0/busy or /status/0/feedback";
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
}
