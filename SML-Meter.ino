
/*
 *  Application note: Read a Holley DTZ541 electricity meter via RS485 interface and SML protocol
 *  Version 1.0
 *  Copyright (C) 2020  Hartmut Wendt  www.zihatec.de
 *  
 *  used hardware: Arduino RS485 shield  https://www.hwhardsoft.de/english/projects/rs485-arduino/
 *  
 *  credits: user "rollercontainer"
 *           at https://www.photovoltaikforum.com/volkszaehler-org-f131/sml-protokoll-hilfe-gesucht-sml-gt-esp8266-gt-mqtt-t112216-s10.html
 *           
 *  more information about SML protocol:         
 *           http://www.schatenseite.de/2016/05/30/smart-message-language-stromzahler-auslesen/ 
 *           https://wiki.volkszaehler.org/software/sml
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/   


#include <SoftwareSerial.h>

byte inByte; //byte to store the serial buffer
byte smlMessage[1000]; //byte to store the parsed message
const byte startSequence[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 }; //start sequence of SML protocol
const byte stopSequence[]  = { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A }; //end sequence of SML protocol
const byte powerSequence[] =       { 0x77, 0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xFF }; //sequence preceeding the current "Real Power" value (2 Bytes)
const byte consumptionSequence[] = { 0x77, 0x07, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF }; //sequence predeecing the current "Total power consumption" value (4 Bytes)
int smlIndex; //index counter within smlMessage array
int startIndex; //start index for start sequence search
int stopIndex; //start index for stop sequence search
int stage; //index to maneuver through cases
byte power[8]; //array that holds the extracted 4 byte "Wirkleistung" value
byte consumption[8]; //array that holds the extracted 4 byte "Gesamtverbrauch" value
unsigned long currentpower; //variable to hold translated "Wirkleistung" value
unsigned long currentconsumption; //variable to hold translated "Gesamtverbrauch" value
float currentconsumptionkWh; //variable to calulate actual "Gesamtverbrauch" in kWh

SoftwareSerial MeterSerial(2, 3); // RX, TX

// #define _debug_msg

void setup() {
// bring up serial ports
  Serial.begin(115200); 	// debug via USB 
  MeterSerial.begin(9600); 	// meter via RS485

  Serial.println("Waiting for data");
}

void loop() {
  switch (stage) {
    case 0:
      findStartSequence(); // look for start sequence
      break;
    case 1:
      findStopSequence(); // look for stop sequence
      break;
    case 2:
      findPowerSequence(); //look for power sequence and extract
      break;
    case 3:
      findConsumptionSequence(); //look for consumption sequence and exctract
      break;
    case 4:
      publishMessage(); // do something with the result
      break;   
  }
}


void findStartSequence() {
  while (MeterSerial.available())
  {
    inByte = MeterSerial.read(); //read serial buffer into array
    
    if (inByte == startSequence[startIndex]) //in case byte in array matches the start sequence at position 0,1,2...
    {
      smlMessage[startIndex] = inByte; //set smlMessage element at position 0,1,2 to inByte value
      startIndex++;
      if (startIndex == sizeof(startSequence)) //all start sequence values have been identified
      {
        #ifdef _debug_msg
        Serial.println("Match found - Start Sequence");
        #endif
        stage = 1; //go to next case
        smlIndex = startIndex; //set start index to last position to avoid rerunning the first numbers in end sequence search
        startIndex = 0;
      }
    }
    else {
      startIndex = 0;
    }
  }
}



void findStopSequence() {
  while (MeterSerial.available())
  {
    inByte = MeterSerial.read();
    smlMessage[smlIndex] = inByte;
    smlIndex++;

    if (inByte == stopSequence[stopIndex])
    {
      stopIndex++;
      if (stopIndex == sizeof(stopSequence))
      {
        #ifdef _debug_msg
        Serial.println("Match found - Stop Sequence");
        #endif
        stage = 2;
        stopIndex = 0;
      }
    }
    else {
      stopIndex = 0;
    }
  }
}

void findPowerSequence() {
  byte temp; //temp variable to store loop search data
 startIndex = 0; //start at position 0 of exctracted SML message
 
for(int x = 0; x < sizeof(smlMessage); x++){ //for as long there are element in the exctracted SML message
    temp = smlMessage[x]; //set temp variable to 0,1,2 element in extracted SML message
    if (temp == powerSequence[startIndex]) //compare with power sequence
    {
      startIndex++;
      if (startIndex == sizeof(powerSequence)) //in complete sequence is found
      {
        for(int y = 0; y< 3; y++){ //read the next 2 bytes (the actual power value)
          power[y] = smlMessage[x+y+8]; //store into power array
          #ifdef _debug_msg
          Serial.print(String(power[y], HEX));
          Serial.print(" ");
          #endif        
        }
        #ifdef _debug_msg
        Serial.println();
        #endif
        stage = 3; // go to stage 3
        startIndex = 0;
      }
    }
    else {
      startIndex = 0;
    }
  }
   currentpower = (power[0] << 8 | power[1] << 0); //merge 2 bytes into single variable to calculate power value
}


void findConsumptionSequence() {
  byte temp;
 
  startIndex = 0;
for(int x = 0; x < sizeof(smlMessage); x++){
    temp = smlMessage[x];
    if (temp == consumptionSequence[startIndex])
    {
      startIndex++;
      if (startIndex == sizeof(consumptionSequence))
      {
        for(int y = 0; y < 4; y++){
          //hier muss für die folgenden 4 Bytes hoch gezählt werden
          consumption[y] = smlMessage[x+y+16];
          #ifdef _debug_msg
          Serial.print(String(consumption[y], HEX));
          Serial.print(" ");
          #endif
        }
        #ifdef _debug_msg
        Serial.println();
        #endif
        stage = 4;
        startIndex = 0;
      }
    }
    else {
      startIndex = 0;
    }
  }
 
   currentconsumption = consumption[0];
   currentconsumption <<= 8;
   currentconsumption += consumption[1];
   currentconsumption <<= 8;
   currentconsumption += consumption[2];
   currentconsumption <<= 8;
   currentconsumption += consumption[3];

   currentconsumptionkWh = (float)currentconsumption / 10000; // 10.000 impulses per kWh
}


void publishMessage() {
//Wirkleistung
Serial.print("Real Power: ");
Serial.print(currentpower);
Serial.println(" W");
// Gesamtverbrauch
Serial.print("Total Consumption: ");
Serial.print(currentconsumptionkWh);
Serial.println(" kWh");

// clear the buffers
  memset(smlMessage, 0, sizeof(smlMessage));
  memset(power, 0, sizeof(power));
  memset(consumption, 0, sizeof(consumption));
//reset case
  smlIndex = 0;
  stage = 0; // start over
}
