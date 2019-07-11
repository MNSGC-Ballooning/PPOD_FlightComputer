#include <UbloxGPS.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <RelayXBee.h>
#include <SD.h>
#include "src\Smart.h"

#define chipSelect 53
#define smartPin 9
#define onlight 7
#define temp_pin A0 //The light that turns on when it is powered up and remains on
#define press_pin A1
#define gps_status_pin A2 //blue, flashes every 10 seconds if fix, every second if not
#define smart_status_pin A3 //yellow, flshes every 10 seconds if released
#define sd_status_pin A4 //red, flashes every 10 seconds if logging, every second if not

File datalog;
char filename[] = "SDPPOD00.csv"; //File name template
bool SDactive = false;

bool fix = false;
bool released = false;
String ID = "POD";
String command;
float cutTime = 70; //Time in minutes is this right??? 70,000 feet approx.
float cutAlt = 18288; //Alt in meters
float currentAlt = 0;
String rawGPS;
short status_counter = 0;
bool status_engaged = false;

SoftwareSerial ss = SoftwareSerial(6,7);
SoftwareSerial ssGPS = SoftwareSerial(2,3);
RelayXBee xBee = RelayXBee(&ss, ID);
TinyGPSPlus gps;
Smart smart = Smart(smartPin);

void setup() {
  Serial.begin(9600);
  //smart.initialize();
  ssGPS.begin(UBLOX_BAUD);
  Serial.println("GPS serial set");
  //gps.init();
  Serial.println("GPS initialized");
  //delay(50);
  //gps.setAirborne();
  //(gps.setAirborne()){Serial.println("Air mode successfully set");}
  pinMode(onlight, OUTPUT);
  ss.begin(XBEE_BAUD);
  xBee.init('A');
  Serial.println("Xbee initialized");
  pinMode(10,OUTPUT);
  Serial.print("Initializing SD card...");
  if(!SD.begin(chipSelect))
    Serial.println("Card failed, or not present");
  else {
    Serial.println("Card initialized.\nCreating File...");
    for (byte i = 0; i < 100; i++) {
      filename[6] = '0' + i/10;
      filename[7] = '0' + i%10;
      if (!SD.exists(filename)) {
        datalog = SD.open(filename, FILE_WRITE);
        SDactive = true;
        Serial.println("Logging to: " + String(filename));
        break;
      }
    }
    if (!SDactive) Serial.println("No available file names; clear SD card to enable logging");
  }
  digitalWrite(onlight, LOW);

  String header = "GPS Time,Lat,Lon,Alt (m), temp(C), pressure(PSI), released, seconds since bootup ";
  //String header = "GPS Time,Lat,Lon,Alt (m),# Sats, temp(C), seconds since bootup ";  //old version
  Serial.println(header);
  if(SDactive) {
    datalog = SD.open(filename, FILE_WRITE);
    datalog.println(header);
    datalog.close();
  }

  pinMode(gps_status_pin, OUTPUT);
  pinMode(smart_status_pin, OUTPUT);
  pinMode(sd_status_pin, OUTPUT);
}

void loop() {
  updateXbee();
  logData();
  updateStatus();
  if (!released && ((millis()/60000.0 > cutTime)) || (currentAlt >= cutAlt)) {
     smart.release();
     released = true;
     ss.println("PPOD released on Timer");
     if(SDactive) datalog.println("PPOD released on Timer");
  }
  if (released == true) {digitalWrite(onlight,HIGH);}
  
  delay(500);
  digitalWrite(onlight,LOW);
  updateStatus();
  delay(500);
}

void updateXbee(){
if (ss.available() > 0) {
    command = ss.readString();
    if (command == "timeleft") {
      ss.print(",,,,,,,,,,,,PPOD ," + String(cutTime - millis()/60000));
    }
    else if (command.startsWith("+")) {
      command.remove(0,1);
      float addtime = command.toFloat(); 
      cutTime = cutTime + addtime;
      ss.print(",,,,,,,,,,,,PPOD: " + String(addtime) + " Minutes added." + " New Time Left: ," + String(cutTime - millis()/60000));
      if(SDactive) datalog.println(",,,,,,,,,,,,PPOD: " + String(addtime) + " Minutes added." + " New Time Left: ," + String(cutTime - millis()/60000));
    }
    else if (command.startsWith("-")) {
      command.remove(0,1);
      float subtime = command.toFloat();
      cutTime = cutTime - subtime;
      ss.print(",,,,,,,,,,,,PPOD " + String(subtime) + " Minutes removed." + " New Time Left: ," + String(cutTime - millis()/60000));
      if(SDactive) datalog.println(",,,,,,,,,,,,PPOD " + String(subtime) + " Minutes removed." + " New Time Left: ," + String(cutTime - millis()/60000));
    }
    else if (command.startsWith("add")) {
      command.remove(0,3);
      float addalt = command.toFloat();
      cutAlt = cutAlt + addalt;
      ss.print(",,,,,,,,,,,,PPOD " + String(addalt) + " Meters added. New distance left: ,," + String(cutAlt - currentAlt));
      if(SDactive) datalog.println(",,,,,,,,,,,,PPOD " + String(addalt) + " Meters added. New distance left: ,," + String(cutAlt - currentAlt));
    }
    else if (command.startsWith("sub")) {
      command.remove(0,3);
      float subalt = command.toFloat();
      cutAlt = cutAlt - subalt;
      ss.print(",,,,,,,,,,,,PPOD " + String(subalt) + " Meters lost. New distance left: ,," + String(cutAlt - currentAlt));
      if(SDactive) datalog.println(",,,,,,,,,,,,PPOD " + String(subalt) + " Meters lost. New distance left: ,," + String(cutAlt - currentAlt));
    }
    else if (command == "distleft") {
      ss.print(",,,,,,,,,,,,PPOD ,," + String(cutAlt-currentAlt));
    }
    else if (command.startsWith("ALT")) {
      command.remove(0,3);
      currentAlt = command.toFloat(); 
    }
    else if (command == "cut") {
      if (released == true) {
        ss.print(",,,,,,,,,,,,PPOD: Already Released");
      }
      if (released == false) {
        smart.release();
        released = true;
        ss.print(",,,,,,,,,,,,PPOD: Released");
        if(SDactive) datalog.println(",,,,,,,,,,,,PPOD: Released via command");
      }
    }
    else if (command == "PPOD") {
      if (released == false) {
      ss.print(",PPOD:, " + String(cutTime - millis()/60000) + "," + String(cutAlt - currentAlt));
      }
      else if (released == true) {
        ss.print(",PPOD:, Released");
      }
    }}}

float temperature(){
  float rawTemp = analogRead(temp_pin);
  float tempV = rawTemp*(5.0/1024.0);
  float temp = ((tempV-0.5)*100);
  return temp;
}

float pressure(){
  float rawPress = analogRead(press_pin);
  float pressV = rawPress*(5.0/1024);
  float pressure = ((pressV - 0.5)*(15.0/4.0));
  return pressure;
}

void logData(){

  short seconds = (millis()/1000)%60;
  short minutes = (millis()/60000)%60;
  short hours = (millis()/1000)/3600;
  
  String data = String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second()) + ", "
                + String(gps.location.lat(), 4) + ", " + String(gps.location.lng(), 4) + ", " + String(gps.altitude.meters(), 4) +  ", " 
                + String(temperature()) + ", " + String(pressure()) + ", " + String(released) + ", " + String(hours) + ":" + String(minutes) + ":"
                + String(seconds);
              
  /*if (gps.getFixAge() > 2000){
    fix = false;
    data += ", no fix";}
  else{
    fix = true;
    data+= ", fix";}
    */
  Serial.println(data);
  if(SDactive){
    datalog = SD.open(filename, FILE_WRITE);
    datalog.println(data);
    datalog.close();
  }
}

void updateStatus()
{
  if(status_engaged)
  {
    digitalWrite(gps_status_pin, LOW);
    digitalWrite(smart_status_pin, LOW);
    digitalWrite(sd_status_pin, LOW);
  }
  else
  {
    status_counter ++;
    if(status_counter > 10) status_counter = 1;
    if((fix && status_counter==10) || !fix) digitalWrite(gps_status_pin, HIGH);
    if(released && status_counter==10) digitalWrite(smart_status_pin, HIGH);
    if((SDactive && status_counter==10) || !SDactive) digitalWrite(sd_status_pin, HIGH);
  }
  status_engaged = !status_engaged;
}
