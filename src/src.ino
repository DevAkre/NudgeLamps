#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

#include "NeoPatterns.h"
#include "Button.h"

void receiveNudge(char *topic, byte *payload, unsigned int length);
void sendNudge();
#include "connect_aws.h"

#define BUTTON_LAMP_PIN D2
#define BUTTON_NUDGE_PIN D1
#define LIGHT_PIN D4
#define LIGHT_PIXEL_COUNT 35
#define DEFAULT_MODE 1

#define NUDGE_TIME 120000 //acknowledgement light up
#define NUDGE_SEND_TIME 4000 //time before you can send another nudge
#define NUDGE_REPLY_TIME 300000 //time to give for a reply

#define AP_PASSWORD "password"
#define CONFIG_PORTAL_TIMEOUT 180

void ringCallback(){}
void inline changeLightMode(int);
void revertNudge();

NeoPatterns lightRing(LIGHT_PIXEL_COUNT, LIGHT_PIN, NEO_GRB + NEO_KHZ800, &ringCallback);
Button blamp(BUTTON_LAMP_PIN), bnudge(BUTTON_NUDGE_PIN);
WiFiManager wm;
String lampId;
bool isAWSSetup = false;

void setup() {
  pinMode(BUTTON_LAMP_PIN, INPUT);
  pinMode(BUTTON_NUDGE_PIN, INPUT);
  lightRing.begin();
  lightRing.clear();
  lightRing.show();
  lampId = String(ESP.getChipId());
  Serial.begin(115200);
  Serial.println();
  Serial.print("ESP8266 Chip id: ");
  Serial.println(lampId);
  changeLightMode(-3);
  
  wm.setClass("invert"); //dark theme
//  wm.resetSettings();
//  wm.setDebugOutput(false);
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(180);
  wm.autoConnect(("Lamp-" + lampId).c_str(), AP_PASSWORD);
  
  isAWSSetup = setupAWS();
  checkConnection();
  if(isAWSSetup) changeLightMode(DEFAULT_MODE);
}

int mode=DEFAULT_MODE;
unsigned long lastNudgeTime = 0;
int prev_mode=DEFAULT_MODE;
uint8_t nudgeState = 0; //0 - no nudge, 1 - sent nudge, 2 - wait mode, 3 - nudge received, 4 - nudge acknowledged

void loop() {
  wm.process();
  
  if(!isAWSSetup){
    isAWSSetup = setupAWS();
    if(isAWSSetup) changeLightMode(DEFAULT_MODE);
  }
  
  checkConnection();
  client.loop();
  blamp.readState();
  bnudge.readState();
  lightRing.Update();
  
  if(blamp.isLongDetected){
    lightRing.UpdateBrightness(-2);
    Serial.print("Brightness:");
    Serial.println(lightRing.brightness);
  }
  
  if(bnudge.isLongDetected){
    lightRing.UpdateBrightness(+2);
    Serial.print("Brightness:");
    Serial.println(lightRing.brightness);
  }
  
  if(blamp.isShortDetected){
    mode = (mode+1)%9;
    prev_mode = mode;
    Serial.println(mode);
    changeLightMode(mode);
  }

  if(bnudge.isShortDetected){
    Serial.println(nudgeState);
    unsigned long cur = millis();
    if(nudgeState == 3){
      Serial.println("Acknowledged Nudge!");
      changeLightMode(-2);
      lastNudgeTime = cur;
      nudgeState = 4;
      sendNudge();
    }else if(nudgeState != 1){
      Serial.println("Sent Nudge!");
      lastNudgeTime = cur;
      nudgeState = 1;
      if(mode >= 0) prev_mode = mode;
      mode = -1;
      changeLightMode(mode);
      sendNudge();
    }
  }

  revertNudge();
  blamp.updateState();
  bnudge.updateState();
}

void revertNudge(){
  if(mode != prev_mode){
    unsigned long cur = millis();
    
    if(nudgeState == 4){
      if(cur - lastNudgeTime  > NUDGE_TIME){
        mode = prev_mode;
        changeLightMode(mode);
        nudgeState = 0;
      }
    }else if(nudgeState == 3){
      if(cur - lastNudgeTime  > NUDGE_REPLY_TIME){
        mode = prev_mode;
        changeLightMode(mode);
        nudgeState = 0;
      }
    }else if(nudgeState == 1){
      if(cur - lastNudgeTime > NUDGE_SEND_TIME){
        mode = prev_mode;
        changeLightMode(mode);
        nudgeState = 2;
      }
    }else if(nudgeState == 2){
       if(cur-lastNudgeTime > NUDGE_REPLY_TIME - NUDGE_SEND_TIME){
         nudgeState = 0;
       }
    }
  }
}

void inline changeLightMode(int mode){
  switch(mode){
      case 0:
        lightRing.RainbowCycle(500);
        break;
      case 1:
        lightRing.SolidColor(lightRing.Color(255,20,147)); //deep-pink
        break;
      case 2:
        lightRing.SolidColor(lightRing.Color(255,0,0)); //red/
        break;
      case 3:
        lightRing.SolidColor(lightRing.Color(0,255,0)); //green
        break;
      case 4:
        lightRing.SolidColor(lightRing.Color(0,0,255)); ///blue
        break;
      case 5:
        lightRing.SolidColor(lightRing.Color(255,255,0)); //yellow
        break;
      case 6:
        lightRing.SolidColor(lightRing.Color(255,165,0)); //orange
        break;
      case 7:
        lightRing.SolidColor(lightRing.Color(0,255,255)); //cyan
        break;
      case 8:
        lightRing.SolidColor(lightRing.Color(255,0,255)); //purple
        break;
      case -1:
        lightRing.Breathing(lightRing.Color1, lightRing.DimColor(lightRing.Color1),100,20); //nudge sent or received
        break;
      case -2:
        lightRing.SolidColor(lightRing.Color(127,255,212)); //nudge acknowledged
        break;
      case -3:
        lightRing.TheaterChase(lightRing.Color(0,255,0),lightRing.Color(255,20,147),50); //set-up
        break;
    }
}


void receiveNudge(char *topic, byte *payload, unsigned int length){
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
  String id = doc["id"];
  if(strcmp(topic, AWS_IOT_SUBSCRIBE_TOPIC "nudge")==0){    
    if(id == lampId){
      Serial.println("Received back own Nudge");
      return;
    }
    
    Serial.print("Received Nudge from: ");
    Serial.println(id);
    if(nudgeState == 1 or nudgeState == 2){
      changeLightMode(-2);
      lastNudgeTime = millis();
      nudgeState = 4;
      return;
    }
    nudgeState = 3;
    lastNudgeTime = millis();
    if(mode >=0) prev_mode = mode;
    mode = -1;
    changeLightMode(mode);
  }else if(strcmp(topic, AWS_IOT_SUBSCRIBE_TOPIC "set_solid_color")==0){
    lightRing.SolidColor(lightRing.Color(doc["Color1"]["R"],doc["Color1"]["G"],doc["Color1"]["B"]));
  }else if(strcmp(topic, AWS_IOT_SUBSCRIBE_TOPIC "set_rainbow_cycle")==0){
    lightRing.RainbowCycle(doc["interval"]);
  }else if(strcmp(topic, AWS_IOT_SUBSCRIBE_TOPIC "set_color_wipe")==0){
    lightRing.ColorWipe(lightRing.Color(doc["Color1"]["R"],doc["Color1"]["G"],doc["Color1"]["B"]), doc["interval"]);
  }else if(strcmp(topic, AWS_IOT_SUBSCRIBE_TOPIC "set_theater_chase")==0){
    lightRing.TheaterChase(lightRing.Color(doc["Color1"]["R"],doc["Color1"]["G"],doc["Color1"]["B"]), lightRing.Color(doc["Color2"]["R"],doc["Color2"]["G"],doc["Color2"]["B"]), doc["interval"]);
  }else if(strcmp(topic, AWS_IOT_SUBSCRIBE_TOPIC "set_breathing")==0){
    lightRing.Breathing(lightRing.Color(doc["Color1"]["R"],doc["Color1"]["G"],doc["Color1"]["B"]), lightRing.Color(doc["Color2"]["R"],doc["Color2"]["G"],doc["Color2"]["B"]), doc["steps"],doc["interval"]);
  }
}

void sendNudge(){
  StaticJsonDocument<64> doc;
  doc["id"] = lampId;
  char jsonBuffer[64];
  serializeJson(doc, jsonBuffer);
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}
