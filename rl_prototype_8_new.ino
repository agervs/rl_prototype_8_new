/* Following hardware connections are assumed
*
*   OLED                   Arduino 101
*   ---------------------+------------------------------------------------------------------
*   #1 Vss/GND             GND
*   #2 Vdd                 3V3 (up to 271 mA, use external power supply to feed Arduino 101)
*   #4 D/!C                D9
*   #7 SCLK                D13 (hardware SPI SCLK)
*   #8 SDIN                D11 (hardware SPI MOSI)
*   #16 !RESET             !RESET
*   #17 !CS                D10
*   #5,#6,#10-14,#19,#20   GND
*   #3,#9,#15,#18          not connected
*/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <ESP8266_SSD1322.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
#include <string.h>
#include <Arduino.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"
#include "screens.h"
#include "pitches.h"

#define OLED_CS     10  // Pin 10, CS - Chip select
#define OLED_DC     9   // Pin 9 - DC digital signal
#define OLED_RESET  0   // using hardware !RESET from Arduino instead
#define melodyPin 12

ESP8266_SSD1322 display(OLED_DC, OLED_RESET, OLED_CS);

#if (SSD1322_LCDHEIGHT != 64)
#error("Height incorrect, please fix ESP8266_SSD1322.h!");
#endif

int bookPixel = 1;
int lunchPixel = 2;
int waterPixel = 3;

int redPixelBrightness = 100;
int greenPixelBrightness = 150;

bool water_is_visible = false;
bool water_is_green = false;
bool lunch_is_green = false;
bool book_is_green = false;
bool success_message_triggered = false;

int specialMessageCounter = 0;

bool lunch_status = false; // false = red, true = green
bool book_status = false; // false = red, true = green
bool water_status = false; // false = red, true = green
bool is_connected = false;
int connected_status = 1;
bool icons_visible = false;

int nps_num_of_pixels_in_strip = 5;
int nps_din_pin = 6;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(nps_num_of_pixels_in_strip, nps_din_pin);

int tempo[] = {
  12, 12, 12, 12, 
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12, 
};

int melody[] = {
  NOTE_E7, NOTE_E7, 0, NOTE_E7, 
  0, NOTE_C7, NOTE_E7, 0,
  NOTE_G7, 0, 0,  0,
  NOTE_G6, 0, 0, 0, 
};

#include "BluefruitConfig.h"

#if SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif

    #define FACTORYRESET_ENABLE         0
    #define MINIMUM_FIRMWARE_VERSION    "0.6.6"
    #define MODE_LED_BEHAVIOUR          "MODE"

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

// function prototypes over in packetparser.cpp
uint8_t readPacket(Adafruit_BLE *ble, uint16_t timeout);
float parsefloat(uint8_t *buffer);
void printHex(const uint8_t * data, const uint32_t numBytes);

// the packet buffer
extern uint8_t packetbuffer[];


void setup() {
  pinMode(12, OUTPUT);
  Serial.begin(115200);
  strip.begin();
  set_pixel(0, 0, 0, 0);
  set_pixel(4, 0, 0, 0);

  Wire.begin(); 
  // Initialize and perform reset
  display.begin(true);
  display.clearDisplay();
  display.display();
  // init done

  display.drawBitmap(0, 0,  lunchbookwater, 256, 64, WHITE);
  display.display();
   Serial.println(F("Adafruit Bluefruit App Controller Example"));
  Serial.println(F("-----------------------------------------"));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ){
      error(F("Couldn't factory reset"));
    }
  }


  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in Controller mode"));
  Serial.println(F("Then activate/use the sensors, color picker, game controller, etc!"));
  Serial.println();

  ble.verbose(false);  // debug info is a little annoying after this point!

  /* Wait for connection */
  while (! ble.isConnected()) {
      delay(500);
  }

  Serial.println(F("******************************"));

  // LED Activity command is only supported from 0.6.6
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    // Change Mode LED Activity
    Serial.println(F("Change LED activity to " MODE_LED_BEHAVIOUR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }

  // Set Bluefruit to DATA mode
  Serial.println( F("Switching to DATA mode!") );
  ble.setMode(BLUEFRUIT_MODE_DATA);

  Serial.println(F("******************************"));
}

void loop(void)
{
  /* Wait for new data to arrive */
  uint8_t len = readPacket(&ble, BLE_READPACKET_TIMEOUT);
  if (len == 0) return;

  /* Got a packet! */
  // printHex(packetbuffer, len);


//  tcaselect(3);
//  Display2.setCursor(0, 0);
//  Display2.drawBitmap(64,0,lunch,64,64,WHITE);
//  Display2.display();
//  Display2.clearDisplay();
  
  // Buttons
  if (packetbuffer[1] == 'B') {
    uint8_t buttnum = packetbuffer[2] - '0';
    boolean pressed = packetbuffer[3] - '0';
    Serial.print ("Button "); Serial.print(buttnum);
    if (pressed) {
      button_action(buttnum);
      if(water_is_visible == true){
        if(water_status == true && lunch_status == true && book_status == true){
           trigger_success_message();
        }
      } else {
        if(lunch_status == true && book_status == true){
           trigger_success_message();
        }
      }
    }
  }
}

void set_pixel(int num, int red, int green, int blue) {
    strip.setPixelColor(num, red, green, blue);
    strip.show();
}

void trigger_success_message() {
  display.clearDisplay();
  display.display();
  display.drawBitmap(0,0,success_message,256,64,WHITE);
  display.display();


  delay(2000);
  
  display.clearDisplay();
  display.display();

  success_message_triggered = true;
  set_screens_back_to_normal();
  }

void set_screens_back_to_normal(){
  display.setCursor(0, 0);
  display.drawBitmap(0,0,lunchbook,256,64,WHITE);
  display.display();
  display.clearDisplay();
  
  if(water_is_visible == true){
    display.setCursor(0, 0);
    display.drawBitmap(0,0,lunchbookwater,256,64,WHITE);
    display.display();
    display.clearDisplay();
  }

}

void button_action(int button){

  if(button == 1) {
    if(lunch_status == false){
      lunch_status = true;
      set_pixel(lunchPixel, 0, greenPixelBrightness, 0); // green
    } else {
      lunch_status = false;
      set_pixel(lunchPixel, redPixelBrightness, 0, 0); // red
    }
  } 

  if(button == 2) {
    if(book_status == false){
      book_status = true;
      set_pixel(bookPixel, 0, greenPixelBrightness, 0); // green
    } else {
      book_status = false;
      set_pixel(bookPixel, redPixelBrightness, 0, 0); // red
    }
  } 

  if(button == 3) {
    if(water_is_visible == true){
      if(water_status == false){
        water_status = true;
        set_pixel(waterPixel, 0, greenPixelBrightness, 0); // green
      } else {
        water_status = false;
        set_pixel(waterPixel, redPixelBrightness, 0, 0); // red
      }
    }
  }

  if(button == 4) {
    if(water_is_visible == false){
      water_is_visible = true;  
      display.setCursor(0, 0);
      display.drawBitmap(0,0,lunchbookwater,256,64,WHITE);
      display.display();
      display.clearDisplay();
      set_pixel(waterPixel, redPixelBrightness, 0, 0);
    } else {
      water_is_visible = false;
      display.setCursor(0, 0);
      display.drawBitmap(0,0,lunchbook,256,64,WHITE);
      display.display();
      display.clearDisplay();
      water_is_green = false;
      set_pixel(waterPixel, 0, 0, 0);  
    }
  }

  if(button == 5) {
    clear_pixels();
    trigger_setup_message(connected_status);
  }
  
  if(button == 6) {
    clear_pixels();
    display.clearDisplay();
    display.display();
    display.setCursor(0, 0);
    display.drawBitmap(0,0,remindersUpdated,256,64,WHITE);
    display.display();
    display.clearDisplay();

    delay(2000);
    set_screens_back_to_normal();
    trigger_pixels();    
  }
      
  if(button == 7) {
    if (specialMessageCounter == 1){
      set_screens_back_to_normal();
      trigger_pixels();   
    } 
    if(specialMessageCounter == 0){
      clear_pixels();
      display.clearDisplay();
      display.display();
      display.setCursor(0, 0);
      display.drawBitmap(0,0,specialMessagePermissionSlip,256,64,WHITE);
      display.display();
      display.clearDisplay();
    }

    if(specialMessageCounter == 0){
      specialMessageCounter = 1;  
    } else {
      specialMessageCounter = 0;  
    }
  }

  if(button == 8) {
    show_icons();
  }
}

void sing(){      
     // iterate over the notes of the melody:
     Serial.print("singing");
     int size = sizeof(melody) / sizeof(int);
     for (int thisNote = 0; thisNote < size; thisNote++) {

       // to calculate the note duration, take one second
       // divided by the note type.
       //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
       int noteDuration = 1000/tempo[thisNote];

       buzz(melodyPin, melody[thisNote],noteDuration);

       // to distinguish the notes, set a minimum time between them.
       // the note's duration + 30% seems to work well:
       int pauseBetweenNotes = noteDuration * 1.30;
       delay(pauseBetweenNotes);

       // stop the tone playing:
       buzz(melodyPin, 0,noteDuration);

    }
  }

void buzz(int targetPin, long frequency, long length) {
//  digitalWrite(10,HIGH);
  long delayValue = 1000000/frequency/2; // calculate the delay value between transitions
  //// 1 second's worth of microseconds, divided by the frequency, then split in half since
  //// there are two phases to each cycle
  long numCycles = frequency * length/ 1000; // calculate the number of cycles for proper timing
  //// multiply frequency, which is really cycles per second, by the number of seconds to 
  //// get the total number of cycles to produce
  for (long i=0; i < numCycles; i++){ // for the calculated length of time...
    digitalWrite(targetPin,HIGH); // write the buzzer pin high to push out the diaphram
    delayMicroseconds(delayValue); // wait for the calculated delay value
    digitalWrite(targetPin,LOW); // write the buzzer pin low to pull back the diaphram
    delayMicroseconds(delayValue); // wait again or the calculated delay value
  }
//  digitalWrite(10,LOW);

}

void trigger_setup_message(int state_num) {
//  tcaselect(2);
//  Display1.clearDisplay();
//  Display1.display();
//  Display1.clearDisplay();
//
//  tcaselect(4);
//  Display3.clearDisplay();
//  Display3.display();
//  Display3.clearDisplay();

  if(state_num == 1){
//    tcaselect(3);
//    Display2.setCursor(0, 0);
//    Display2.drawBitmap(4,0,open_our_app_to_continue,120,64,WHITE);
//    Display2.display();
//    Display2.clearDisplay();
    connected_status = 2;   
  }

  if(state_num == 2){
//    tcaselect(3);
//    Display2.setCursor(0, 0);
//    Display2.drawBitmap(4,0,connecting,120,64,WHITE);
//    Display2.display();
//    Display2.clearDisplay();
    connected_status = 3;   
  }

  if(state_num == 3){
//    tcaselect(3);
//    Display2.setCursor(0, 0);
//    Display2.drawBitmap(4,0,connected_msg,120,64,WHITE);
//    Display2.display();
//    Display2.clearDisplay(); 
    connected_status = 4;   
  }

  
  if(state_num == 4){
//    tcaselect(3);
//    Display2.setCursor(0, 0);
//    Display2.drawBitmap(4,0,awaiting_setup,120,64,WHITE);
//    Display2.display();
//    Display2.clearDisplay();
    connected_status = 1;   
  }
}

void clear_pixels(){
    set_pixel(bookPixel, 0, 0, 0);
    set_pixel(lunchPixel, 0, 0, 0);
    set_pixel(waterPixel, 0, 0, 0);
    water_status = false;
    lunch_status = false;
    book_status = false;
         
}

void trigger_pixels(){
  set_pixel(lunchPixel, redPixelBrightness, 0, 0);
  set_pixel(bookPixel, redPixelBrightness, 0, 0);
  water_status = false;
  lunch_status = false;
  book_status = false;
  if(water_is_visible == true){
     set_pixel(waterPixel, redPixelBrightness, 0, 0); 
  }
}

void show_icons(){
  icons_visible = true;
  display.clearDisplay();
  display.display();
  display.setCursor(0, 0);
  display.drawBitmap(0,0,lunchbook,256,64,WHITE);
  display.display();
  display.clearDisplay();

  if(water_is_visible == true){
      icons_visible = true;
      display.setCursor(0, 0);
      display.drawBitmap(0,0,lunchbookwater,256,64,WHITE);
      display.display();
      display.clearDisplay();

      set_pixel(lunchPixel, redPixelBrightness, 0, 0);
  }

  set_pixel(bookPixel, redPixelBrightness, 0, 0);
  set_pixel(waterPixel, redPixelBrightness, 0, 0);
  water_status = false;
  lunch_status = false;
  book_status = false;
}
