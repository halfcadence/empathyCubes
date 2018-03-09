#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
  #include "Wire.h"
#endif

#include <Light.h>

MPU6050 mpu;

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

//#define OUTPUT_READABLE_YAWPITCHROLL

// 4DC4 is the current prod build
const char* ssid = "ESP8266 Thing 4DC4";
const char* password = "sparkfun";

// cube num
const int cubeIndex = 0;
int cubeStatus = 0;

int accelGyroOffsets[6] = {-2648, 766, 1488, 193, 28,  15};

enum Color {busy, feedback, avail, white};

// colors
const long COLOR_BUSY = 0xFA0005;
const long COLOR_FEEDBACK = 0xDF2500;
const long COLOR_AVAILABLE= 0x4BC8C8;
const long COLOR_WHITE = 0xffffff;

// rgb led
const int RED_PIN = 14;
const int GREEN_PIN = 12;
const int BLUE_PIN = 13;
const int MAX_GPIO = 1023;
const bool COMMON_ANODE = false;
Light light = Light(RED_PIN, GREEN_PIN, BLUE_PIN, MAX_GPIO, COMMON_ANODE);

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
  mpuInterrupt = true;
}

void setup () {
  initializeWire();
  
  Serial.begin(115200);
  Serial.println("waiting for serial");
  while (!Serial);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  light.setHexColor(COLOR_WHITE);
  while (WiFi.status() != WL_CONNECTED) {
 
    delay(1000);
    Serial.println("Connecting to wifi...");
 
  }
  Serial.println("Connected to wifi.");

  initializeI2CDevices();

  light.setHexColor(COLOR_AVAILABLE);
}

void loop()
{
  int oldCubeStatus = cubeStatus;
  checkMPU();
  if (cubeStatus != oldCubeStatus) {
    Serial.printf("YPR: %.2f | %.2f | %.2f | Cube Status: %d\n", (ypr[0] * 180/M_PI), (ypr[1] * 180/M_PI), (ypr[2] * 180/M_PI), cubeStatus);
    switch (cubeStatus) {
      case 0:
        light.setHexColor(COLOR_BUSY);  // red
        sendColorHttpRequest(busy);
        break;
      case 1:
        light.setHexColor(COLOR_FEEDBACK);  // yellow
        sendColorHttpRequest(feedback);
        break;
      case 2:
        light.setHexColor(COLOR_AVAILABLE);  // blue
        sendColorHttpRequest(avail);
        break;
      case 3:
        light.setHexColor(COLOR_WHITE);  // blue
        sendColorHttpRequest(white);
    }
  }
}

void initializeWire() {
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.begin();
    Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
  #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
    Fastwire::setup(400, true);
  #endif
}

void initializeI2CDevices() {
  Serial.println(F("Initializing I2C devices..."));
  mpu.initialize();

  // verify connection
  Serial.println(F("Testing device connections..."));
  while (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    delay(1000);
  }
  Serial.println("MPU6050 connection successful");
  delay(1000);

  /* uncomment to wait for input to begin
  Serial.println(F("\nSend any character to begin DMP programming and demo: "));
  while (Serial.available() && Serial.read()); // empty buffer
  while (!Serial.available());                 // wait for data
  while (Serial.available() && Serial.read()); // empty buffer again
  */
  
  // load and configure the DMP
  Serial.println(F("Initializing DMP..."));
  devStatus = mpu.dmpInitialize();

  // supply your own gyro offsets here, scaled for min sensitivity
  mpu.setXAccelOffset(accelGyroOffsets[0]);
  mpu.setYAccelOffset(accelGyroOffsets[1]);
  mpu.setZAccelOffset(accelGyroOffsets[2]);

  mpu.setXGyroOffset(accelGyroOffsets[3]);
  mpu.setYGyroOffset(accelGyroOffsets[4]);
  mpu.setZGyroOffset(accelGyroOffsets[5]);
  

  // make sure it worked (returns 0 if so)
  if (devStatus == 0) {
    // turn on the DMP, now that it's ready
    Serial.println(F("Enabling DMP..."));
    mpu.setDMPEnabled(true);

    mpuIntStatus = mpu.getIntStatus();

    // set our DMP Ready flag so the main loop() function knows it's okay to use it
    Serial.println(F("DMP ready!"));
    dmpReady = true;

    // get expected DMP packet size for later comparison
    packetSize = mpu.dmpGetFIFOPacketSize();
  } else {
    // ERROR!
    // 1 = initial memory load failed
    // 2 = DMP configuration updates failed
    // (if it's going to break, usually the code will be 1)
    Serial.print(F("DMP Initialization failed (code "));
    Serial.print(devStatus);
    Serial.println(F(")"));
  }
}

int checkMPU() {
  // if programming failed, don't try to do anything
  if (!dmpReady) {
    Serial.println("DMP programming failed, exiting");
    return -1;
  }

  int waitTime = 0;
  // reset interrupt flag and get INT_STATUS byte
  mpuInterrupt = false;
  mpuIntStatus = mpu.getIntStatus();

  // get current FIFO count
  fifoCount = mpu.getFIFOCount();
  
  // check for overflow (this should never happen unless our code is too inefficient)
  if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
    // reset so we can continue cleanly
    mpu.resetFIFO();
    Serial.println(F("FIFO overflow!"));
    return -1;
  // otherwise, check for DMP data ready interrupt (this should happen frequently)
  } else if (mpuIntStatus & 0x02) {
    // wait for correct available data length, should be a VERY short wait
    while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

    // read a packet from FIFO
    mpu.getFIFOBytes(fifoBuffer, packetSize);
    
    // track FIFO count here in case there is > 1 packet available
    // (this lets us immediately read more without waiting for an interrupt)
    fifoCount -= packetSize;

    #ifdef OUTPUT_READABLE_YAWPITCHROLL
        // display Euler angles in degrees
        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
        Serial.printf("YPR: %.2f | %.2f | %.2f\n", (ypr[0] * 180/M_PI), (ypr[1] * 180/M_PI), (ypr[2] * 180/M_PI));
    #endif
    
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    float pitch = ypr[1] * 180/M_PI;
    float roll = ypr[2] * 180/M_PI;

    int oldCubeStatus = cubeStatus;
    if (abs(pitch) > 45) {
      cubeStatus = 0;
    }
    if (abs(roll) > 45) {
      cubeStatus = 1;
    }
    if (abs(pitch) <= 45 && abs(roll) <= 45) {
      cubeStatus = 2;
    }
    return cubeStatus;
  }
}

void sendColorHttpRequest(Color color) {
  String httpString = "http://192.168.4.1/status/";
  httpString += cubeIndex;
  switch (color) {
     case busy:
      httpString += "/busy";
      break;
    case feedback:
      httpString += "/feedback";
      break;
    case avail:
      httpString += "/available";
      break;
    case white:
      httpString += "/white";
      break;
    default: break;
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
      //Serial.println(payload);                     //Print the response payload
      Serial.println(url);
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

