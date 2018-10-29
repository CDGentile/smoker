
//Smoker V3
//Updated 1 Nov 2017
//Designed for operation at Galway with local Wifi & MQTT broker
//In progress in this update:
//  -Option to disable FAN mode
//  -Analog probe sensing
//  -Interrupt based time loop
//  -Program button based input (skip WiFi/MQTT)
//  -MQTT based inputs

//New update 23 Jul 2018
//adding local webpage interface


//#include <MCP320x.h>
#include <Ticker.h>
#include <Servo.h>
#include <ArduinoOTA.h>
#include <OLEDDisplay.h>
#include <OLEDDisplayFonts.h>
#include <OLEDDisplayUi.h>
#include <SSD1306.h>
#include <SSD1306Wire.h>
#include <SPI.h>
#include <Adafruit_MAX31855.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

//comment out to stop serial comms
//#define DEBUG

#ifdef DEBUG
 #define DEBUG_PRINT(x)  Serial.print(x)
 #define DEBUG_PRINTLN(x) Serial.println(x)
 #define DEBUG_PRINTF(x,y) Serial.printf(x,y)
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTLN(x)
 #define DEBUG_PRINTF(x,y)
#endif

//Pin definitions
#define MISO    12
#define MOSI    13
#define TC1CS   2
#define TC2CS   0
#define ADCCS   15
#define CLK     14
#define FANPIN  1
#define SRVPIN  3

int pit;
int old_pit;
int err[3] = {0,0,0};       //Perror, Ierror, Derror
int k[3] = {1,5,0};         //Kp, Ki, Kd (Ki * 100)
int food[2] = {40,40};
int fan = 0;                //fan off
int damp = 0;             //damper open
int timeCount = 0;
int mode = 1;
int setTemp = 225;
int command;
int setting = 0;

Servo damper;

const char* setTempTopic  = "/cdgentile@gmail.com/Smoker/Tmp";
const char* setModeTopic  = "/cdgentile@gmail.com/Smoker/Mode";
const char* setDampTopic  = "/cdgentile@gmail.com/Smoker/Damp";
const char* setFanTopic   = "/cdgentile@gmail.com/Smoker/Fan";
const char* ackTopic      = "/cdgentile@gmail.com/Smoker/Ack";
const char* rptTopic      = "/cdgentile@gmail.com/Smoker/Rpt";

// WiFi Credentials
const char* ssid = "Bullpup_CA";
const char* password = "************";

//DIOTTY credentials
//const char* userId = "cdgentile@gmail.com";
//const char* passWd = "73cd89c4";

// Initialize Thermocouples
Adafruit_MAX31855 TC1(CLK, TC1CS, MISO);
Adafruit_MAX31855 TC2(CLK, TC2CS, MISO);
//MCP320x adc(ADCCS, MOSI, MISO, CLK);


// MQTT Class
IPAddress server(192,168,1,11);
//#define server "mqtt.dioty.co"
WiFiClient espClient;
PubSubClient client(espClient);

//Display Setup
SSD1306  display(0x3C, 4, 5);



void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  DEBUG_PRINTLN();
  DEBUG_PRINT("Connecting to ");
  DEBUG_PRINTLN(ssid);

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 0, "Smoker v2.1");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 20, "Connecting to WiFi:");
  display.drawString(64, 32, ssid);
  display.display();

  WiFi.begin(ssid, password);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_PRINT(".");
    count++;
    if (count==21) return;
  }

  randomSeed(micros());

  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("WiFi connected");
  DEBUG_PRINTLN("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 0, "Smoker v2.0");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 20, "Connected to WiFi:");
  display.drawString(64, 32, ssid);
  display.drawString(64, 44, WiFi.localIP().toString());
  display.display();

  delay(3000);
}


void reconnect() {
  //connects to MQTT broker and subscribes to required topics
  // only runs once - calling function (loop in this case) must recall if required
  DEBUG_PRINT("Attempting MQTT connection...");

  // Attempt to connect
  if (client.connect("Smoker Remote", userId, passWd)) {
    DEBUG_PRINTLN("connected");
    // ... and subscribe to topic
    client.subscribe(setTempTopic);
    client.subscribe(setModeTopic);
    client.subscribe(setFanTopic);
    client.subscribe(setDampTopic);
  } else {
    DEBUG_PRINT("failed, rc=");
    DEBUG_PRINT(client.state());
    DEBUG_PRINTLN(" try again on next loop");
  }
}


int readPit() {       //takes 6 measurements (up to 300ms)
  //averages both thermocouple readings (if connected)
  int numValid = 0;
  int numCount = 0;
  double total = 0.0;

  while (numCount< 4) {
    double a = TC1.readCelsius();
    if (!isnan(a)) {
      total += a;
      numValid++;
    }
    double b = TC2.readCelsius();
    if (!isnan(b)) {
      total += b;
      numValid++;
    }
    delay(50);
    numCount++;
  }

  total = total/numValid;
  total = (total * 1.8) + 32.0;

  return(int(total));
}

/*int readFood(int num) {     //reads a channel of the MCP3202 and returns integer degrrees F
  double R, T;
  int aval;

  aval = adc.readChannel(num);
  // These were calculated from the thermister data sheet
  //  A = 2.3067434E-4;
  //  B = 2.3696596E-4;
  //  C = 1.2636414E-7;
  //
  // This is the value of the other half of the voltage divider
  //  Rknown = 10000;

  // Do the log once so as not to do it 4 times in the equation
  //  R = log(((1024/(double)aval)-1)*(double)10000);

  R = log((1 / ((4096 / (double) aval) - 1)) * (double) 10000);
  //lcd.print("A="); lcd.print(aval); lcd.print(" R="); lcd.print(R);
  // Compute degrees C
  T = (1 / ((2.3067434E-4) + (2.3696596E-4) * R + (1.2636414E-7) * R * R * R)) - 273.25;
  // return degrees F
  return ((int) ((T * 9.0) / 5.0 + 32.0));
}*/

void drawScreen() {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(15, 0, "Pit ");
    display.drawString(60, 0, "Food");
    if (WiFi.status() == WL_CONNECTED) {
      display.drawString(108,0, "W");
    }
    if (client.connected()) {
      display.drawString(120,0, "M");
    }
    display.drawString(110,53, "F");
    display.drawString(121,53, "D");

    display.setFont(ArialMT_Plain_24);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    if ((pit<10) or (pit>900)) {
      display.drawString(26,12, "---");
    } else {
      display.drawString(26, 12, String(pit) + "°");
    }
    if ((food[0]<10) or (food[0]>900)) {
      display.drawString(80,12,"---");
    } else {
      display.drawString(80,12, String(food[0]) + "°");
    }
     if ((food[1]<10) or (food[1]>900)) {
      display.drawString(80,40,"---");
    } else {
      display.drawString(80,40, String(food[1]) + "°");
    }


    display.setFont(ArialMT_Plain_16);
    switch (mode) {
      case 0:
        display.drawString(26, 46, "MAN");
        break;
      case 1:
        display.drawString(26, 46, "AUTO");
        break;
      case 2:
        display.drawString(26, 46, "WARM");
        break;
      case 3:
        display.drawString(26, 46, "DOOR");
        break;
    }


    int height = int(fan * 0.4);
    display.fillRect(108,53-height,8,height);
    height = int(damp * 0.4);
    display.fillRect(120,53-height,8,height);
}

void setDamp() {
  int val;

  if (setting < 101) {
    damp = setting;
  } else {
    damp = 100;
  }
  val = map(damp, 0, 100, 100, 175);
  damper.write(val);

  delay(15);
}

void setFan() {
  //need to check if fan needs setting - 100
  if (setting < 101) {
    fan = 0;
  } else {
    fan = setting;
  }
  int val = map(fan, 0, 100, 0, 1023);
  //currently has ~20% deadband - either use integrator or do coarse PWM
  if (val < 200) val = 0;
  analogWrite(FANPIN, val);
}

void callback(char* topic, byte* payload, unsigned int length) {

  DEBUG_PRINT("Message arrived [");
  DEBUG_PRINT(topic);
  DEBUG_PRINTLN("] ");

  //translate payload (3 digit integer)
  payload[length] = '\0';
  char *msgString = (char *)payload;
  int val = atoi(msgString);


  if (strcmp(topic, setModeTopic) ==0) {
    //change mode
    if (val == 0) {
      mode = 0;
    }
    if (val == 1) {
      if (mode==0) {
        mode = 1;
      }
    }
  }

  if (strcmp(topic, setTempTopic) ==0) {
    //change mode
  }

  if (strcmp(topic, setFanTopic) ==0) {
    //change mode
  }

  if (strcmp(topic, setDampTopic) ==0) {
    //change mode
  }
}

void sendMQTT(){
  //called every 5 seconds
  //send temps, fan, damper, and mode

  String payload = "Pit:" + String(pit) + " Food:" + String(food[0]) + "/" + String(food[1]) +
                    " Fan:" + String(fan) + " Damper:" + String(damp) + " " + mode;
  int pld_len = payload.length()+1;
  char payload_char[pld_len];
  payload.toCharArray(payload_char, pld_len);
  client.publish(rptTopic, payload_char);
}

void doPID() {
  switch (mode) {


    case 0:  //man

    break;
    case 1:  //auto
      err[0] = setTemp - pit;
      err[1] += err[0];
      if (err[1]>100) err[1]=100;
      setting += ((err[0]*k[0]) - (err[2]*k[2]) + int((err[1]*k[1])/100));
      if (setting < 0) {
        setting = 0;
      }
      if (setting > 100) {
        setting = 100;
      }
    break;
    case 2:  //warm
    break;
    case 3: //door
    break;
  }
}

void setupArduinoOTA() {
  ArduinoOTA.onStart([]() {
    DEBUG_PRINTLN("Start");
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 0, "OTA Update");
    display.drawString(64, 24, "Starting...");
    display.display();
    delay(1000);
  });
  ArduinoOTA.onEnd([]() {
    DEBUG_PRINTLN("\nEnd");
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 0, "OTA Update");
    display.drawString(64, 24, "COMPLETE");
    display.display();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG_PRINTF("Progress: %u%%\r", (progress / (total / 100)));
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 0, "OTA Update");
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 24, ("Progress: "+ String(progress/(total/100))+"%"));
    display.drawProgressBar(10,45,108,12,uint8_t(progress/(total/100)));
    display.display();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_PRINTF("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) DEBUG_PRINTLN("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) DEBUG_PRINTLN("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) DEBUG_PRINTLN("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINTLN("Receive Failed");
    else if (error == OTA_END_ERROR) DEBUG_PRINTLN("End Failed");
  });
  ArduinoOTA.begin();
}


void setup()  {
 // pinMode(ADCCS, OUTPUT);
 // digitalWrite(ADCCS, HIGH);

  display.init();
  display.flipScreenVertically();

  setDamp();

  #ifdef DEBUG
    Serial.begin(9600);
  #else
    damper.attach(SRVPIN);
    delay(15);
  #endif



  DEBUG_PRINTLN("Thermocouple test");
  delay(500);

  pit = readPit();
  old_pit = pit;

  setup_wifi();
  client.setServer(server, 1883);
  client.setCallback(callback);
}

void loop()
{
   //check wifi and MQTT, reconnect if necessary
   if (!client.connected()) {
     reconnect();
   }

   //increment time and call time based functions

   timeCount++;

   //update temps
   pit = readPit();
   //food[0] = readFood(0);
   //food[1] = readFood(1);


   //doPID();
  /* if ((timeCount%15)==0){
        err[2] = pit - old_pit;
        old_pit = pit;
        timeCount = 0;
        sendMQTT();

   }*/

 //  setDamp();


  // setFan();

   drawScreen();
   display.display();

   delay(900);

   ArduinoOTA.handle();
   client.loop();

}
