#include <UbloxGPS.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <RelayXBee.h>
#include "src\Smart.h"

#define smartPin 10
#define onlight 7
#define temp_pin A0 //The light that turns on when it is powered up and remains on

bool fix = false;
bool released = false;
String ID = "POD";
String command;
float cutTime = 70; //Time in minutes is this right??? 70,000 feet approx.
float cutAlt = 18288; //Alt in meters
float currentAlt = 0;
String rawGPS;

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
  digitalWrite(onlight, LOW);

  String header = "GPS Time,Lat,Lon,Alt (m),# Sats, temp(C), seconds since bootup ";
  Serial.println(header);
}

void loop() {
  updateXbee();
  logData();
  if (!released && ((millis()/60000.0 > cutTime)) || (currentAlt >= cutAlt)) {
     smart.release();
     released = true;
     ss.println("PPOD released on Timer");
  }
  if (released == true) {digitalWrite(onlight,HIGH);}
  
  delay(500);
  digitalWrite(onlight,LOW);
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
    }
    else if (command.startsWith("-")) {
      command.remove(0,1);
      float subtime = command.toFloat();
      cutTime = cutTime - subtime;
      ss.print(",,,,,,,,,,,,PPOD " + String(subtime) + " Minutes removed." + " New Time Left: ," + String(cutTime - millis()/60000));
    }
    else if (command.startsWith("add")) {
      command.remove(0,3);
      float addalt = command.toFloat();
      cutAlt = cutAlt + addalt;
      ss.print(",,,,,,,,,,,,PPOD " + String(addalt) + " Meters added. New distance left: ,," + String(cutAlt - currentAlt));
    }
    else if (command.startsWith("sub")) {
      command.remove(0,3);
      float subalt = command.toFloat();
      cutAlt = cutAlt - subalt;
      ss.print(",,,,,,,,,,,,PPOD " + String(subalt) + " Meters lost. New distance left: ,," + String(cutAlt - currentAlt));
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

double temperature(){
  double rawTemp = analogRead(temp_pin);
  double tempV = rawTemp*(5.0/1024.0);
  double temp = ((tempV-0.5)*100);
  return temp;
}

void logData(){

  int seconds = (millis()/1000)%60;
  int minutes = (millis()/60000)%60;
  int hours = (millis()/1000)/3600;
  
  String data = String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second()) + ", "
                + String(gps.location.lat(), 4) + ", " + String(gps.location.lng(), 4) + ", " + String(gps.altitude.meters(), 4) +  ", " 
                + String(temperature()) + ", " + String(released) + ", " + String(hours) + ":" + String(minutes) + ":"
                + String(seconds);

  /*if (gps.getFixAge() > 2000){
    fix = false;
    data += ", no fix";}
  else{
    fix = true;
    data+= ", fix";}
    */
  Serial.println(data);
}
