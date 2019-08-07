#include <UbloxGPS.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <RelayXBee.h>
#include <SD.h>
#include <SPI.h>
#include <Smart.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//#define chipSelect 53 //use this for Mega board
#define smartPin 2
#define onlight 7
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
bool released = false;
String ID = "POD";
String command;
float cutTime = 70.0; //Time in minutes is this right??? 70,000 feet approx.
float cutAlt = 18288; //Alt in meters
float currentAlt = 0;
String rawGPS;
short status_counter = 0;
bool status_engaged = false;
unsigned long int radioTime = 0;
const int chipSelect = BUILTIN_SDCARD; //for Teensy only

//SoftwareSerial XBee_serial = SoftwareSerial(6,7);
//SoftwareSerial XBee_serialGPS = SoftwareSerial(2,3);
RelayXBee xBee = RelayXBee(&XBee_serial, ID);
UbloxGPS ublox(&GPS_serial);
Smart smart = Smart(smartPin);
OneWire oneWire(temp_pin);
DallasTemperature digital_temp(&oneWire);

void setup() {
  pinMode(gps_status_pin, OUTPUT);
  pinMode(smart_status_pin, OUTPUT);
  pinMode(sd_status_pin, OUTPUT);
  Serial.begin(9600);
  smart.initialize();
  digital_temp.begin();
  pinMode(onlight, OUTPUT);
  XBee_serial.begin(XBEE_BAUD);
  xBee.init('A');
  Serial.println("Xbee initialized");
  pinMode(10,OUTPUT);
  GPS_serial.begin(UBLOX_BAUD);
  ublox.init();
  delay(200);
  Serial.println("GPS initialized");
  byte i = 0;
  while (i<50) {
    i++;
    if (ublox.setAirborne()) {
      Serial.println("Air mode successfully set.");
      break;}}
  Serial.println("GPS configured");
  Serial.print("Initializing SD card...");
  pinMode(chipSelect,OUTPUT);
  if(!SD.begin(chipSelect)){
    Serial.println("Card failed, or not present");
    while(millis()<30000)
    {
    digitalWrite(sd_status_pin,HIGH);
    delay(200);
    digitalWrite(sd_status_pin,LOW);
    delay(200);
    }}
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

  String header = "GPS Time,Lat,Lon,Alt (m), temp(C), pressure(PSI), released? , time since bootup ";
  //String header = "GPS Time,Lat,Lon,Alt (m),# Sats, temp(C), seconds since bootup ";  //old version
  Serial.println(header);
  if(SDactive) {
    datalog = SD.open(filename, FILE_WRITE);
    datalog.println(header);
    datalog.close();
  }
}

void loop() {
  ublox.update();
  if (millis()%1000 == 500) {
    updateXbee();
    logData();
    updateStatus();
    if (!released && ((millis()/60000.0 > cutTime) || (currentAlt >= cutAlt))) {
       smart.release();
       released = true;
       XBee_serial.println("PPOD released on Timer");
       if(SDactive) datalog.println("PPOD released on Timer");
    }
    if (released == true) {digitalWrite(onlight,HIGH);}
  }
  if (millis()%1000 == 0) {
    digitalWrite(onlight,LOW);
    updateStatus();}
}

void updateXbee(){
if (XBee_serial.available() > 0) {
    command = xBee.receive();
    if (command.startsWith("T")) {
      XBee_serial.print(",,,,,,,,,,,,PPOD ," + String(cutTime - millis()/60000));
    }
    else if (command.startsWith("+")) {
      command.remove(0,1);
      float addtime = command.toFloat(); 
      cutTime = cutTime + addtime;
      XBee_serial.print(",,,,,,,,,,,,PPOD: " + String(addtime) + " Minutes added." + " New Time Left: ," + String(cutTime - millis()/60000));
      if(SDactive) datalog.println(",,,,,,,,,,,,PPOD: " + String(addtime) + " Minutes added." + " New Time Left: ," + String(cutTime - millis()/60000));
    }
    else if (command.startsWith("-")) {
      command.remove(0,1);
      float subtime = command.toFloat();
      cutTime = cutTime - subtime;
      XBee_serial.print(",,,,,,,,,,,,PPOD " + String(subtime) + " Minutes removed." + " New Time Left: ," + String(cutTime - millis()/60000));
      if(SDactive) datalog.println(",,,,,,,,,,,,PPOD " + String(subtime) + " Minutes removed." + " New Time Left: ," + String(cutTime - millis()/60000));
    }
    else if (command.startsWith("C")) {
      if (released == true) {
        XBee_serial.print(",,,,,,,,,,,,PPOD: Already Released");
      }
      if (released == false) {
        smart.release();
        released = true;
        XBee_serial.print(",,,,,,,,,,,,PPOD: Released");
        if(SDactive) datalog.println(",,,,,,,,,,,,PPOD: Released via command");
      }
    }
    else if (command.startsWith("A")) {
      if (released == false) {
      XBee_serial.print(data);//",PPOD:, " + String(cutTime - millis()/60000) + "," + String(cutAlt - currentAlt));
      }
      else if (released == true) {
        XBee_serial.print(",PPOD:, Released");
      }
    }}}

float temperature(){
  digital_temp.requestTemperatures();
  float temp = digital_temp.getTempCByIndex(0);
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
  
  data = String(ublox.getHour()-5) + ":" + String(ublox.getMinute()) + ":" + String(ublox.getSecond()) + ", "
                + String(ublox.getLat(), 4) + ", " + String(ublox.getLon(), 4) + ", " + String(ublox.getAlt_feet(), 4) +  ", "
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
  if(radioTime + 10000 < millis()){
    xBee.send(data);
    radioTime = millis();}
  
  if(SDactive){
    datalog = SD.open(filename, FILE_WRITE);
    datalog.println(data);
    datalog.close();
    digitalWrite(sd_status_pin, HIGH);
  }
}

void updateStatus()
{
  if(status_engaged)
  {
    digitalWrite(gps_status_pin, LOW);
    digitalWrite(smart_status_pin, LOW);
    if(SDactive)digitalWrite(sd_status_pin, LOW);
  }
  else
  {
    status_counter ++;
    if(status_counter > 10) status_counter = 1;
    if((fix && status_counter==10) || !fix) digitalWrite(gps_status_pin, HIGH);
    if((!released && status_counter==10) || released) digitalWrite(smart_status_pin, HIGH);
    if(!SDactive) digitalWrite(sd_status_pin, HIGH);
  }
  status_engaged = !status_engaged;
}
