//    *********************************************
//
//    MINIMAL OTA
//    FOR TOBERS TIMESWITCH
//
//    V 1.0 - 18.04.2021
//
//    *********************************************
//
//    This sketch is a helper to enable OTA for TOBERS TIMESWITCH on devices with only 1MB flash memory.
//    On these devices OTA cannot be done directly - this program must be flashed as an intermediate step.
//  
//    MINIMAL OTA connects to WiFi using the credentials formerly saved in flash memory by the timeswitch sketch. 
//    It only enables OTA with the current version of TOBERS TIMESWITCH via ota.html. There are no further functions!
//
//
//    IMPORTANT NOTE:
//
//    Please read the information on https://www.hackster.io/eltoberino/tobers-timeswitch-for-esp8266-ab3e06#toc-over-the-air-update--ota-10
//    and take the steps described very carefully - otherwise you risk bricking the device! 
//
//    Make sure you have chosen the proper board settings and made the same PIN definitions as in your timeswitch sketch setup.
//    --> REMEMBER:   Flash Size Setting: File System (FS) must be at least 192k! <--
//  
//    Always double-check before any OTA if you have selected the proper settings in board manager! 
//    Uploading an incorrect file can brick the device - in that case you can only unbrick it by re-uploading the program via serial interface.
//
//
//        
//
//    Copyright (c) 2021 Tobias Schulz
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.

//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//    
//
//    ****************************************************************
//
//    Credits:
//    - Special thanks to Jens Fleischer - the timeswitches presented on his site are the nucleus around which I built my own version of a timeswitch.
//      His fantastic website www.fipsok.de is one of the best places in the web to find inspiration and help concerning ESP8266 and ESP32 projects.
//    - HTML background pattern graphic by Henry Daubrez, taken from http://thepatternlibrary.com/
//    - Thanks to the many, many other programmers and enthusiasts whose work and helpfulness enabled me to realize such a project.
//
//    ***************************************************************
//
//    PLEASE NOTICE THE COMMENTS and set the configuration parameters according to the settings made in your timeswitch sketch.
//
//    ***************************************************************



#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <WiFiUdp.h>
#include <LittleFS.h>




//// --> USER SETTINGS <-- ////

//#define DEBUG                                   // show debug messages in serial monitor
//#define LOCAL_LANG                              // enable local language


// PIN DEFINITIONS :                            // must be identical to the definitions made in timeswitch sketch

const uint8_t relPin = 12;                      // set pin for relay
const uint8_t aktiv = HIGH;                     // active state of relay

const uint8_t ledPin = 13;                      // set pin for status LED
const uint8_t led_aktiv = LOW;                  // active state of status LED

const uint8_t buttonPin = 0;                    // set pin for manual switch button 
const uint8_t button_aktiv = LOW;               // state of pressed button


//// --> END OF USER SETTINGS <-- ////




//// WEBSERVER ////

ESP8266WebServer server(80);



//// AUTHENTICATION ////

uint8_t enableAuth = 1;                                             // enable html basic authentication (1 = enabled)                                           
char www_username[21] = "tober";                                    // credendtials for html sites -> must be entered in browser to reach ALL html sites
char www_password[21] = "franz";                   



//// OTA ////

#ifdef LOCAL_LANG
    const char OTAsuccessMsg[] PROGMEM = "<div style=\"border:0.1em solid #e1c3a9; margin:0; padding:0.5em;font-size:0.8em; font-family:sans-serif; text-align:center; color:white; background-color:green;\">Update erfolgreich</span>";
    const char OTAfailMsg[] PROGMEM = "<div style=\"border:0.1em solid #e1c3a9; margin:0; padding:0.5em;font-size:0.8em; font-family:sans-serif; text-align:center; color:white; background-color:red;\">Update fehlgeschlagen</span>";
#else
    const char OTAsuccessMsg[] PROGMEM = "<div style=\"border:0.1em solid #e1c3a9; margin:0; padding:0.5em;font-size:0.8em; font-family:sans-serif; text-align:center; color:white; background-color:green;\">Update successful</span>";
    const char OTAfailMsg[] PROGMEM = "<div style=\"border:0.1em solid #e1c3a9; margin:0; padding:0.5em;font-size:0.8em; font-family:sans-serif; text-align:center; color:white; background-color:red;\">Update failed</span>";
#endif



//// TIME SWITCH ////

char devicename[21];                            // name of time switch
bool relState = !aktiv;                         // initial relState = off


//// FUNCTIONS ////


void wificonnect(){                                                   // read https://forum.arduino.cc/index.php?topic=652513 to understand how WiFi setup works on ESP
  uint8_t  wifi_retry=0;                                              // Counter solves ESP32-Bug with certain routers where connection can only be established every second time
  uint8_t  staticIP = 0;

  File f = LittleFS.open("/IP_mode.txt", "r");                          // reads information saved on LittleFS file if device shall connect with static IP
   if(!f) {
    #ifdef DEBUG
      Serial.println("opening \"IP_mode.txt\" failed");
    #endif
  } else {
  String temp0 = f.readStringUntil('\n');
  temp0.trim();
  staticIP = temp0.toInt();
  String temp1 = f.readStringUntil('\n');
  temp1.trim();
  uint32_t IP_raw1 = temp1.toInt();
  IPAddress ip(IP_raw1);
  String temp2 = f.readStringUntil('\n');
  temp2.trim();
  uint32_t IP_raw2 = temp2.toInt();
  IPAddress gateway(IP_raw2);
  String temp3 = f.readStringUntil('\n');
  temp3.trim();
  uint32_t IP_raw3 = temp3.toInt();
  IPAddress subnet(IP_raw3);
  String temp4 = f.readStringUntil('\n');
  temp4.trim();
  uint32_t IP_raw4 = temp4.toInt();
  IPAddress dns(IP_raw4);
  f.close();

  #ifdef DEBUG
    Serial.println("IP Configuartion loaded from LittleFS:");
    Serial.print("Static IP enabled: ");
    Serial.println(staticIP);
    if (staticIP == 1) {
      Serial.print("IP: ");
      Serial.println(ip);
      Serial.print("Gateway: ");
      Serial.println(gateway);
      Serial.print("Subnet: ");
      Serial.println(subnet);
      Serial.print("DNS: ");
      Serial.println(dns);
    }
  #endif
  
  if (staticIP == 1) WiFi.config(ip, gateway, subnet, dns);             // sets static IP parameters
  }                                                                     // end of else (-> if File exists!)
  
  WiFi.softAPdisconnect(true);                                          // closes AP mode on startup to avoid ESP working as AP during normal operation
  
  while (WiFi.waitForConnectResult() != WL_CONNECTED && wifi_retry < 3) {
        #ifdef DEBUG
          Serial.print("Not yet connected...retrying - Attempt No. ");
          Serial.println(wifi_retry+1);
        #endif
        WiFi.begin();
        delay(3000);
        wifi_retry++;
  } 

  if(wifi_retry >= 5) {
        #ifdef DEBUG
          Serial.println("no connection, restarting ...");
        #endif
          delay(500);
          ESP.restart();  
       }
 
  if (WiFi.waitForConnectResult() == WL_CONNECTED){
    digitalWrite(ledPin, !led_aktiv);
        #ifdef DEBUG
          Serial.print("Connected to network \"");
          Serial.print(WiFi.SSID());
          Serial.print("\" with IP ");
          Serial.println(WiFi.localIP());
        #endif
  }
}


void switchButtonGuard() {                                                  // manual button press function
  uint8_t tmCount = 0;
  bool longpress = false;
    while (digitalRead(buttonPin) == button_aktiv) {
      tmCount++;
      delay(100);
      if (tmCount > 40){                                                    // if button pressed longer than 4 sec -> restart
        #ifdef DEBUG
          Serial.println("Long button press - restarting device");
        #endif
        ESP.restart();
        #ifdef DEBUG
          Serial.println("set true");
        #endif
        longpress = true;
       }
    }
    if (tmCount > 0 && longpress == false){
      #ifdef DEBUG
        Serial.println("Relais manually activated by button press");         // if button press < 4 sec -> manual relais state change
      #endif
      relState = !relState;
      digitalWrite(relPin, relState);
    }
}
 

void showDeviceName() {                                                                           // sends device name to html sites 
  String temp = "{\"No1\":\""+ String(devicename) + "\"}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", temp);
  #ifdef DEBUG
      Serial.printf("Device Name \"%s\" sent successfully\n", devicename);    
  #endif
}


void loadConfig() {                                                                                 // loads configuration data from file on LittleFS
  File f = LittleFS.open("/config.txt", "r");
  if(!f) {
    #ifdef DEBUG
      Serial.println("file open failed");
    #endif
    return;
  }
    String temp0 = f.readStringUntil('\n');
    temp0.trim();
    //count = temp0.toInt();
    String temp1 = f.readStringUntil('\n');
    temp1.trim();
    //if (temp1 != "") snprintf(ntpServer, sizeof(ntpServer), "%s", temp1.c_str());
    String temp2 = f.readStringUntil('\n');
    temp2.trim();
    //if (temp2 != "") refreshTime = temp2.toInt();
    String temp3 = f.readStringUntil('\n');
    temp3.trim();
    snprintf(www_username, sizeof(www_username), "%s", temp3.c_str());
    String temp4 = f.readStringUntil('\n');
    temp4.trim();
    snprintf(www_password, sizeof(www_password), "%s", temp4.c_str());
    String temp5 = f.readStringUntil('\n');
    temp5.trim();
    enableAuth = temp5.toInt();
    String temp6 = f.readStringUntil('\n');
    temp6.trim();
    //longitude = temp6.toDouble();
    String temp7 = f.readStringUntil('\n');
    temp7.trim();
    //latitude = temp7.toDouble();
    String temp8 = f.readStringUntil('\n');
    temp8.trim();
    snprintf(devicename, sizeof(devicename), "%s", temp8.c_str());
    String temp9 = f.readStringUntil('\n');
    temp9.trim();
    //choice = temp9.toInt();
    String temp10 = f.readStringUntil('\n');
    temp10.trim();
    //twilight_type = temp10.toInt();
    f.close();
   #ifdef DEBUG
      Serial.printf("\n Loaded from file successfully:\n Enable Auth:%i, Username:\"%s\", Password:\"%s\", Device Name \"%s\"\n", 
      enableAuth, www_username, www_password, devicename);
   #endif
}


String sketchName() {                                               // shortens file name for sendSketchName()
  char file[sizeof(__FILE__)] = __FILE__;
  char * pch;
  char * svr;
  pch = strtok (file,"\\");
  while (pch != NULL){ 
    svr = pch;
    pch = strtok (NULL, "\\");
  }
  return svr;
}  

void sendSketchName(){
  String temp = "{\"Name\":\""+ sketchName() + "\"  }";             // sends name of current sketch to requesting web page
  server.send(200, "application/json", temp); 
  #ifdef DEBUG
    Serial.printf("Current sketch name: ");
    Serial.print(sketchName());
    Serial.println(" sent successfully");
  #endif
}



/// LittleFS ////

void html_authentify (String& site) {                                                              // verifies authentication for calls via webserver
  if (!server.authenticate(www_username, www_password)) {
      return server.requestAuthentication();
    } 
  if (server.hasArg("delete")) {
        LittleFS.remove(server.arg("delete"));                                                      // deletes file
        server.sendHeader("Location","/spiffs.html");
        server.send(303);
  } else {
        File f = LittleFS.open(site, "r"); server.streamFile(f, contentType(site)); f.close(); 
  }
}

 

void send404() {
      File f = LittleFS.open("/404.html", "r"); server.streamFile(f, "text/html"); f.close(); 
}


const char Helper[] PROGMEM = R"(<form method="POST" action="/upload" enctype="multipart/form-data">              //enables uploading a file before spiffs.html is present on LittleFS
     <input type="file" name="upload"><input type="submit" value="Upload"></form>Lade die spiffs.html hoch.)";


const String &contentType(String& filename) {                                                   // identifies content type of file
  if (filename.endsWith(".htm") || filename.endsWith(".html")) filename = "text/html";
  else if (filename.endsWith(".css")) filename = "text/css";
  else if (filename.endsWith(".js")) filename = "application/javascript";
  else if (filename.endsWith(".json")) filename = "application/json";
  else if (filename.endsWith(".png")) filename = "image/png";
  else if (filename.endsWith(".gif")) filename = "image/gif";
  else if (filename.endsWith(".jpg")) filename = "image/jpeg";
  else if (filename.endsWith(".ico")) filename = "image/x-icon";
  else if (filename.endsWith(".xml")) filename = "text/xml";
  else if (filename.endsWith(".pdf")) filename = "application/x-pdf";
  else if (filename.endsWith(".zip")) filename = "application/x-zip";
  else if (filename.endsWith(".gz")) filename = "application/x-gzip";
  else filename = "text/plain";
  return filename;
}


bool handleFile(String&& path) {
  if (server.hasArg("delete")) {
    LittleFS.remove(server.arg("delete"));                                                          // deletes file
    server.sendHeader("Location","/spiffs.html");
    server.send(303); 
    return true;
  }
  if (!LittleFS.exists("/spiffs.html"))server.send(200, "text/html", Helper);                       // enables uploading a file before spiffs.html is present on LittleFS:
  if (path.endsWith("/")) path += "ota.html";
  if (enableAuth == 1){
    return LittleFS.exists(path) ? ({html_authentify(path); true;}) : false;
  } else {
    return LittleFS.exists(path) ? ({File f = LittleFS.open(path, "r"); server.streamFile(f, contentType(path)); f.close(); true;}) : false;
  }
}


void listener(){
  server.onNotFound([]() {
            if (!handleFile(server.urlDecode(server.uri())))
              send404();
             });               
  server.on("/showname", showDeviceName);
  server.on("/sketchName", sendSketchName);



  

/// OTA ///                                               // see: https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WebServer/examples/WebUpdate/WebUpdate.ino
      server.on("/ota", HTTP_POST, [=]() {
      digitalWrite(ledPin, led_aktiv);                    // -> LED on
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", (Update.hasError()) ? OTAfailMsg : OTAsuccessMsg);
      delay(1000);
      ESP.restart();
      }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        WiFiUDP::stopAll();
       #ifdef DEBUG
        Serial.printf("Update: %s\n", upload.filename.c_str());
       #endif
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) { //start with max available size
         #ifdef DEBUG
          Update.printError(Serial);
         #endif
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
         #ifdef DEBUG
          Update.printError(Serial);
         #endif
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
         #ifdef DEBUG
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
         #endif
        } else {
         #ifdef DEBUG
          Update.printError(Serial);
         #endif
        }
      } 
      yield();
    });
 /// End of OTA ///  
}



void setup(){
 #ifdef DEBUG
  Serial.begin(57600);
 #endif
  pinMode(ledPin, OUTPUT);                            // sets the digital pin as output
  digitalWrite(ledPin, led_aktiv);                    // -> LED on
  pinMode(relPin, OUTPUT);                            // sets the digital pin as output
  digitalWrite(relPin, relState);                     // relay off
  
  LittleFS.begin();
  
  loadConfig();
  wificonnect();
  server.begin();
  listener();
 }
 

void loop() {
 server.handleClient(); 
 switchButtonGuard();
}
