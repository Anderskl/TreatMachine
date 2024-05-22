// #include <ESP8266WiFi.h>
// // #include <WiFiClient.h>
// // #include <ESP8266WebServer.h>
#include <LittleFS.h> // https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html 
#include <WiFiManager.h>
// #define WEBSERVER_H
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h> // Note i have changed 
#include <AccelStepperAKL.h>
#include "homePage.h"

#define DEBUG 0
#if DEBUG
  #define DEBUG_SerialBegin(...) Serial.begin(__VA_ARGS__)
  #define DEBUG_print(...) Serial.print(__VA_ARGS__)
  #define DEBUG_println(...) Serial.println(__VA_ARGS__)
  #define DEBUG_printf(...)      Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_SerialBegin(...)
  #define DEBUG_print(...)
  #define DEBUG_println(...)
  #define DEBUG_printf(...)
#endif


//---------------------------------------------------------------

// unsigned long oldTime = 0;
int treatsRemaining = 0;
int machineCapacity = 33;
int runCounter = 0;
bool giveGoodie = false;
const char* treatsRemainingPath = "/treatsRemaining.txt";

#define motorInterfaceType 1
const int enablePin = 2;
const int stepPin = 0;
const int dirPin = 255;
const int LEDpin = 1;

// Create a new instance of the AccelStepper class:
AccelStepperAKL stepper = AccelStepperAKL(motorInterfaceType, enablePin, stepPin, dirPin);

// Username and password for the treat machine website
const char* http_username = "Ollie";
const char* http_password = "Ollie";

//Declare a global object variable from the ESP8266WebServer class.
AsyncWebServer server(80); //Server on port 80
AsyncWebSocket ws("/ws");

WiFiManager wifiManager;


//==============================================================
//                  Handling websocket trafic
//==============================================================


void notifyClients(const String& inputString) {
  DEBUG_printf("notifyClients received message: %s\n",inputString.c_str());
  ws.textAll(inputString);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    DEBUG_printf("Received message: %s\n",message.c_str());
    if (message.indexOf("reloadmachine") >= 0) {
      int idx1 = message.lastIndexOf(":");
      int idx2 = message.indexOf("}");
      treatsRemaining = message.substring(idx1+2,idx2-1).toInt(); // +2 is because of +1 from start of searchChar and then +1 more because value is in quotation. Because of quotation, also idx2-1
      writeFile(treatsRemainingPath, String(treatsRemaining));
      notifyClients(String("{\"type\":\"treatsRemaining\",\"value\":" + String(treatsRemaining) + "}"));
    }
    else if (message.indexOf("treatButton") >= 0) {
      treatsRemaining = (treatsRemaining - 1 <= 0) ? 0 : treatsRemaining - 1;
      writeFile(treatsRemainingPath, String(treatsRemaining));
      notifyClients(String("{\"type\":\"treatsRemaining\",\"value\":" + String(treatsRemaining) + "}"));
      giveGoodie = true;
    }
    else if (message.indexOf("ResetWifi") >= 0) {
      DEBUG_println("Resetting wifi settings");
      // notifyClients(String("{\"type\":\"resetWifi\",\"value\":" + String(1) + "}"));
      wifiManager.resetSettings();
      DEBUG_println("ESP8266 rebooting...");
      ESP.restart();
    }
    else if (message.indexOf("RestartESP") >= 0) {
      DEBUG_println("ESP8266 rebooting...");
      ESP.restart();
    }
  }
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      DEBUG_println("WS_EVT_CONNECT executed");
      ws.cleanupClients();
      notifyClients(String("{\"type\":\"machineCapacity\",\"value\":" + String(machineCapacity) + "}"));
      notifyClients(String("{\"type\":\"treatsRemaining\",\"value\":" + String(treatsRemaining) + "}"));
      notifyClients(String("{\"type\":\"nrClients\",\"value\":" + String(ws.count()) + "}"));
      break;
    case WS_EVT_DISCONNECT:
      DEBUG_println("WS_EVT_DISCONNECT executed");
      DEBUG_printf("WebSocket client #%u disconnected\n", client->id());
      ws.cleanupClients();
      notifyClients(String("{\"type\":\"nrClients\",\"value\":" + String(ws.count()) + "}"));
      break;
    case WS_EVT_DATA:
      DEBUG_println("WS_EVT_DATA executed");
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
      break;
    case WS_EVT_ERROR:
      break;
  }
}

//==============================================================
//                  functions for LittleFS - taken from Examples -> LittleFS_TimeStamp
//==============================================================
String readFile(const char *path) {
  String result = "";
  DEBUG_printf("Reading file: %s\n", path);

  File file = LittleFS.open(path, "r");
  if (!file) {
    DEBUG_println("Failed to open file for reading");
    return result;
  }

  DEBUG_print("Read from file: ");
  while (file.available()) {
      result += (char)file.read();
  }

  DEBUG_println(result);
  file.close();
  return result;
}

bool writeFile(const char *path, const String& message) {
  DEBUG_printf("Writing file: %s\n", path);

  File file = LittleFS.open(path, "w");
  if (!file) {
    DEBUG_println("Failed to open file for writing");
    return false;
  }
  if (file.print(message)) {
    DEBUG_println("File written");
    file.close();
    return true;
  } else {
    DEBUG_println("Write failed");
    file.close();
    return false;
  }
}


//==============================================================
//                  SETUP
//==============================================================
void setup(){
  DEBUG_SerialBegin(115200);

  pinMode(LEDpin,OUTPUT);
  digitalWrite(LEDpin,LOW);

  digitalWrite(enablePin,HIGH);
  stepper.setAcceleration(400);
  stepper.setMaxSpeed(9000);
  // stepper.setSpeed(1000);
  
  #if !DEBUG
    wifiManager.setDebugOutput(false);
  #endif
  bool isConnected = wifiManager.autoConnect("OllieTreatMachine","OllieBollie"); // password protected ap;
  if (isConnected) {
    for (int i = 0; i < 10; i++){
      digitalWrite(LEDpin,LOW);
      delay(100);
      digitalWrite(LEDpin,HIGH);
      delay(100);
    }
  } else {
    while (true){
      digitalWrite(LEDpin,LOW);
      delay(500);
      digitalWrite(LEDpin,HIGH);
      delay(500);
    }
  }

    // if(!isConnected) {
    //     DEBUG_println("Failed to connect");
    //     // ESP.restart();
    //     while (true) {DEBUG_println("Need to restart"); delay(500);}
    // } 
    // else {
    //     //if you get here you have connected to the WiFi    
    //     DEBUG_println("connected...yeey :)");
    // }


  // LittleFS.format();
  if (!LittleFS.begin())
    DEBUG_println("An error has occurred while mounting LittleFS");
  DEBUG_println("LittleFS mounted successfully");

  if (!LittleFS.exists(treatsRemainingPath)) {
    writeFile(treatsRemainingPath, String(treatsRemaining));
  }
  else {
    String output = readFile(treatsRemainingPath);
    DEBUG_print("file output: ");
    DEBUG_println(output);
    treatsRemaining = output.toInt();
    DEBUG_print("file output as int: ");
    DEBUG_println(treatsRemaining);
  }

  delay(500);

  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET_Async, [](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200, "text/html", HTML_webPage);
  });

  server.begin();                  //Start server
  DEBUG_println("HTTP server started");
}
//==============================================================
//                     LOOP
//==============================================================
void loop(){

  if (giveGoodie){
    runCounter = (runCounter + 1 > 3) ? 1 : runCounter+1; // 1, 2, 3 -> 1, 2, 3 ...
    digitalWrite(enablePin,LOW);
    int distToMove = (runCounter > 2) ? 266 : 267; // 267, 267, 266 -> 267, 267, 266 ...
    stepper.move(stepper.distanceToGo() + distToMove);
    giveGoodie = false;
  }


  if (abs(stepper.distanceToGo()) > 0){
    stepper.run();
  } else if(!stepper.run()){
    digitalWrite(enablePin,HIGH);
  }

  // if (millis() - oldTime > 2000) {
  //   // Serial.print("number of clients connected: ");
  //   // Serial.println(ws.count());
  //   // AsyncWebSocket::AsyncWebSocketClientLinkedList clients = ws.getClients();
  //   // for(const auto &c: clients){
  //   //   if(c->status() == WS_CONNECTED){
  //   //     Serial.print("IP of client number " + String(c->id()) + ": ");
  //   //     Serial.println(c->remoteIP());
  //   //   }
  //   // }
  //   ws.cleanupClients();
  //   oldTime = millis();
  // }
}

