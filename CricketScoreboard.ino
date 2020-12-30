/* 
  CricketScoreboard - display maximum 16 digits
  using a TM1640 LED display drive control chip

  website files stored in 'data' folder 
  upload the contents of the data folder with LittleFS Data Upload Tool
  ("ESP8266 LittleFS Data Upload" in Tools menu of Arduino IDE)

  PJD 28-12-2020
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "LittleFS.h"
#include <Ticker.h>
#include <WEMOS_DHT12.h>
#include <WEMOS_TM1640.h>
#include "credentials.h"

//-------------------------------------------------------------
// DHT12 temperature sensor - SCL and SDA use default pins D1(GPIO5) and D2(GPIO4) respectively 
// TM1640 display           - CLK and DIN use default pins D5(GPIO14) and D7(GPIO13) respectively

String            score            = "0";
String            overs            = "0";
String            wicket           = "0";
String            target           = "0";
String            seconds          = "00";     // time in seconds sourced from http request
uint32_t          millipede        = 0;        // time stamp in millis
uint8_t           schedCount       = 0;        // scheduler counter
uint8_t           schedInterval    = 1;        // scheduler time interval
//-------------------------------------------------------------
ESP8266WebServer  webServer(80);
Ticker            timebomb;
DHT12             dht12;
TM1640            tm(4);                       // (intensity=4, dataPin=D7, clockPin=D5)

// character position assignment
// [0-2] runs  [3-5] target score  [6-7] over  [9-10] wicket  [11-15] time/temp (HHMM:)
/////////////////////////////////////////////////////////////////
String formatBytes(size_t bytes){
  //format bytes - human readable format
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
} // end of formatBytes
/////////////////////////////////////////////////////////////////
String getContentType(String filename){
  if(webServer.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
} // end of getContentType
/////////////////////////////////////////////////////////////////
bool displayTemperature(){
  // Display temperature - characters 11-15
  if (dht12.get()!=0) return false;
  uint8_t temp = dht12.cTemp;

  //Serial.printf("temperature: %i\n", temp);
   
  uint8_t t = temp/10;                            //tens
  uint8_t u = temp%10;                            //units
  
  if(t == 0) t=14;                               //suppress leading zeros
  
  //Serial.printf("display temperature:%i%i\n",t,u);
  
  tm.writeCharToBuffer(11, t);
  tm.writeCharToBuffer(12, u);
  tm.writeCharToBuffer(13, 10);                  //o degrees
  tm.writeCharToBuffer(14, 11);                  //C
  tm.display();
  return true;
} // end of displayTemperature
///////////////////////////////////////////////////////////////// 
void displayTime(){
  // Display time - characters 11-15
  if (millipede == 0) return;                     //haven't had a http request yet
  uint8_t pulse;
  uint32_t secNow = seconds.toInt() + ((millis() - millipede)/1000);
  uint8_t minutesNow = ((secNow)/60)% 60; 
  uint8_t hoursNow = ((secNow)/3600)% 24;
  
  if (hoursNow != 12) hoursNow = hoursNow % 12;   //convert to 12hr time

  uint8_t t = hoursNow/10;                        //tens
  uint8_t u = hoursNow%10;                        //units
  
  if(t == 0) t=14;                                //suppress leading zeros

  tm.writeCharToBuffer(11, t);
  tm.writeCharToBuffer(12, u);
  tm.writeCharToBuffer(13, minutesNow/10);
  tm.writeCharToBuffer(14, minutesNow%10);

  pulse = (secNow%2)? 0x03 : 0x00;
  tm.writeToBuffer(15, pulse);                   //pulse colon
  
  //Serial.printf("display time:%i%i:%i %i\n",t,u,minutesNow, pulse);
  
  tm.display();
} // end of displayTime
/////////////////////////////////////////////////////////////////
void displayScore(bool clearDisp=true){ 
  // Display score - characters 0-2 

  //Serial.printf("score: %i\n", score.toInt());

  //score
  uint8_t h = score.toInt()/100%10;     //hundreds
  uint8_t t = score.toInt()/10%10;      //tens
  uint8_t u = score.toInt()%10;         //units
  if(h == 0) h=14;                      //suppress leading zeros
  if(h == 14 && t == 0) t=14;
  
  tm.writeCharToBuffer(0, h);
  tm.writeCharToBuffer(1, t);
  tm.writeCharToBuffer(2, u);
  
  //target
  h = target.toInt()/100%10;            //hundreds
  t = target.toInt()/10%10;             //tens
  u = target.toInt()%10;                //units
  if(h == 0) h=14;                      //suppress leading zeros
  if(h == 14 && t == 0) t=14;
  
  tm.writeCharToBuffer(3, h);
  tm.writeCharToBuffer(4, t);
  tm.writeCharToBuffer(5, u);
  
  //Serial.printf("display score:%i%i%i\n",h,t,u);

  //over
  h = overs.toInt()/100%10;             //hundreds
  t = overs.toInt()/10%10;              //tens
  u = overs.toInt()%10;                 //units
  if(h == 0) h=14;                      //suppress leading zeros
  if(h == 14 && t == 0) t=14;
  
  tm.writeCharToBuffer(6, t);
  tm.writeCharToBuffer(7, u);
  tm.writeCharToBuffer(8, h);
  
  //wickets
  t = wicket.toInt()/10%10;             //tens
  u = wicket.toInt()%10;                //units
  if(t == 0) t=14;                      //suppress leading zeros
  
  tm.writeCharToBuffer(9, t);
  tm.writeCharToBuffer(10, u);

  //for (uint8_t i=0; i<16; i++){Serial.printf("display score disBuffer[%i] %i\n", i, tm.disBuffer[i]);}
  
  tm.display();

} // end of displayScore
/////////////////////////////////////////////////////////////////
void handleUpdate(){
  // parse parameters, update scoreboard and return message
  if(webServer.args() == 6){
    score         = webServer.arg("score");
    overs         = webServer.arg("overs");
    wicket        = webServer.arg("wicket");
    target        = webServer.arg("target");
    tm.intensity  = webServer.arg("brightness").toInt();
    seconds       = webServer.arg("seconds");
    millipede     = millis();
//    Serial.printf("score:%s\novers:%s\nwicket:%s\ntarget:%s\nbrightness:%i\nseconds:%s\n\n",
//      score.c_str(), overs.c_str(), wicket.c_str(), target.c_str(), tm.intensity, seconds.c_str());

    webServer.send(200, "text/plain", "success");
  }
  else {
    webServer.send(400, "text/plain", "invalid request message");
  }
  displayScore();
} // end of handleUpdate
/////////////////////////////////////////////////////////////////
bool handleFileRead(String path){
  Serial.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "sboard.htm";    // default page
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(LittleFS.exists(pathWithGz) || LittleFS.exists(path)){
    if(LittleFS.exists(pathWithGz))
      path += ".gz";
    File file = LittleFS.open(path, "r");
    size_t sent = webServer.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
} // end of handleFileRead
/////////////////////////////////////////////////////////////////
void schedule() {
  // schedule the display screens (called every 1 sec by timebomb)
  // blink the time : every 1 second and display temperature for 5 sec every 30 sec
  schedCount++;
  if(schedCount > 25){
    if (!displayTemperature()) displayTime();   //if read fails display time instead of temperature
  } 
  else
  {
    displayTime();  
  }
  schedCount = schedCount % 30;
} // end of schedule
/////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.printf("\n");
  //-------------------------------------------------------------
  // start LittleFS file system
  LittleFS.begin();
  {
    Dir dir = LittleFS.openDir("/");
    Serial.println("File System:");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf(" File: %-25s size: %8s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
  //-------------------------------------------------------------
  // start wifi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);
  Serial.println("");
  Serial.print("WiFi Access Point established! \nIP address: ");
  Serial.print(WiFi.softAPIP());
  Serial.print("   Network: ");
  Serial.println(ssid);
  //-------------------------------------------------------------
  // webServer initialisation
  webServer.on("/update", HTTP_GET, handleUpdate);         //update button

  webServer.onNotFound([](){                               //any other url not defined here
    if(!handleFileRead(webServer.uri()))                   //attempt to load content from LittleFS
      webServer.send(404, "text/plain", "File Not Found");
  });

  webServer.begin();
  Serial.println("Web Server started");
  //-------------------------------------------------------------
  // scheduler initialisation
  timebomb.attach(schedInterval, schedule);             //call schedule every 1 second
  //-------------------------------------------------------------
  // Initial Display on startup
  for (uint8_t digit = 0; digit < 16; digit++){
    tm.writeToBuffer(digit, 0x7f);                     //888 888 888 88 88888
  }
  tm.display();
  //for (byte i = 0; i < 16; i++){Serial.printf("buffer[%i] %i\n", i, tm.disBuffer[i]);}
} // end of setup
/////////////////////////////////////////////////////////////////
void loop() {
  webServer.handleClient();
}
