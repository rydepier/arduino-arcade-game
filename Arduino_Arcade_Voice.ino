/************************************************************/
/* Talking sketch for Arduino Amusement Arcade using WTD588D
  
 
 
WTD588D::
Upload the audio to this device before connecting into the circuit

Arduino pin 3 to Module pin 7 RESET
Arduino pin 4 to Module pin 21 BUSY
Arduino pin 5 to Module pin 18 PO1
Arduino pin 6 to Module pin 17 PO2
Arduino pin 7 to Module pin 16 PO3
Arduino Gnd to Module pin 14
Arduino 5v to Module pin 20
Speaker pins 9 and 10

Chris Rouse
Aprik 2016

*/
/************************************************************/


#include "WT588D.h"  // audio module
#include <SPI.h>
#include <Wire.h> // I2C
#include <SoftwareSerial.h>


// set the correct pin connections for the WT588D chip
#define WT588D_RST 3  //Module pin "REST"
#define WT588D_CS 6   //Module pin "P02"
#define WT588D_SCL 7  //Module pin "P03"
#define WT588D_SDA 5  //Module pin "P01"
#define WT588D_BUSY 4 //Module pin "LED/BUSY" 
WT588D myWT588D(WT588D_RST, WT588D_CS, WT588D_SCL, WT588D_SDA, WT588D_BUSY);
#define talkPin 2 // pin used to request time
#define ledPin 13 // onboard LED

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete
boolean stringValid = false;     // used to show a valid speak command
/************************************************************/

void setup() {
  Serial.begin(9600);
  // initialize the chip and port mappiing
  myWT588D.begin();
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // turn off onboard LED
  //
  // reserve 200 bytes for the inputString:
  inputString.reserve(200);
  
}
/************************************************************/
void loop(){

  serialEvent(); //call the function
  // data is being sent from the TTF Nano constantly
  // speak phrases are prefixed by a # symbol
  // this routine reads the data comming over the Serial port
  // and filters out the speak comands
  // flag stringValid is true if its a voice command
  if (stringComplete) {
    Serial.println(inputString);
    if(stringValid){
      digitalWrite(ledPin, HIGH);
      speakPhrase(inputString.toInt());
      Serial.println("$"); // show WTD588D finished
      digitalWrite(ledPin, LOW); 
    }
    // clear the string:
    inputString = "";
    stringComplete = false;
    stringValid = false;
  }
}
/*--------------------------------------------------------*/
void speakPhrase(int phrase) {
  myWT588D.playSound(phrase);
  delay(100);
  while (myWT588D.isBusy() ) {
  }
}
//
/*-----------------------------------------------------*/
void serialEvent() {
  
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    if (inChar == char(35)) stringValid = true;
    else inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}


/*-----------------------------------------------------*/
