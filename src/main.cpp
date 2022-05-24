#define BLYNK_TEMPLATE_ID "TMPL8WRxgmtw"
#define BLYNK_DEVICE_NAME "SmartPlantProject"
#define BLYNK_AUTH_TOKEN "ZRKGxAJdYeyZ4OfFCLl76S3NqauX2Hsm"

// Libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>


// Networking
char auth[] = BLYNK_AUTH_TOKEN;   
char ssid[] = "JARVIS 2.4";
char pass[] = "ToTheNameOfNetaji#100";
HTTPClient https;


//variables for sending data to google sheets
const uint8_t fingerprint[20] = {0x7A, 0xA4, 0x6F, 0xD8, 0xA5, 0x63, 0xB3, 0x1F, 0x3D, 0x33, 0x74, 0x86, 0x68, 0x0A, 0xB0, 0xAD, 0x66, 0x8F, 0x96, 0x1F};
//7A A4 6F D8 A5 63 B3 1F 3D 33 74 86 68 0A B0 AD 66 8F 96 1F
const String sheet = "https://script.google.com/macros/s/AKfycbwnn9LCp2LgipVH_OCwd_gSJEzW00LFpPKJsx6nsHSAoWSE5ZsUhj4M2CjwydCFp4Eckg/exec?Moisture=";
String final_link;

// Blynk
BlynkTimer timer;
BlynkTimer timer1;
BlynkTimer timer2;
WidgetRTC rtc;

// System state
bool systemOn = 0;
bool systFlag = false;
long ontime;
long lastWater = 0;               
bool pumpOn = false;

// Settings (can be adjusted from app)
int amount = 0;
int interval = 0;
int thresh = 600;

// sensor
// float tf = 0.1;                         // trust factor for smoothing filter
float sensor = 600;       // Set highest start value to avoid unwanted triggers


//Functions//////////////////////

void updateSheet(){
  final_link = sheet+String(sensor);
  final_link.trim();

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setFingerprint(fingerprint);
  
  Serial.print(F("[HTTPS] begin...\n"));
    if (https.begin(*client, (String)final_link))
    {  
      // HTTP
      Serial.print(F("[HTTPS] GET...\n"));
      // start connection and send HTTP header
      int httpCode = https.GET();
    
      // httpCode will be negative on error
      if (httpCode > 0) 
      {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        // file found at server
      }
      else 
      {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }
      https.end();
    } 
    else 
    {
      Serial.printf("[HTTPS} Unable to connect\n");
    }
}

void readSensor() 
{
  float reading=0.0;

  // read raw values
  for(int i=0; i<10; i++){
    reading += analogRead(A0);
  }
  sensor =  reading/10; 

  Blynk.virtualWrite(V21, sensor);
}

int waterdur () 
{
  // convert ml to ms for controlling pump on-time, different values depending on the chosen pump
  return amount * 57 + 350; // set experimentally--> 57 millisecond/ml, 350 milliseconds is the base value 
                            //after which water starts entering the tob
}

bool plantCheck() 
{
  // find elapsed time since last water
  long elapsedTime = now() - lastWater;
  
  if (elapsedTime > interval * 60 * 60 and sensor > thresh) {
    return true;
  }

  return false;
}

String getStatus() 
{
  String status = "";

  if (pumpOn == true) {
    // pump is on
    return "Watering ...";
    
  } else if (systemOn) {
    if (lastWater == ontime) {
      status = "not watered.";
    } else {
      // calculate difference
      long diff = now() - lastWater;

      // onvert between minutes, hours, days and too much
      if (diff < 60) {
        status = "just now.";

      } else if (diff < 2 * 60) {
        status = "1 min ago.";
        
      } else if (diff < 60 * 60) {
        status = String(diff / 60) + " mins ago.";
        
      } else if (diff < 60 * 60 * 2) {
        status = "1 hour ago";

      } else if (diff < 60 * 60 * 24) {
        status = String(diff / (60 * 60)) + " hours ago.";

      } else if (diff > 60 * 60 * 24){
        status = "+1 day ago";
      }
    }
    
    return "Last Watered: " + status;

  } else {
    return "Syste Off";
  } 
}

void water() 
{
  Serial.print("Watering plant ...");

  // set flag (used for status update)
  pumpOn = true;

  // push-update status in app and then turn pump on, and
  Blynk.virtualWrite(V11, getStatus());
  digitalWrite(D7, HIGH);

  // delay loop
  long startTime = millis();
  while (millis() - startTime < waterdur()) {
    // keep everything running in the meantime (except the control loop)
    Blynk.run();
    timer1.run();
    timer2.run();
  }

  // remove flag
  pumpOn = false;

  // turn pump off and push new status
  digitalWrite(D7, LOW);  
  Blynk.virtualWrite(V11, getStatus());
  

  // Reset water button (if used)
  Blynk.virtualWrite(V8, 0);

  // update lastwater to server
  lastWater = now();
  Blynk.virtualWrite(V34, lastWater);
}


void control () 
{
  // check if system is on
  if (systemOn) {
    
    // check if it's time to water
    if (plantCheck()){
      water();
    }
  }
}


// BLYNK ///////////////////////////////////////

BLYNK_CONNECTED()
{
  // Synchronize unix-time on connection
  rtc.begin();
}


// IN-APP EVENT CALLS ///////////////////////
// for when the user presses any button in the app

// System on-off button event
BLYNK_WRITE(V7)
{
  // change system state
  systemOn = param.asInt();

  if (systemOn) {
    if (systFlag) {
      // system has just been turned on!
      systFlag = false;

      // set lastWater to now:
      ontime = now();
      Blynk.virtualWrite(V34, ontime);
      Blynk.syncVirtual(V34);
    }
    // system was turned on when connected -> do nothing
  } else {
    // system is off
    systFlag = true;
  }
}

// Manual water button event
BLYNK_WRITE(V8)
{
  // if button was pressed and plant is eligible for water
  if (param.asInt() == 1 and now() - lastWater > 5 and systemOn) {
    // execute water routine
    water();
  } else {
    // reset water button to unpressed state
    Blynk.virtualWrite(V8, 0);
  }
}

// Reload button event
BLYNK_WRITE(V2) 
{
  // Reload requested -> update display values in app
  if (param.asInt() == 1) {
    Blynk.virtualWrite(V3, amount);
    Blynk.virtualWrite(V4, interval);
    Blynk.virtualWrite(V5, thresh);
//    Blynk.virtualWrite(V11, getStatus());
  }
}

// Amount change event
BLYNK_WRITE(V3)
{
  // Store updated value depending on selected plant
  amount= param.asInt();

  // save to server
  Blynk.virtualWrite(V31, amount);
}

// Interval change event
BLYNK_WRITE(V4)
{
  // Store updated value depending on selected plant
  interval = param.asInt();

  // save to server
  Blynk.virtualWrite(V32, interval);
}

// Threshold change event
BLYNK_WRITE(V5)
{
  // Store updated value depending on selected plant
  thresh = param.asInt();

  // save to server
  Blynk.virtualWrite(V33, thresh);
}

// GET CALLS //////////////////////////////
// used when 'sync' is called at startup 

BLYNK_WRITE(V31)
{
  // Get values from server:
  amount = param.asInt();
}

BLYNK_WRITE(V32)
{
  // Get values from server:
  interval = param.asInt();
}

BLYNK_WRITE(V33)
{
  // Get values from server:
  thresh = param.asInt();
}

// Last Water - retreive values from server
BLYNK_WRITE(V34)
{
  // Get values from server:
  lastWater = param.asInt();
}



// STATUS DISPLAY FUNCTIONS /////////////////////
// when app request current plant status 

BLYNK_READ(V11)
{
  Blynk.virtualWrite(V11, getStatus());
}


void setup()
{

  // Serial
  Serial.begin(115200);

  // Blynk 
  Blynk.begin(auth, ssid, pass);

  // fetch stored data from server
  Blynk.syncVirtual(V7);
  Blynk.syncVirtual(V31);
  Blynk.syncVirtual(V32);
  Blynk.syncVirtual(V33);
  Blynk.syncVirtual(V34);
//  Blynk.syncVirtual(V35);

  

  // timers
  timer.setInterval(30000L, control);     // for control loop, run every 30 secs
  timer1.setInterval(5000L, readSensor); // for sensor read loop, run every 5 sec
  timer2.setInterval(10*60*1000L, updateSheet);

  // RTC
  setSyncInterval(10 * 60);               // Sync interval in seconds (10 minutes)
  
  // pin assignments
  pinMode(D7, OUTPUT);


  // set pumps to OFF (active-low)
  digitalWrite(D7, LOW);


  // Set a reasonable start value for sensors (a little above the triggering threshold)
  sensor = thresh + 10;

  Serial.println("Setup Complete");
}


void loop()
{
  // main blynk loop
  Blynk.run();

  // timers
  timer.run();
  timer1.run(); 
  timer2.run();
}