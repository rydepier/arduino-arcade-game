/***************************************************************
 * Arcade MatchMaker using Touch Screen TTF Display
 * by Chris Rouse
 * April 2016
 * 
 *
 * Uses Adafruit ttf libraries
***************************************************************/
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <SD.h>
#include <SPI.h>
#include <PinChangeInt.h>
//
#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0
#define LCD_RESET A4

#define PIN 19  // the pin the capSwitch is connected to 

boolean lightLed = true;
boolean gotACK = false;
String ackPhrase = "";
boolean capSwitch = false; // has touch switch been pressed
int y; // led position
int x; // led position
int z; // used to map results from 1 to 8 to 1 to 4
int matchValue;
int ledx;
int ledy;
int pointer = 1;
int results[6]; // stores values
int skipLine; // used if one of the results is two lines long
//
// Set the chip select line to whatever you use (10 doesnt conflict with the library)
// In the SD card, place 24 bit color BMP files)
#define SD_CS 10     
//
Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, A4);
//


/*****************************************************************/
void setup(){
  Serial.begin(9600);
  tft.reset();
  uint16_t identifier = tft.readID();
  tft.begin(identifier);
//
// Now initialise the SD Card
  if (!SD.begin(SD_CS)) {
    Serial.println("#1"); // SD card not ready
    while(true ==1){
    }
    }
  tft.setRotation(1); // puts the display in portrait rotation

  //
  pinMode(13, OUTPUT); // onboar LED lights when screen touched
  //
  tft.setTextSize(2);
  randomSeed(analogRead(0)); // set the random seed
  //
  // setup interupt
  pinMode(PIN, INPUT);     //set the pin to input
  digitalWrite(PIN, HIGH); //use the internal pullup resistor
  PCintPort::attachInterrupt(PIN, touchSwitch,RISING); // attach a PinChange Interrupt to our pin on the rising edge
  bmpDraw("clear.bmp", 0, 0); // draw 
  Serial.println("#0");
//
/*****************************************************************/
}
void loop(){
  pointer = 1; // reset pointer
  lightLed = true; // ensure leds light first time
  openDisplay(); // opening display and wait for switch to be touched
  if(capSwitch){
    playGame();
    capSwitch = false;
  }
}
  
/*****************************************************************/
// This function opens a Windows Bitmap (BMP) file 
// this is the Adafruit code
#define BUFFPIXEL 20

void bmpDraw(char *filename, int x, int y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel in buffer (R+G+B per pixel)
  uint16_t lcdbuffer[BUFFPIXEL];  // pixel out buffer (16-bit per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();
  uint8_t  lcdidx = 0;
  boolean  first = true;

  if((x >= tft.width()) || (y >= tft.height())) return;
  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.println(F("File not found"));
    return;
  }
  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.println(F("File size: ")); 
    Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed
        goodBmp = true; // Supported BMP format -- proceed!
        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;
        // If bmpHeight is negative, image is in top-down order.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip = false;
        }
        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;
        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x+w-1, y+h-1);
        for (row=0; row<h; row++) { // For each scanline...
          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }
          for (col=0; col<w; col++) { // For each column...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              // Push LCD buffer to the display first
              if(lcdidx > 0) {
                tft.pushColors(lcdbuffer, lcdidx, first);
                lcdidx = 0;
                first  = false;
              }
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }
            // Convert pixel from BMP to TFT format
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            lcdbuffer[lcdidx++] = tft.color565(r,g,b);
          } // end pixel
        } // end scanline
        // Write any remaining data to LCD
        if(lcdidx > 0) {
          tft.pushColors(lcdbuffer, lcdidx, first);
        } 

      } // end goodBmp
    }
  }
  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
}

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
/*****************************************************************/

void openDisplay(){
  // wait here until the sensor is pressed
  // load the background graphics
  if(capSwitch == false){ // this allows this long loop to be quit quickly
    bmpDraw("scrn1.bmp", 0, 0); // draw the base graphic
    speakPhrase("#3"); // let me find your perfect match
    delay(2000); // wait for a short while, then load second screen
  }
  if(capSwitch == false){  
    bmpDraw("scrn5.bmp", 0, 0);
    delay(2000);
  }
  if(capSwitch == false){    
    bmpDraw("inst.bmp", 0, 0);
    speakPhrase("#23");    
    delay(2000);
  }
  if(capSwitch == false){ 
    bmpDraw("words2.bmp", 0, 0); // draw the
    speakPhrase("#4"); // have I got a match for you
    delay(2000);
  }
  if(capSwitch == false){ 
    bmpDraw("scrn2.bmp", 0, 0); // draw the base graphic
  }
  // draw leds around the border
    for(int a = 0; a<4; a++){
      ledx = 0;
      ledy = 0;
      for(int f =1; f<19; f++){
        if(capSwitch == false){ 
          if (lightLed) bmpDraw("led2.bmp", ledx, ledy);
          else bmpDraw("led3.bmp", ledx, ledy);
          ledx += 18;
        }
      }  
      ledx = 310; // start of new row
      ledy = 20; 
      for(int f =1; f<12; f++){
       if(capSwitch == false){ 
         if (lightLed) bmpDraw("led2.bmp", ledx, ledy);
          else bmpDraw("led3.bmp", ledx, ledy);
          ledy += 19; 
       }   
      }
      for(int f =1; f<19; f++){
        if(capSwitch == false){ 
         if (lightLed) bmpDraw("led2.bmp", ledx, ledy);
          else bmpDraw("led3.bmp", ledx, ledy);
          ledx -= 18; 
        } 
      }
      ledx = 0; // start of new row
      ledy = ledy - 19;
      for(int f =1; f<12; f++){
        if(capSwitch == false){ 
         if (lightLed) bmpDraw("led2.bmp", ledx, ledy);
          else bmpDraw("led3.bmp", ledx, ledy);
          ledy -= 19;
        }
      } 
      lightLed = !lightLed;   
   }          
}
/*****************************************************************/
void generateRandom(){
  // column 1
  matchValue = random(1,9);
  matchValue = random(1,9);
  y = 200; // set first led position  
  for (int f = 0; f<matchValue;f++){
   bmpDraw("led.bmp",68,y);
   speakPhrase("#7"); // tone
   delay(500);
   y -= 20; // next led position
  }
  results[pointer] = matchValue; // save results
  pointer ++; // increment pointer
  // column 2
  matchValue = random(1,9);  
  matchValue = random(1,9);
  y = 200;
  for (int f = 0; f<matchValue;f++){
   bmpDraw("led.bmp",140,y);
   speakPhrase("#7"); // tone
   delay(500);
   y -= 20; // next led position
  }
  results[pointer] = matchValue; // save results
  pointer ++; // increment pointer
  // column 3
  matchValue = random(1,9);  
  matchValue = random(1,9);
  y = 200;
  for (int f = 0; f<matchValue;f++){
   bmpDraw("led.bmp",212,y);
   speakPhrase("#7"); // tone
   delay(500);
   y -= 20; // next led position
  }
  results[pointer] = matchValue; // save results
  pointer ++; // increment pointer
}
/*****************************************************************/
void addResults(){
  // put the values on the final result screen
  bmpDraw("words4.bmp", 0, 0); // your result page 
  // personality
  speakPhrase("#8"); // // your perfect match for personality 
  z = map(results[1],1,8,1,4);
  switch (z) {
    case 1:
      speakPhrase("#12");
    break;
    case 2:
      speakPhrase("#11");
    break;
    case 3:
      speakPhrase("#10");
    break;
    case 4:
      speakPhrase("#9");
    break;
  } 
  //
  // wealth
  delay(500);
  speakPhrase("#13");
  z = map(results[2],1,8,1,4);
  switch (z) {
    case 1:
      speakPhrase("#17");
    break;
    case 2:
      speakPhrase("#16");
    break;
    case 3:
      speakPhrase("#15");
    break;
    case 4:
      speakPhrase("#14");
    break;
  } 
  // looks
  delay(500);
  speakPhrase("#18"); 
  z = map(results[3],1,8,1,4);
  switch (z) {
    case 1:
      speakPhrase("#22");
    break;
    case 2:
      speakPhrase("#21");
    break;
    case 3:
      speakPhrase("#20");
    break;
    case 4:
      speakPhrase("#19");
    break;
  } 
  //
  delay(1000);
  //
  // screen 2 of results
  bmpDraw("words6.bmp", 0, 0); // your result page 
  // shape
  speakPhrase("#24"); // // your perfect athletic shape 
  z = map(results[4],1,8,1,4);
  switch (z) {
    case 1:
      speakPhrase("#28");
    break;
    case 2:
      speakPhrase("#27");
    break;
    case 3:
      speakPhrase("#26");
    break;
    case 4:
      speakPhrase("#25");
    break;
  } 
  // brain
  z = map(results[5],1,8,1,4);
  switch (z) {
    case 1:
      speakPhrase("#37");
    break;
    case 2:
      speakPhrase("#31");
    break;
    case 3:
      speakPhrase("#30");
    break;
    case 4:
      speakPhrase("#29");
    break;
  }   
  // height
  z = map(results[6],1,8,1,4);
  switch (z) {
    case 1:
      speakPhrase("#35");
    break;
    case 2:
      speakPhrase("#34");
    break;
    case 3:
      speakPhrase("#33");
    break;
    case 4:
      speakPhrase("#32");
    break;
  }   
  //
  delay(3000);
  // final words of wisdom
  bmpDraw("words5.bmp", 0, 0); // be careful with friends 
  speakPhrase("#36"); // changing friends
  delay(5000);
}
/*****************************************************************/
void touchSwitch(){
  capSwitch = true; // show switch touched
}
/*****************************************************************/
void playGame(){
  // touch switch pressed
  // we are here if the switch was touched
  bmpDraw("blank.bmp", 0, 0); // clear the screen
  delay(3000);
  bmpDraw("words3.bmp", 0, 0);
  Serial.println("#5"); //  i can read you like a book
  delay(4000); 
  bmpDraw("scrn3.bmp", 0, 0); // first result page
  generateRandom();
  delay(5000);
  bmpDraw("scrn4.bmp", 0, 0); // first result page 
  generateRandom();  
  delay(5000);
  //
  addResults(); // add the results
  bmpDraw("blank.bmp", 0, 0); // clear the screen
  delay(3000);  
}

/*****************************************************************/

void waitForACK() {
  while (Serial.available() > 0) {
    // get incoming byte:
    char inChar = (char)Serial.read();
    if (inChar == char(36)){ // found ack
     gotACK = true;
    }
  }
}

void speakPhrase(String talk){
  // send talk command and wait for reply from the WTD588D
  Serial.flush();
  ackPhrase = "";
  Serial.println(talk);
  // now wait for a reply
  delay(100);
  while(gotACK == false){
    waitForACK();
  }
  gotACK = false;
}

/*****************************************************************/

