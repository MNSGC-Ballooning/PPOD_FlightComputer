#include <UbloxGPS.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <RelayXBee.h>
#include <SD.h>
#include <SPI.h>
#include "src/Smart.h"
#include <OneWire.h>
#include <DallasTemperature.h>

//#define chipSelect 53 //use this for Mega board
#define smartPin 3
#define temp_pin 4
#define press_pin A1
#define gps_status_pin A2 //blue, flashes every 10 seconds if fix, every second if not
#define smart_status_pin A3 //red, flashes every 10 seconds if not released, every second after release
#define sd_status_pin A4 //yellow, flashes every log cycle if logging, stays on if not logging
#define GPS_serial Serial1
#define XBee_serial Serial3

File datalog;
char filename[] = "SDPPOD00.csv"; //File name template
bool SDactive = false;

String data;
bool fix = false;
bool attempt = false;
bool released = false;
String ID = "POD";
String command;
float cutTime = 70.0; //Time in minutes, 70,000 feet approx.
float cutAlt = 18288; //Alt in meters
float currentAlt = 0;
String rawGPS;
short status_counter = 0;
unsigned long int radioTime = 0;
const int chipSelect = BUILTIN_SDCARD; //for Teensy only

RelayXBee xBee = RelayXBee(&XBee_serial, ID);
UbloxGPS ublox(&GPS_serial);
Smart smart = Smart(smartPin);
OneWire oneWire(temp_pin); //For Dallas sensor
DallasTemperature digital_temp(&oneWire); //For Dallas sensor

void setup() {
  pinMode(gps_status_pin, OUTPUT);
  pinMode(smart_status_pin, OUTPUT);
  pinMode(sd_status_pin, OUTPUT);
  Serial.begin(9600);
  smart.initialize();
  digital_temp.begin();
  pinMode(10, INPUT_PULLUP);
  XBee_serial.begin(XBEE_BAUD);
  xBee.init('A');
  Serial.println("Xbee initialized");
  GPS_serial.begin(UBLOX_BAUD);
  ublox.init();
  delay(200);
  Serial.println("GPS initialized");
  byte i = 0;
  while (i<50) {
    i++;
    if (ublox.setAirborne()) {
      Serial.println("Air mode successfully set.");
      break;}
    if (i==50) Serial.println("Failed to set to air mode.");
  }
  Serial.println("GPS configured");
  Serial.print("Initializing SD card...");
  pinMode(chipSelect,OUTPUT);
  if(!SD.begin(chipSelect)){
    Serial.println("Card failed, or not present");
    while(millis()<30000) {
    digitalWrite(sd_status_pin,HIGH);
    delay(200);
    digitalWrite(sd_status_pin,LOW);
    delay(200);}
  }
  else {
    Serial.println("Card initialized.\nCreating File...");
    for (byte i = 0; i < 100; i++) {
      filename[6] = '0' + i/10;
      filename[7] = '0' + i%10;
      if (!SD.exists(filename)) {
        datalog = SD.open(filename, FILE_WRITE);
        SDactive = true;
        Serial.println("Logging to: " + String(filename));
        break;}
    }
    if (!SDactive) Serial.println("No available file names; clear SD card to enable logging");
  }
  String header = "GPS Time, Lat, Lon, Alt (ft), temp(C), pressure(PSI), attempted?, released?, time since bootup ";
  Serial.println(header);
  if(SDactive) logData(header);
}

void loop() {
  ublox.update();
  if (millis()%1000 == 500) {
    released = digitalRead(10);
    updateXbee();
    updateData();
    updateStatus();
    if (!released) {
      float previousAlt = currentAlt;
      currentAlt = ublox.getAlt_meters();
      if (millis()/60000.0 > cutTime) {
        smart.release();
        attempt = true;
        xBee.send("PPOD attempted release on timer");
        if (SDactive) logData("PPOD attempted release on timer");}
      else if ((currentAlt >= cutAlt) && ((currentAlt-previousAlt) < 500)) {
        smart.release();
        attempt = true;
        xBee.send("PPOD attempted release at altitude " + String(currentAlt) + " m");
        if(SDactive) logData("PPOD attempted release at altitude " + String(currentAlt) + " m");}
    }
    delay(100);
    updateStatus();
  }
}

void logData(String dataa) {
  datalog = SD.open(filename, FILE_WRITE);
  datalog.println(dataa);
  datalog.close();
}

void updateXbee(){
  if (XBee_serial.available() > 0) {
    command = xBee.receive();
    if (command.startsWith("T")) {
      xBee.send(String(cutTime - millis()/60000) + "minutes left");}
    else if (command.startsWith("+")) {
      command.remove(0,1);
      float addtime = command.toFloat(); 
      cutTime = cutTime + addtime;
      xBee.send(String(addtime) + " minutes added." + " New time left: ," + String(cutTime - millis()/60000));
      if(SDactive) logData(String(addtime) + " minutes added." + " New time left: ," + String(cutTime - millis()/60000));}
    else if (command.startsWith("-")) {
      command.remove(0,1);
      float subtime = command.toFloat();
      cutTime = cutTime - subtime;
      xBee.send(String(subtime) + " minutes removed." + " New time left: ," + String(cutTime - millis()/60000));
      if(SDactive) logData(String(subtime) + " minutes removed." + " New time left: ," + String(cutTime - millis()/60000));}
    else if (command.startsWith("A")) {
      xBee.send(String(cutAlt - ublox.getAlt_meters()) + " meters left");}
    else if (command.startsWith("A+")) {
      command.remove(0,2);
      float addalt = command.toFloat(); 
      cutAlt = cutAlt + addalt;
      xBee.send(String(addalt) + " meters added." + " New distance left: ," + String(cutAlt - ublox.getAlt_meters()));
      if(SDactive) logData(String(addalt) + " meters added." + " New distance left: ," + String(cutAlt - ublox.getAlt_meters()));}
    else if (command.startsWith("A-")) {
      command.remove(0,2);
      float subalt = command.toFloat(); 
      cutAlt = cutAlt - subalt;
      xBee.send(String(subalt) + " meters removed." + " New distance left: ," + String(cutAlt - ublox.getAlt_meters()));
      if(SDactive) logData(String(subalt) + " meters removed." + " New distance left: ," + String(cutAlt - ublox.getAlt_meters()));}
    else if (command.startsWith("C")) {
      if (released == true) {
        xBee.send("PPOD already released");}
      if (released == false) {
        smart.release();
        attempt = true;
        xBee.send("PPOD attempted release");
        if(SDactive) logData("PPOD attempted release via command");}}
    else if (command.startsWith("D")) {
       xBee.send(data);}
    else if (command.startsWith("M")) {
      xBee.send("Polo");}
  }
}

float temperature() {
  digital_temp.requestTemperatures();
  float temp = digital_temp.getTempCByIndex(0);
  return temp;
}

float pressure() {
  float rawPress = analogRead(press_pin);
  float pressV = rawPress*(5.0/1024);
  float pressure = ((pressV - 0.5)*(15.0/4.0));
  return pressure;
}

void updateData() {
  short seconds = (millis()/1000)%60;
  short minutes = (millis()/60000)%60;
  short hours = (millis()/1000)/3600;
  
  data = String(ublox.getHour()-5) + ":" + String(ublox.getMinute()) + ":" + String(ublox.getSecond()) + ", "
                + String(ublox.getLat(), 4) + ", " + String(ublox.getLon(), 4) + ", " + String(ublox.getAlt_feet(), 4) +  ", "
                + String(temperature()) + ", " + String(pressure()) + ", " + String(attempt) + ", " + String(released) + ", "
                + String(hours) + ":" + String(minutes) + ":" + String(seconds);
  if(ublox.getFixAge() > 2000) fix = false;
  else fix = true;
  Serial.println(data);
  if(radioTime + 10000 < millis()) {
    xBee.send(data);
    radioTime = millis();} 
  if(SDactive) {
    logData(data);
    digitalWrite(sd_status_pin, HIGH);}
}

void updateStatus() {
  if(status_counter%2 == 1) {
    digitalWrite(gps_status_pin, LOW);
    digitalWrite(smart_status_pin, LOW);
    if(SDactive)digitalWrite(sd_status_pin, LOW);} 
  else {
    if((fix && status_counter==20) || !fix) digitalWrite(gps_status_pin, HIGH);
    if((!released && status_counter==20) || released) digitalWrite(smart_status_pin, HIGH);
    if(!SDactive) digitalWrite(sd_status_pin, HIGH);}
  status_counter ++;
  if(status_counter > 20) status_counter = 1;
}
