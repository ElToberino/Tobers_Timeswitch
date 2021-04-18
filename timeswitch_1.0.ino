//    *********************************************
//
//    TOBERS TIMESWITCH
//    FOR ESP8266
//
//    V 1.0 - 18.04.2021
//
//    *********************************************
//
//    TOBERS TIMESWITCH is a versatile and multifunctional time switch for ESP8266 devices, 
//    based on the great time switches created by Jens Fleischer and presented on https://www.fipsok.de.
//
//    Features:
//    - classic time switch functions widely configurable for every single day, adjustable number of switching times
//    - many different twilight modes configurable
//    - countdown timer
//    - master-client administration to program many devices by one click
//    - comfortable configuration of nearly all parameters via web interface with optional authentication for html sites
//        
//    For instructions and further information see: https://www.hackster.io/eltoberino/tobers-timeswitch-for-esp8266-ab3e06
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
//    ***************************************************************
//
//    successfully compiled with ARDUINO IDE 1.8.13
//
//    required board installation:
//    - ESP8266 core for Arduino -> https://github.com/esp8266/Arduino                 successfully compiled with V 2.7.4
//
//    required libraries:
//    - my fork of WifiManager library (development branch) by tzapu/tablatronix -> https://github.com/ElToberino/WiFiManager_for_Multidisplay
//
//    required files:
//    - all files delivered with this ino.file are required! -> the files in folder data must be uploaded on LittleFS
//      --> NOTE:	Flash Size Setting: Flash Size (FS) must be at least 192k! <--
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
//    PLEASE NOTICE THE COMMENTS and set the configuration parameters in section "USER SETTINGS" according to your setup.
//
//    ***************************************************************




#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <WiFiManager.h>                        // WiFiManager_for_Multidisplay, my fork of WifiManager library by tzapu/tablatronix -> https://github.com/ElToberino/WiFiManager_for_Multidisplay
#include <DNSServer.h>                          // required for WifiManager
#include <LittleFS.h>




//// --> USER SETTINGS <-- ////

//#define DEBUG                                   // show debug messages in serial monitor (57600)
//#define LOCAL_LANG                              // enable local language


// PIN DEFINITIONS :                            // see: https://tttapa.github.io/ESP8266/Chap04%20-%20Microcontroller.html

const uint8_t relPin = 12;                      // set pin for relay
const uint8_t aktiv = HIGH;                     // active state of relay

const uint8_t ledPin = 13;                      // set pin for status LED
const uint8_t led_aktiv = LOW;                  // active state of LED

const uint8_t buttonPin = 0;                    // set pin for manual switch button
const uint8_t button_aktiv = LOW;               // state of pressed button


// TIME ZONE :

const char* timezone =  "CET-1CEST,M3.5.0/02,M10.5.0/03";       // = CET/CEST  --> for adjusting your local time zone see: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv


//// --> END OF USER SETTINGS <-- ////




//// WIFI ////

#ifdef LOCAL_LANG
const char* AP_NAME = "Tobers_Zeitschaltuhr";         // Parameters of wifimanager configportal and/or operation as Access Point without WiFi connection
#else
const char* AP_NAME = "Tobers_Timeswitch";
#endif
const char* AP_PW = "";                               // set a password here if you don't want the AP to be open -> at least 8 characters !

IPAddress AP_IP(192,168,5,1);
IPAddress AP_Netmask(255,255,255,0);
uint8_t static_Mode_enabled = 0;

bool wifiManagerWasCalled = false;                    // is set to true if WifiManager is executed on startup
bool AP_established = false;                          // is set to true if WifiManager was skipped anddevice operates as AP

#ifdef LOCAL_LANG                                                                     
  const char replaceH1[] = "<script>document.addEventListener('DOMContentLoaded', () => {document.getElementsByTagName('h1')[0].innerHTML ='Tobers Zeitschaltuhr';})</script>";
#else                                                                                   
  const char replaceH1[] = "<script>document.addEventListener('DOMContentLoaded', () => {document.getElementsByTagName('h1')[0].innerHTML ='Tobers Time Switch';})</script>";
#endif




///// TIME SERVER AND DATE ////

char ntpServer[41] = "pool.ntp.org";                         // can be changed via web interface      // server pool prefix "2." could be necessary with IPv6
uint8_t refreshTime = 10;                                       // interval of time server call in minutes; can be configured via web interface
struct tm tm;
extern "C" uint8_t sntp_getreachability(uint8_t);               // shows reachability of NTP Server (value != 0 means server could be reached) see explanation in http://savannah.nongnu.org/patch/?9581#comment0:

char timeshow[10] = "XX:XX";                                    // array shwiwng current time
char date[9] = "00/00/00";                                      // array showing current date
char timestamp[25] = "no information";                          // time stamp of start up
bool timestampExists = false;                                   // is set to true after first successful time server call
bool DST_state;                                                 // is set true if DST (Daylight Saving Time) is active

unsigned long previousTimeCall = 0;

#ifdef LOCAL_LANG                                               // messages only called by print functions (FPSTR not reqired) can be saved in flash to save RAM
  const char msgTime[] PROGMEM = "Uhr";  
#else                                                                                   
  const char msgTime[] PROGMEM = "h";
#endif

const uint8_t numOfLogs = 10;                                   // number of server logs
char serverLog[numOfLogs][60];                                  // array for time server logs accessabke via web interface

double longitude = 11.393262;                                   // geographical longitude
double latitude = 47.268646;                                    // geographical latitude

char dawn[6];                                                   // twilight -> morning
char dusk[6];                                                   // twilight -> evening
char sunRise[6];                                                // sunrise
char sunSet[6];                                                 // sunset
uint8_t choice = 0;                                             // contains choice between sunrise/sunset or twilight as reference for switching times
uint8_t twilight_type = 6;                                      // defines value for twilight calculation: civil = 6, nautic = 12, astronomic = 18



//// WEBSERVER ////

ESP8266WebServer server(80);



//// AUTHENTICATION ////

uint8_t enableAuth = 1;                                             // enable html basic authentication (1 = enabled)                                           
char www_username[21] = "time";                                     // credendtials for html sites -> must be entered in browser to reach ALL html sites
char www_password[21] = "switch";                   



//// OTA ////

#ifdef LOCAL_LANG
    const char OTAsuccessMsg[] PROGMEM = "<div style=\"border:0.1em solid #e1c3a9; margin:0; padding:0.5em;font-size:0.8em; font-family:sans-serif; text-align:center; color:white; background-color:green;\">Update erfolgreich</span>";
    const char OTAfailMsg[] PROGMEM = "<div style=\"border:0.1em solid #e1c3a9; margin:0; padding:0.5em;font-size:0.8em; font-family:sans-serif; text-align:center; color:white; background-color:red;\">Update fehlgeschlagen</span>";
#else
    const char OTAsuccessMsg[] PROGMEM = "<div style=\"border:0.1em solid #e1c3a9; margin:0; padding:0.5em;font-size:0.8em; font-family:sans-serif; text-align:center; color:white; background-color:green;\">Update successful</span>";
    const char OTAfailMsg[] PROGMEM = "<div style=\"border:0.1em solid #e1c3a9; margin:0; padding:0.5em;font-size:0.8em; font-family:sans-serif; text-align:center; color:white; background-color:red;\">Update failed</span>";
#endif



//// TIME SWITCH ////

bool relState = !aktiv;                         // initial relState = off
char devicename[21];                            // name of time switch
const uint8_t maxSize = 40;                     // max number of switching times
uint8_t count = 4;                              // number of active switching times, configurable via web interface

struct Collection {                             // MAIN ARRAY containing all switching times and activation data
  bool fixed;                                   // all times activated/deactivated
  byte switchActive[maxSize];                   // individual activation state
  byte wday[maxSize];                           // selected weekdays
  char switchTime[maxSize * 2][6];              // switching times
  byte astro[maxSize];                          // replacement of switching times by astro values (0 = manual switching times active, 1 = astro switching times active)
} times;


unsigned long eggTimerStartTime;                // start time of egg timer in millis
bool eggTimerState = false;                     // egg timer activation state
uint16_t countdownTime;                         // countdown time in minutes
bool fixedFlag;                                 // saves 'fixed' state while egg timer is active
bool timesChangedflag;                          // changes when switching times have been updated


const uint8_t numOfClients = 20;                // Master mode: max number of clients
char UhrClients[numOfClients][17];              // Master mode: array with IP addresses of clients



//// FUNCTIONS ////


void wificonnect(){                                                     // read https://forum.arduino.cc/index.php?topic=652513 to understand how WiFi setup works on ESP
  uint8_t  wifi_retry = 0;
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
  }                                                                     // end of else (-> if file exists!)
  
  WiFi.softAPdisconnect(true);                                          // closes AP mode on startup to avoid ESP working as AP during normal operation
  
  while (WiFi.waitForConnectResult() != WL_CONNECTED && wifi_retry < 3) {               // retries connecting to WiFi if connection has failed on start up
        #ifdef DEBUG
          Serial.print("Not yet connected...retrying - Attempt No. ");
          Serial.println(wifi_retry+1);
        #endif
        WiFi.begin();
        delay(3000);
        wifi_retry++;
  } 

  if(wifi_retry >= 3) {
        #ifdef DEBUG
          Serial.println("no connection, starting AP");
          Serial.println("... starting AP");
        #endif
        WiFiManager wifiManager;
        wifiManager.useLittleFS();                                            // enables wifi manager loading files from LittleFS
        wifiManager.setAPStaticIPConfig(AP_IP, AP_IP, AP_Netmask);            // sets IP adress of AP with config portal
        wifiManager.setCustomHeadElement(replaceH1);                          // replaces "<h1>Tobers Multidisplay</h1>" in config portal html -> WifiManager_for_multidisplay can be used without changes
        wifiManager.setConfigPortalTimeout(300);                              // timeout for configportal in seconds
        wifiManager.startConfigPortal(AP_NAME, AP_PW);
        
        if (wifiManager.getTimeoutState() == true) {                          // if config portal has timed out, ESP restarts
          #ifdef DEBUG                                                        // (-> necessary after power blackout, when Home WiFi network needs some time to start up again)
            Serial.println("Config Portal has timed out. Restarting...");
          #endif
          delay(500);
          ESP.restart();  
        }
        
        wifiManagerWasCalled = true;
             
        if (wifiManager.getStaticMode() == true) {                            // gets information from WiFiManager if connection has been made with static IP
            static_Mode_enabled = 1;                                          // and writes it to file on LittleFS for future start ups. 
          }
        File f = LittleFS.open("/IP_mode.txt", "w");
        f.printf("%i\n%i\n%i\n%i\n%i\n", static_Mode_enabled, uint32_t(WiFi.localIP()), uint32_t(WiFi.gatewayIP()), uint32_t(WiFi.subnetMask()), uint32_t(WiFi.dnsIP()));
        f.close();
        #ifdef DEBUG
          Serial.println("IP Configuration saved on LittleFS:");
          Serial.print("Static IP enabled: ");
          Serial.println(static_Mode_enabled);
          Serial.print("IP: ");
          Serial.println(WiFi.localIP());
          Serial.print("Gateway: ");
          Serial.println(WiFi.gatewayIP());
          Serial.print("Subnet: ");
          Serial.println(WiFi.subnetMask());
          Serial.print("DNS: ");
          Serial.println(WiFi.dnsIP());
        #endif
  }
 
  if (WiFi.waitForConnectResult() == WL_CONNECTED){
        #ifdef DEBUG
          Serial.print("Connected to network \"");
          Serial.print(WiFi.SSID());
          Serial.print("\" with IP ");
          Serial.println(WiFi.localIP());
        #endif
  }
}

void checkIfAP(){                                                                      // opens access point if captive portal is skipped via "Exit"
  if (WiFi.status() != WL_CONNECTED && wifiManagerWasCalled == true) {
          WiFi.mode(WIFI_AP);
          delay(500);
         // WiFi.softAPConfig(AP_IP, AP_IP, AP_Netmask);
          WiFi.softAP(AP_NAME, AP_PW);
        #ifdef DEBUG
          Serial.print("AP started with IP ");
          Serial.println(WiFi.softAPIP());
        #endif
          AP_established = true;
    }

  if (WiFi.status() == WL_CONNECTED && wifiManagerWasCalled == true) {                  // restart necessary due to possible static IP settings
   #ifdef DEBUG
    Serial.println("Wifi ok, restarting device");
   #endif
    delay(500);
    ESP.restart();
    }
}

void getTimeFromServer(){
  uint8_t  time_retry=0;                                         // counter for connection attempts to time server
  struct tm initial;                                             // temp struct for checking if year==1970 (no received time information means year is 1970)
  initial.tm_year=70;
  static uint8_t z=0;                                            // counter for array serverLog

  while(initial.tm_year == 70 && time_retry < 15){                  
    if (esp8266::coreVersionNumeric() >= 20700000){               
      configTime(timezone, ntpServer); 
   } else {                                                      // ensures compatibility with ESP8266 Arduino Core Versions < 2.7.0
      setenv("TZ", timezone , 1);
      configTime(0, 0, ntpServer);
   }                            
    delay(500);
    if (sntp_getreachability(0) != 0){                          // if sntp_getreachability(0) == 0 -> ntp server call failed
      time_t now = time(&now);
      localtime_r(&now, &initial);
    }
    #ifdef DEBUG
      Serial.print("Time Server connection attempt: ");
      Serial.println(time_retry + 1);
      Serial.print("Reachabilty Value: ");
      Serial.println(sntp_getreachability(0));
      Serial.print("current year: ");
      Serial.println(1900 + initial.tm_year);
    #endif
   time_retry++;
  }

  if (time_retry >=15 && initial.tm_year == 70 ){
      snprintf(serverLog[z], sizeof(serverLog[z]), "%s - %s %s:<br>>> %s &#10060;", date, timeshow, msgTime, ntpServer);         // writes time and state (-> fail) of last 10 server calls into array serverLOG
      #ifdef DEBUG
        Serial.println("Connection to time server failed");
      #endif
  } else {
      time_t now = time(&now);
      localtime_r(&now, &tm);
      strftime (timeshow, sizeof(timeshow), "%H:%M", &tm);                                                                      // writes current time into array timeshow
      strftime (date, sizeof(date), "%y/%m/%d", &tm);                                                                           // writes current date into array date
      if(tm.tm_isdst == 1) DST_state = true;                                                                                    // checks if DST is active
        else DST_state = false;
      snprintf(serverLog[z], sizeof(serverLog[z]), "%s - %s %s:<br>>> %s  &#9989;", date, timeshow, msgTime, ntpServer);        // writes time and state (-> success) of last 10 server calls into array serverLOG
      if (timestampExists == false) {
         char tempTimestamp[25];
         strftime(tempTimestamp, sizeof(tempTimestamp), "%d.%m.%Y - %H:%M", &tm);              // writes time of first server call into array timestamp, see http://www.cplusplus.com/reference/ctime/strftime/
         snprintf(timestamp, sizeof(timestamp), "%s %s", tempTimestamp, msgTime);
         timestampExists = true;
      }
      #ifdef DEBUG
        Serial.print("Successfully requested current time from server: ");
        Serial.print(timeshow);
        Serial.println(DST_state ? "  -  Daylight Saving Time" : "  -  Standard Time"); 
      #endif
      digitalWrite(ledPin, !led_aktiv);                                                        // LED is turned off, if time has been loaded successfully on device startup!
    }
    z++;
    if (z == numOfLogs) z=0;       
  }


void timeGuard() {
  if (millis() - previousTimeCall > (refreshTime * 60000)){                         // calling time server every x minutes; "refreshTime" configurable via web interface
    getTimeFromServer();
    previousTimeCall = millis();
  }
  static bool last_DST_state = DST_state;
  static uint8_t lastday;
  
  if(tm.tm_isdst == 1) DST_state = true;                                            // checks if DST is active
    else DST_state = false; 
  
  if (tm.tm_mday != lastday || DST_state != last_DST_state) {                       // calculate astro times at 0:00 h and when DST state has changed
    lastday = tm.tm_mday;
    last_DST_state = DST_state;
    calculateSun();
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
        Serial.println("Relay manually activated by button press");         // if button press < 4 sec -> manual relay state change
      #endif
      relState = !relState;
    }
}
 


void singleTimerSwitch() {                                                  // sends, receives and processes timeswitch data via html sites
  pinMode(relPin, OUTPUT);
  digitalWrite(relPin, !aktiv);
  File file = LittleFS.open("/ctime.dat", "r");
  if (file && file.size() == sizeof(times)) {                               // loads data if file exists and if file size is ok
    file.read(reinterpret_cast<byte*>(&times), sizeof(times));
    file.close();
  } else {                                                                  // if file doesn't exist
    for (auto i = 0; i < count; i++) {
      times.switchActive[i] = 1;                                            // activation of all switching times
      times.wday[i] = ~times.wday[i];                                       // activation of all week days
      times.astro[i] = 0;                                                   // astro times deactivated for all times
    }
  }
  server.on("/timer", HTTP_POST, []() {
    if (server.args() == 1) {                                                             // activation/deactivation of switching time:
      times.switchActive[server.argName(0).toInt()] = server.arg(0).toInt();              // receives and processes activation state of switching times
      //printer();
      String temp = "\"";
      for (uint8_t j = 0; j < count; j++) temp += times.switchActive[j];    
      temp += "\"";
      server.sendHeader("Access-Control-Allow-Origin", "*");                              // required for master html sites, allows showing cross origin requests
      server.send(200, "application/json", temp);
    }
    if (server.hasArg("sTime")) {                                                         // setting new switching times:
      byte i = 0;                                                                         // receives and processes switching times and days
      char str[count * 14];
      strcpy (str, server.arg("sTime").c_str());
      char* ptr = strtok(str, ",");
      while (ptr != NULL) {
        strcpy (times.switchTime[i++], ptr);
        ptr = strtok(NULL, ",");
      }
      if (server.arg("sDay")) {
        i = 0;
        strcpy (str, server.arg("sDay").c_str());
        char* ptr = strtok(str, ",");
        while (ptr != NULL) {
          times.wday[i++] = atoi(ptr);
          ptr = strtok(NULL, ",");
        }
      }
      if (server.arg("State")) {                                                          // used only when called from master-main-switch.html:
        i = 0;                                                                            // ->  processes on/off state of every single time
        strcpy (str, server.arg("State").c_str());
        char* ptr = strtok(str, ",");
        while (ptr != NULL) {
          times.switchActive[i++] = atoi(ptr);
          ptr = strtok(NULL, ",");
        }
       }
       if (server.arg("SunValue")) {                                                      // used only when called from master-main-switch.html:
        i = 0;                                                                            // -> processes astro value -> values "choice" and "twilight_value" are not affected!
        strcpy (str, server.arg("SunValue").c_str());
        char* ptr = strtok(str, ",");
        while (ptr != NULL) {
          times.astro[i++] = atoi(ptr);
          ptr = strtok(NULL, ",");
        }
        processAstro();
       } 
      for (uint8_t j = count; j < maxSize; j++){                                          // deletes switching times and deactivates unused slots in array times                  
      snprintf(times.switchTime[j*2], sizeof(times.switchTime[j*2]), "0    ");
      snprintf(times.switchTime[(j*2)+1], sizeof(times.switchTime[(j*2)+1]), "0    ");
      times.switchActive[j] = 0;
      times.wday[j]=127;
      times.astro[j]=0;                                   
      }
      printer();                                                                          // call of saving function
    }
    String temp = "[";                                                                    // sends switching times, activated week days and activation state to html page
    for (uint8_t j = 0; j < (count*2); j++){
      if (temp != "[") temp += ',';
      temp += (String)"\"" + times.switchTime[j] + "\"";
    }
    temp += ",\"";
    for (uint8_t j = 0; j < count; j++) {
      temp += times.switchActive[j];
    }
    for (uint8_t j = 0; j < count; j++) {
      temp += "\",\"";
      temp += times.wday[j];
    }
    temp += "\",\"";
    for (uint8_t j = 0; j < count; j++) {
      temp += times.astro[j];
    }
    temp += "\"]";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", temp);
  });
  server.on("/timer", HTTP_GET, []() {
    if (server.hasArg("tog") && server.arg(0) == "tog") relState = !relState;             // manual change of relay state initiated by button press on website
    if (server.hasArg("tog") && server.arg(0) == "fix") {
      times.fixed = !times.fixed;                                                         // activate/deactivate all switching times initiated by button press on website
      #ifdef DEBUG
        Serial.printf("All switching times %s\n", times.fixed == true ? "deactivated" : "activated");
      #endif
    }
    if (server.hasArg("tog") && server.arg(0) == "go") times.fixed = false;               // required for master function: acivates/deactivates all time switches simultaniously, independent from their current state
    if (server.hasArg("tog") && server.arg(0) == "stop") times.fixed = true;              //
    if (server.hasArg("tog") && server.arg(0) == "on") relState = aktiv;                  //
    if (server.hasArg("tog") && server.arg(0) == "off") relState = !aktiv;                //
     
    String singleTimeStates;
    for (uint8_t j = 0; j < count; j++) singleTimeStates += times.switchActive[j];
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", (String)"[\"" + (relState == aktiv) + "\",\"" + timeshow + "\",\"" + times.fixed + "\",\"" + singleTimeStates + "\",\"" + timesChangedflag + "\",\"" + count + "\"]");    
  });
}



void printer() {                                                                          // writes all data of main array 'times' to file ctime.dat on LittleFS
  File file = LittleFS.open("/ctime.dat", "w");
  if (file) {
    file.write(reinterpret_cast<byte*>(&times), sizeof(times));
    file.close();
   #ifdef DEBUG
      Serial.println("Circuit times saved successfully");
   #endif
   timesChangedflag = !timesChangedflag;
  }
}



void timerSwitch() {                                                                      // monitors time and switching times and activates/deactivates relay
  static uint8_t lastmin = CHAR_MAX;
  static uint8_t lastState = aktiv;
  time_t now = time(&now);
  localtime_r(&now, &tm);
  if (tm.tm_min != lastmin) {
    lastmin = tm.tm_min;
    strftime (timeshow, sizeof(timeshow), "%H:%M", &tm);  
    #ifdef DEBUG  
      Serial.print("current time: ");
      Serial.println(timeshow);
    #endif
    if (!times.fixed){
      char buf[6];
      sprintf(buf, "%.2d:%.2d", tm.tm_hour, tm.tm_min);
      for (auto i = 0; i < count * 2; i++) {
        if (times.switchActive[i / 2] && !strcmp(times.switchTime[i], buf)) {
          if (times.wday[i / 2] & (1 << (tm.tm_wday ? tm.tm_wday - 1 : 6))) relState = i & 1 ? !aktiv : aktiv;
          }
        }
      }
   }
    
  if (relState != lastState) {                                                                  // turns relay on/off if its state has changed
    lastState = relState;
    digitalWrite(relPin, relState);
    #ifdef DEBUG
      Serial.printf("Relay o%s\n", digitalRead(relPin) == aktiv ? "n" : "ff");
    #endif
  }
}


void startEggTimer() {                                                                          // starts countdown timer
 if (server.hasArg("EggTime")) {
  if (eggTimerState == false)  fixedFlag = times.fixed;                                         // keeps 'fixed state' before egg timer start while timer is running and times are fixed
  countdownTime = server.arg("EggTime").toInt();
  eggTimerStartTime = millis();
  eggTimerState = true;
  times.fixed = true;                                                                           // fixes all switching times while timer is running
  relState = aktiv;
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "");
  #ifdef DEBUG
      Serial.printf("Saved Fixed State: %s\n", fixedFlag ? "true" : "false");
      Serial.printf("Egg Timer started: Relay on for %i minutes\n", countdownTime);    
  #endif     
 }
 if (server.hasArg("EggStop")) {                                                               // manual stop of egg timer
    eggTimerState = false;
    relState = !aktiv;
    times.fixed = fixedFlag;
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "");
    #ifdef DEBUG
      Serial.println("Egg Timer manually switched off");
      Serial.printf("Restored Fixed State: %s\n", fixedFlag ? "true" : "false");
    #endif
 }
}



void showEggTimeLeft(){                                                                       // sends egg timer state and/or remaining time to requesting html site
 String temp;
 if (eggTimerState == true){
  unsigned long leftSecs = (countdownTime*60) - ((millis()- eggTimerStartTime)/1000);
  temp = "{\"TimeLeft\":\""+ String(leftSecs) + "\", \"Relais\":\""+ String(relState == aktiv) + "\"}";
 } else {
  temp = "{\"TimeLeft\":\"inaktiv\", \"Relais\":\""+ String(relState == aktiv) + "\"}";
 }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", temp);
}


void eggTimerGuard() {                                                                      // monitors eggTimer state / countdown time and switches off relay when time has elapsed
if (millis() - eggTimerStartTime >= (countdownTime*60000) && eggTimerState == true) {        
        eggTimerState = false;
        relState = !aktiv;
        times.fixed = fixedFlag;
        #ifdef DEBUG
          Serial.println("Egg Timer off");
          Serial.printf("Restored Fixed State: %s\n", fixedFlag ? "true" : "false");
        #endif
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



void showCounts() {                                                                                // sends number of switching times (counts) to html sites 
  String helper = "";
  for (uint8_t j = 0; j < count; j++) helper += times.astro[j]; 
  String temp = "{\"No1\":\""+ String(count) + "\", \"No2\":\""+ helper + "\"}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", temp);
  #ifdef DEBUG
      Serial.printf("Counts (number of active switching times) sent successfully: %i\n", count);
  #endif
}



void showConfig() {                                                                              // sends all configuration data to requesting html page
  String Network_Name = "Onkelweb";
  String Network_IP = "111.111.1.111";
  if(AP_established == false){
    Network_Name = WiFi.SSID().c_str();
    Network_IP = WiFi.localIP().toString();
  } else {
    String MyAP = AP_NAME;
    Network_Name = "AP " + MyAP;
    Network_IP = WiFi.softAPIP().toString();    
  }
  String temp = "{\"Name\":\""+ String(devicename) + "\", \"Timestamp\":\""+ String(timestamp) + "\", \"SSID\":\""+ Network_Name + "\", \"IP\":\""+ Network_IP + 
                "\", \"CNT\":\""+ String(count)  + "\", \"MaxS\":\""+ String(maxSize)+ "\", \"NTP\":\""+ String(ntpServer)  + "\", \"REF\":\""+ String(refreshTime)  + 
                "\", \"USER\":\""+ String(www_username)  + "\", \"PW\":\""+ String(www_password)  + "\" , \"EnaAuth\":\""+ String(enableAuth)  + 
                "\" , \"Long\":\""+ String(longitude, 10)  + "\" , \"Lat\":\""+ String(latitude, 10) + "\", \"TWI\":\""+ String(twilight_type) + "\"}";
                
  server.send(200, "application/json", temp);
  #ifdef DEBUG
    Serial.println("Successfully sent current configuration data:");
    Serial.printf("Device Name:\"%s\", Counts (number of active switching times):\"%i\", NTP Server:\"%s\", NTP Refresh Time:\"%i\", Enable Auth:%i, Username:\"%s\", "
                  "Password:\"%s\", Timestamp:\"%s\", Longitude: %f,  Latitude: %f, Twilight Type:%i\n", devicename, count, ntpServer, refreshTime, enableAuth, www_username, 
                  www_password, timestamp, longitude, latitude, twilight_type);
   #endif
}




void handleConfig() {                                                                               // processes configuration data received from html page
  if (server.hasArg("CNT")){
    count = server.arg("CNT").toInt();
    uint8_t saver = server.arg("SAVER").toInt();
    if (count > maxSize) count = maxSize;
    if (count == 0) count = 1;
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "");
    for (uint8_t j = count; j < maxSize; j++){                                                       // deletes switching times and deactivates unused slots in array times
        snprintf(times.switchTime[j*2], sizeof(times.switchTime[j*2]), "0    ");
        snprintf(times.switchTime[(j*2)+1], sizeof(times.switchTime[(j*2)+1]), "0    ");
        times.switchActive[j] = 0;
        times.wday[j]=127;
        times.astro[j] = 0;                                                                         
    }
    #ifdef DEBUG
      Serial.printf("Number of active switching times: \"%i\" received and processed successfully\n", count);
    #endif
    if (saver==1) printer();
  }
  if (server.hasArg("newName")){
   snprintf(devicename, sizeof(devicename), "%s",server.arg("newName").c_str());
   server.send(200, "text/plain", "");                                                            // sends "ok" state and blank message to invisible iframe of requesting html site   
   #ifdef DEBUG
      Serial.printf("Device Name \"%s\" received and processed successfully\n", devicename);
   #endif
  }
  if (server.hasArg("NTP")){
   snprintf(ntpServer, sizeof(ntpServer), "%s", server.arg("NTP").c_str());
   server.send(200, "text/plain", "");
  #ifdef DEBUG
      Serial.printf("New NTP Server: \"%s\" received and processed successfully\n", ntpServer);
  #endif
   getTimeFromServer();
   previousTimeCall = millis();
  }
  if (server.hasArg("REF")){
   refreshTime = server.arg("REF").toInt();
   if (refreshTime == 0) refreshTime = 1;
   server.send(200, "text/plain", "");
  #ifdef DEBUG
      Serial.printf("New NTP Refresh Time: \"%i\" received and processed successfully\n", refreshTime);
  #endif
  }
  if (server.hasArg("USER")){
   snprintf(www_username, sizeof(www_username), "%s", server.arg("USER").c_str());
   snprintf(www_password, sizeof(www_password), "%s", server.arg("PW").c_str());
   enableAuth = server.arg("EnableAuth").toInt();
   server.send(200, "text/plain", "");
  #ifdef DEBUG
      Serial.printf("New User Name:\"%s\", New Password:\"%s%\", Enable Auth:%i received and processed successfully\n", www_username, www_password, enableAuth);     
  #endif
  }
  if (server.hasArg("LONG")){
   longitude = server.arg("LONG").toDouble();
   latitude = server.arg("LAT").toDouble();
   twilight_type = server.arg("TWI").toInt();
   server.send(200, "text/plain", "");
  #ifdef DEBUG
      Serial.printf("New Coordinates: Longitude: %f,  Latitude: %f, Twilight Type:%i  received and processed successfully\n", longitude, latitude, twilight_type);
  #endif
  calculateSun();
  printer();
  }
   File f = LittleFS.open("/config.txt", "w"); 
   f.printf("%i\n%s\n%i\n%s\n%s\n%i\n%f\n%f\n%s\n%i\n%i\n", count, ntpServer, refreshTime, www_username, www_password, enableAuth, longitude, latitude, devicename, choice, twilight_type);
   f.close();
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
    count = temp0.toInt();
    String temp1 = f.readStringUntil('\n');
    temp1.trim();
    if (temp1 != "") snprintf(ntpServer, sizeof(ntpServer), "%s", temp1.c_str());
    String temp2 = f.readStringUntil('\n');
    temp2.trim();
    if (temp2 != "") refreshTime = temp2.toInt();
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
    longitude = temp6.toDouble();
    String temp7 = f.readStringUntil('\n');
    temp7.trim();
    latitude = temp7.toDouble();
    String temp8 = f.readStringUntil('\n');
    temp8.trim();
    snprintf(devicename, sizeof(devicename), "%s", temp8.c_str());
    String temp9 = f.readStringUntil('\n');
    temp9.trim();
    choice = temp9.toInt();
    String temp10 = f.readStringUntil('\n');
    temp10.trim();
    twilight_type = temp10.toInt();
    f.close();
    #ifdef DEBUG
      Serial.printf("\n Loaded from file successfully:\n NTP Server:\"%s\", Counts (number of active switching times):\"%i\", NTP Refresh Time:\"%i\", Enable Auth:%i, Username:\"%s\", "
                    "Password:\"%s\", Longitude:%f, Latitude:%f, Device Name \"%s\", Choice:%i, Twilight Type:%i\n", ntpServer, count, refreshTime, enableAuth, www_username, 
                    www_password, longitude, latitude, devicename, choice, twilight_type);
    #endif
}



void showClients(){                                                                              // master function: sends all clients' IP addresses to html page
  String dlf = "{\"IPs\":[";
    for( int t =0; t < numOfClients; t++) {
      if(strcmp(UhrClients[t], "") == 0) snprintf(UhrClients[t], sizeof(UhrClients[t]), "0.0.0.0");       // if UhrClients[t]="" -> "0.0.0.0" is written into array
      dlf += "\""; 
      dlf += UhrClients[t];
      dlf += "\"";
      if (t < (numOfClients-1)) dlf += ","; 
    }
  dlf += "], \"HOST\":\""+ WiFi.localIP().toString() + "\"}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", dlf);
  #ifdef DEBUG
    Serial.print("Client IPs sent successfully: ");
    Serial.println(dlf);
  #endif
}


void handleClients(){                                                                           // master function: writes clients' IP addresses into file on LittleFS
  if (server.hasArg("newIP_0")){
    #ifdef DEBUG
      Serial.println("Successfully received the following client IPs: ");
    #endif
    for (int t=0; t < numOfClients; t++){
      String provi = "newIP_" + String(t);
      snprintf(UhrClients[t], sizeof(UhrClients[t]), "%s",server.arg(provi).c_str());
      #ifdef DEBUG
        Serial.printf("%s, ", UhrClients[t]);
      #endif  
    }
  server.sendHeader("Access-Control-Allow-Origin", "*");  
  server.send(200, "text/plain", "");
  File f = LittleFS.open("/clients.txt", "w");
  for (int t=0; t < numOfClients; t++){
    f.printf("%s\n", UhrClients[t]);
  }
  f.close();
  #ifdef DEBUG
    Serial.println("");
    Serial.println("Client IPs successfully written in file");
  #endif
  }
}


void loadClients () {                                                                           // master function: loads all clients' IP addresses from file on LittleFS
    File f = LittleFS.open("/clients.txt", "r");
    if(!f) {
      #ifdef DEBUG
        Serial.println("opening \"clients.txt\" failed");
      #endif
      return;
    }
    #ifdef DEBUG
      Serial.println("Following client IPs successfully loaded from file: ");
    #endif
    for (int t=0; t < numOfClients; t++){
      String temp = f.readStringUntil('\n');
      temp.trim();
      snprintf(UhrClients[t], sizeof(UhrClients[t]), "%s", temp.c_str());
      #ifdef DEBUG
        Serial.printf("%s, ", UhrClients[t]);
      #endif
    }
    #ifdef DEBUG
      Serial.println("");
    #endif
    f.close();
}



void showLog() {                                                                  // sends log of time server requests
  String temp = "{\"LOGS\":[";
  for (uint8_t i = 0; i < numOfLogs; i++){
    temp += "\"" + String(serverLog[i]) + "\"";
    if (i < (numOfLogs-1)) temp+= ",";
  }
  temp += "],\"Name\":\"" + String(devicename) + "\" ,\"RFT\":\"" + String(refreshTime) + "\"";
  temp += "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", temp);
  #ifdef DEBUG
      Serial.println("Server Log successfully sent");
  #endif
}


void showAstro() {                                                                // sends astronomical data
  char aujourd[11];
  strftime(aujourd, sizeof(aujourd), "%d.%m.%Y", &tm);
  String helper = "";
      for (uint8_t j = 0; j < count; j++) helper += times.astro[j]; 
  String temp = "{\"No1\":\""+ String(devicename) + "\", \"Rise\":\""+ String(sunRise) + "\", \"Dawn\":\""+ String(dawn) + "\", \"Set\":\""+ String(sunSet) + 
                "\", \"Dusk\":\""+ String(dusk) + "\", \"Astro\":\"" + helper + "\", \"Choice\":\"" + String(choice) + "\", \"Counts\":\""+ String(count) + 
                "\", \"Date\":\"" + String(aujourd) + "\" }";
                
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", temp);
  #ifdef DEBUG
      Serial.printf("Device Name \"%s\", Dawn:%s, Sunrise:%s, Sunset:%s, Dusk:%s, Value:%s, Choice:%i, Counts:%i, Date:%s sent successfully\n", 
                    devicename, dawn, sunRise, sunSet, dusk, helper.c_str(), choice, count, aujourd);    
  #endif
}


void handleAstro(){                                                              // processes and saves astronomical data sent from html site                              
  if (server.hasArg("Astro")){
    String prov = server.arg("Astro");
    for(uint8_t t=0;t<count;t++){
      String temp;
      temp = prov.charAt(t);
      times.astro[t] = temp.toInt();
    }
    choice = server.arg("Choice").toInt();
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "");
    processAstro();
    printer();
    handleConfig();
    #ifdef DEBUG
      Serial.printf("Successfully received the following Astro value:%s, Choice:%i\n", prov.c_str(), choice);
    #endif
  }  
}


void processAstro(){                                                            // sets astro type according to users choice and sets switching times
  char astroMorning[6];
  char astroEvening[6];
  if (choice==0){
    snprintf(astroMorning, sizeof(astroMorning), sunRise );
    snprintf(astroEvening, sizeof(astroEvening), sunSet );
  } 
  if (choice==1){
    snprintf(astroMorning, sizeof(astroMorning), sunRise );
    snprintf(astroEvening, sizeof(astroEvening), dusk);
  }
   if (choice==2){
    snprintf(astroMorning, sizeof(astroMorning), dawn );
    snprintf(astroEvening, sizeof(astroEvening), sunSet);
  }
  if (choice==3){
    snprintf(astroMorning, sizeof(astroMorning), dawn );
    snprintf(astroEvening, sizeof(astroEvening), dusk);
  }
  for(uint8_t j=0;j<count;j++){
    switch (times.astro[j]) {
      case 1: 
        snprintf(times.switchTime[j*2], sizeof(times.switchTime[j*2]), astroEvening);                       // evening on
        break;
      case 2: 
        snprintf(times.switchTime[(j*2)+1], sizeof(times.switchTime[(j*2)+1]), astroEvening);               // evening off
        break;
      case 3: 
        snprintf(times.switchTime[j*2], sizeof(times.switchTime[j*2]), astroMorning);                       // morning on
        snprintf(times.switchTime[(j*2)+1], sizeof(times.switchTime[(j*2)+1]), astroEvening);               // evening off
        break;
      case 4: 
        snprintf(times.switchTime[j*2], sizeof(times.switchTime[j*2]), astroEvening);                       // evening on
        snprintf(times.switchTime[(j*2)+1], sizeof(times.switchTime[(j*2)+1]), astroMorning);               // morning off
        break;
      case 5: 
        snprintf(times.switchTime[j*2], sizeof(times.switchTime[j*2]), astroMorning);                       // morning on
        break;
      case 6: 
        snprintf(times.switchTime[(j*2)+1], sizeof(times.switchTime[(j*2)+1]), astroMorning);               // morning off
        break;
    }
  }
}


void resetDevice(){                                                 // restarts device
  ESP.restart();
  #ifdef DEBUG
   Serial.println("restarting ESP...");
  #endif
}


void clearCredentials() {                                           // erases WiFi credentials persistently saved in flash memory and restarts device 
  #ifdef DEBUG                                                      //     -> WifiManager captive portal is shown on follwing start up
   Serial.println("erasing WiFi credentials");
  #endif
  static_Mode_enabled = 0;
  File f = LittleFS.open("/IP_mode.txt", "w");                        // resets IP config file on LittleFS to default values
  f.printf("%i\n%i\n%i\n%i\n%i\n", static_Mode_enabled, uint32_t(WiFi.localIP()), uint32_t(WiFi.gatewayIP()), uint32_t(WiFi.subnetMask()), uint32_t(WiFi.dnsIP()));
  f.close();
  
  WiFi.disconnect(true);                                            // erases WiFi credentials

  #ifdef DEBUG
   Serial.println("restarting");
  #endif
  delay(5000);
  ESP.restart();
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



/// ASTRO CALCULATING FUNCTIONS ///                                               // based on  https://forum.arduino.cc/index.php?topic=218280.msg1596349#msg1596349
                                                                                  //      and  https://fipsok.de/Esp8266-Webserver/sonnenaufgang-esp8266.tab

double julianDate (int year, int month, int day) {                                // converts gregorian date to julian date 
  if (month <= 2) {
    month = month + 12;
    year = year - 1;
  }
  int gregorian = (year / 400) - (year / 100) + (year / 4);
  return 2400000.5 + 365.0 * year - 679004.0 + gregorian + (30.6001 * (month + 1)) + day + 12.0 / 24.0;
}

double InPi(double x) {
  int n = x / TWO_PI;
  x = x - n * TWO_PI;
  if (x < 0) x += TWO_PI;
  return x;
}

double calculate_EoT(double &DK, double T) {                                                      // equation of time -> see: https://en.wikipedia.org/wiki/Sunrise_equation#Calculate_sunrise_and_sunset
  double RAm = 18.71506921 + 2400.0513369 * T + (2.5862e-5 - 1.72e-9 * T) * T * T;
  double M  = InPi(TWO_PI * (0.993133 + 99.997361 * T));
  double L  = InPi(TWO_PI * (  0.7859453 + M / TWO_PI + (6893.0 * sin(M) + 72.0 * sin(2.0 * M) + 6191.2 * T) / 1296.0e3));
  double e = DEG_TO_RAD * (23.43929111 + (-46.8150 * T - 0.00059 * T * T + 0.001813 * T * T * T) / 3600.0);                           // earth's axial tilt -> see: https://en.wikipedia.org/wiki/Axial_tilt
  double RA = atan(tan(L) * cos(e));
  if (RA < 0.0) RA += PI;
  if (L > PI) RA += PI;
  RA = 24.0 * RA / TWO_PI;
  DK = asin(sin(e) * sin(L));
  RAm = 24.0 * InPi(TWO_PI * RAm / 24.0) / TWO_PI;
  double dRA = RAm - RA;
  if (dRA < -12.0) dRA += 24.0;
  if (dRA > 12.0) dRA -= 24.0;
  dRA = dRA * 1.0027379;
  return dRA ;
}

char* outputFormat(double sunTime) {
  if (sunTime < 0) sunTime += 24;
  else if (sunTime >= 24) sunTime -= 24;
  int8_t decimal = 60 * (sunTime - static_cast<int>(sunTime)) + 0.5;
  int8_t predecimal = sunTime;
  if (decimal >= 60) {
    decimal -= 60;
    predecimal++;
  }
  else if (decimal < 0) {
    decimal += 60;
    predecimal--;
    if (predecimal < 0) predecimal += 24;
  }
  static char buf[6];
  snprintf(buf, sizeof(buf), "%.2d:%.2d", predecimal, decimal);
  return buf;
}

void calculateSun() {                                                                                                           // see example of astro data and different twilight types: https://www.timeanddate.com/sun/austria/innsbruck
    const double w = latitude * DEG_TO_RAD;
    double JD = julianDate(1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday);
    double T = (JD - 2451545.0) / 36525.0;
    double DK;
    double EoT = calculate_EoT(DK, T);
    double h = -0.833333333333333 * DEG_TO_RAD;                                                                                 // value of h for sunrise/sunset
    double differenceTime = 12.0 * acos((sin(h) - sin(w) * sin(DK)) / (cos(w) * cos(DK))) / PI;
    strcpy(sunRise, outputFormat((12.0 - differenceTime - EoT) - longitude / 15.0 + (_timezone * -1) / 3600 + tm.tm_isdst));
    strcpy(sunSet, outputFormat((12.0 + differenceTime - EoT) - longitude / 15.0 + (_timezone * -1) / 3600 + tm.tm_isdst));
    h = (-1 * twilight_type) * DEG_TO_RAD;                                                                                      // h => -6.0 = civil twilight , -12 = nautical tw., -18 = astronomical tw.
    differenceTime = 12.0 * acos((sin(h) - sin(w) * sin(DK)) / (cos(w) * cos(DK))) / PI;
    strcpy(dawn, outputFormat((12.0 - differenceTime - EoT) - longitude / 15.0 + (_timezone * -1) / 3600 + tm.tm_isdst));
    strcpy(dusk, outputFormat((12.0 + differenceTime - EoT) - longitude / 15.0 + (_timezone * -1) / 3600 + tm.tm_isdst));
  #ifdef LOCAL_LANG
    Serial.printf("Astro-Daten:\nMorgendämmerung: %s - Sonnenaufgang: %s\nSonnenuntergang: %s - Abenddämmerung: %s\n" , dawn, sunRise, sunSet, dusk);
  #else
    Serial.printf("Astro Data:\nDawn: %s - Sunrise: %s\nSunset: %s - Dusk: %s\n" , dawn, sunRise, sunSet, dusk);
  #endif
    processAstro();
}




/// LITTLE_FS ////

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
     <input type="file" name="upload"><input type="submit" value="Upload"></form>Please upload spiffs.html.)";


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
  if (path.endsWith("/")) path += "index.html";
  if (enableAuth == 1){
    return LittleFS.exists(path) ? ({html_authentify(path); true;}) : false;
  } else {
    return LittleFS.exists(path) ? ({File f = LittleFS.open(path, "r"); server.streamFile(f, contentType(path)); f.close(); true;}) : false;
  }
}


const String formatBytes(size_t const& bytes) {                                                   // transforms file size into common format
  return (bytes < 1024) ? String(bytes) + " Byte" : (bytes < (1024 * 1024)) ? String(bytes / 1024.0) + " KB" : String(bytes / 1024.0 / 1024.0) + " MB";
}


void formatSpiffs() {                                                                             // formats LittleFS Memory
  LittleFS.format();
  server.sendHeader("Location","/spiffs.html");
  server.send(303); 
}


void handleList() {                                                                         // sends a list of all files on LittleFS to requesting html page
  FSInfo fs_info;  LittleFS.info(fs_info);                                                    // fills up FSInfo structure with information about file system
  Dir dir = LittleFS.openDir("/");                                                            // lists all files saved on LittleFS
  String temp = "[";
  while (dir.next()) {
    if (temp != "[") temp += ',';
    temp += "{\"name\":\"" + dir.fileName() + "\",\"size\":\"" + formatBytes(dir.fileSize()) + "\"}";
  }
  temp += ",{\"usedBytes\":\"" + formatBytes(fs_info.usedBytes * 1.05) + "\"," +             // calculates used memory + additional 5% for safety reasons
          "\"totalBytes\":\"" + formatBytes(fs_info.totalBytes) + "\",\"freeBytes\":\"" +    // shows total memory
          (fs_info.totalBytes - (fs_info.usedBytes * 1.05)) + "\"}]";                        // calculates free memory + additional 5% for safety reasons
  server.send(200, "application/json", temp);
}


void handleUpload() {                                                                         // loads file from computer and saves it to LittleFS
  static File fsUploadFile;                                                                   // keeps current upload
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (upload.filename.length() > 30) {
      upload.filename = upload.filename.substring(upload.filename.length() - 30, upload.filename.length());        // shortens file name to a length of 30 characters
    }
   #ifdef DEBUG
    Serial.printf("handleFileUpload Name: /%s\n", upload.filename.c_str());
   #endif
    fsUploadFile = LittleFS.open("/" + server.urlDecode(upload.filename), "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
   #ifdef DEBUG 
    Serial.printf("handleFileUpload Data: %u\n", upload.currentSize);
   #endif
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile)
      fsUploadFile.close();
   #ifdef DEBUG
    Serial.printf("handleFileUpload Size: %u\n", upload.totalSize);
   #endif
    server.sendHeader("Location","/spiffs.html");
    server.send(303); 
  }
}



void listener(){
  server.onNotFound([]() {
            if (!handleFile(server.urlDecode(server.uri())))
              send404();
             });
                   
  server.on("/json", handleList);
  server.on("/format", formatSpiffs);
  server.on("/upload", HTTP_POST, []() {}, handleUpload);
  server.on("/showname", showDeviceName);
  server.on("/showCounts", showCounts);
  server.on("/showConfig", showConfig);
  server.on("/postCounts", handleConfig);
  server.on("/postConf", handleConfig);
  server.on("/showClients", showClients);
  server.on("/postIPs", handleClients);
  server.on("/reset", resetDevice);
  server.on("/ex", clearCredentials);
  server.on("/sketchName", sendSketchName);
  server.on("/showLog", showLog);
  server.on("/eggStart", startEggTimer);
  server.on("/showEgg", showEggTimeLeft);
  server.on("/showAstro", showAstro);
  server.on("/postAstro", handleAstro);
}

  

/// OTA ///                                               // see: https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WebServer/examples/WebUpdate/WebUpdate.ino
      
void enableOTA(){      
  server.on("/ota", HTTP_POST, [=]() {
  digitalWrite(ledPin, led_aktiv);                        // LED on
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
}
 




void setup(){
 #ifdef DEBUG
  Serial.begin(57600);                               // starts serial communication
 #endif
  pinMode(ledPin, OUTPUT);                           // sets the pin as output
  digitalWrite(ledPin, led_aktiv);                   // LED on
  LittleFS.begin();                                  // sets up file system
  loadConfig();                                      // loads and processes configuration
  loadClients();                                     // loads and processes client list
  wificonnect();                                     // connects to WiFi 
  checkIfAP();                                       // checks if WifiManager config portal has been skipped
  if(AP_established == false){                       // normal operation with wifi connection
      getTimeFromServer();                           // calls time server for current time
  }
  server.begin();                                    // starts webserver
  singleTimerSwitch();                               // sets up timeswitch
  listener();                                        // defines actions for calls to webserver
  enableOTA();                                       // enables over the air update
 }
 

void loop() {
 server.handleClient();                              // handles webserver
 timerSwitch();                                      // handles time switch functions
 timeGuard();                                        // handles NTP call intervalls
 eggTimerGuard();                                    // handles countdown timer
 switchButtonGuard();                                // handles switch button inputs
}
