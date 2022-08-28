/*
* BeeMonitor V1
* Chris Loidolt
* 
* References:
* Temperature Sensor Displayed on 4 Digit 7 segment common CATHODE
* https://gist.github.com/ruisantos16/5419223
* 
* Averaging Library
* https://github.com/MajenkoLibraries/Average
* 
*/
#include "FastLED.h"
#include "Button.h"                                           // Button library. Includes press, long press, double press detection.
#include <DHT.h>
#include <DHT_U.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Average.h>

// FastLED Setup
#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN   12
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS    5
#define BRIGHTNESS  10
CRGB leds[NUM_LEDS];

#define FRAMES_PER_SECOND  30

// DHT11 setup
#define DHTTYPE DHT11   // DHT 11
#define DHTPIN A1
DHT dht(DHTPIN, DHTTYPE);

// DS18B20 setup
#define ONE_WIRE_BUS 10
OneWire ds(ONE_WIRE_BUS);
DallasTemperature sensors(&ds);

// Pushbutton pin definition
const int BUTTON_PIN = A0;                                      // Digital pin used for debounced pushbutton

bool oldState = HIGH;
int showType = 0;

// Display setup
const int digitPins[4] = {5,4,3,2}; //4 common CATHODE pins of the display (inverted the pins order)
const int clockPin = 6;    //74HC595
const int latchPin = 7;    //74HC595
const int dataPin = 8;     //74HC595
const byte digit[10] =      //seven segment digits in bits
{
  B00111111, //0
  B00000110, //1
  B01011011, //2
  B01001111, //3
  B01100110, //4
  B01101101, //5
  B01111101, //6
  B00000111, //7
  B01111111, //8
  B01101111  //9
};
int digitBuffer[4] = {0};
int digitScan = 0;

// Status Variables
float tempS;
float tempD;
float humid;
float avgTempS;
float avgTempD;
float avgHumid;

// Average Readings
Average <int>tempDArray(60);
Average <int>tempSArray(60);
Average <int>humidArray(60);

// Reference Values
float idealTemp = 95;
float warningTempLow = 55;
float dangerTempLow = 46;

float idealHumid = 55;
float warningHumidLow = 45;
float warningHumidHigh = 75;
float dangerHumidLow = 35;
float dangerHumidHigh = 85;

void setup(){
  delay(1000);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness( BRIGHTNESS );
  
  for(int i=0;i<4;i++)
  {
    pinMode(digitPins[i],OUTPUT);
  }
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT); 
  
  //Serial.begin(9600);
  //Serial.println("Bee Monitor");

  dht.begin();
  sensors.begin();
  startup();
}

// List of modes to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = {idle, tempDeep, tempShallow, humidity, tempDeepAvg, tempShallowAvg, humidityAvg };

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current

void loop(){
  // Check sensors every minute
  const unsigned long minutes = 1 * 60 * 1000UL;
  static unsigned long lastSampleTime = 0 - minutes;  // initialize such that a reading is due the first time through loop()
  
  unsigned long now = millis();
  if (now - lastSampleTime >= minutes)
  {
    lastSampleTime += minutes;
    updateSensors();
  }
  
  // Call the current mode once, updating the display
  gPatterns[gCurrentPatternNumber]();
  
  FastLED.show();
  
  readbutton();
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

void idle(){
  
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  
  if (tempD <= dangerTempLow || humid <= dangerHumidLow || humid >= dangerHumidHigh){
    for( int i = 0; i < 4; i++) {
        leds[i] = CRGB::Red;
    }
  }
  else if (tempD <= warningTempLow || humid <= warningHumidLow || humid >= warningHumidHigh){
    for( int i = 0; i < 4; i++) {
        leds[i] = CRGB::Yellow;
    }
  }
  else {
    for( int i = 0; i < 4; i++) {
        leds[i] = CRGB::Green;
    }
  }
  
  if (avgTempD <= dangerTempLow || avgHumid <= dangerHumidLow || avgHumid >= dangerHumidHigh){
    leds[4] = CRGB::Red;
  }
  else if (avgTempS <= warningTempLow || avgHumid <= warningHumidLow || avgHumid >= warningHumidHigh){
    leds[4] = CRGB::Yellow;
  }
  else {
    leds[4] = CRGB::Green;
  }
  
  for(byte j=0; j<4; j++) {digitalWrite(digitPins[j], HIGH);} // Turns the display off
  
}

void tempDeep(){
  //Serial.print("Deep Temperature: ");
  //Serial.println(tempD);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[0] = CRGB::Yellow;

  int D = tempD * 100;

  digitBuffer[3] = D/1000;
  digitBuffer[2] = (D%1000)/100;
  digitBuffer[1] = (D%100)/10;
  digitBuffer[0] = (D%100)%10;
  updateDisp();
}

void tempShallow(){
  //Serial.print("Shallow Temperature: ");
  //Serial.println(tempS);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[1] = CRGB::Yellow;

  int S = tempS * 100;
  
  digitBuffer[3] = S/1000;
  digitBuffer[2] = (S%1000)/100;
  digitBuffer[1] = (S%100)/10;
  digitBuffer[0] = (S%100)%10;
  updateDisp();
}

void humidity(){
  //Serial.print("Humidity: ");
  //Serial.println(humid);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[2] = CRGB::Yellow;

  int H = humid * 100;

  digitBuffer[3] = H/1000;
  digitBuffer[2] = (H%1000)/100;
  digitBuffer[1] = (H%100)/10;
  digitBuffer[0] = (H%100)%10;
  updateDisp();
}

void tempDeepAvg(){
  //Serial.print("Average Deep Temperature: ");
  //Serial.println(avgTempD);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[0] = CRGB::Yellow;
  leds[4] = CRGB::Yellow;

  int DA = avgTempD * 100;

  digitBuffer[3] = DA/1000;
  digitBuffer[2] = (DA%1000)/100;
  digitBuffer[1] = (DA%100)/10;
  digitBuffer[0] = (DA%100)%10;
  updateDisp();
}

void tempShallowAvg(){
  //Serial.print("Average Shallow Temperature: ");
  //Serial.println(avgTempS);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[1] = CRGB::Yellow;
  leds[4] = CRGB::Yellow;

  int DS = avgTempS * 100;

  digitBuffer[3] = DS/1000;
  digitBuffer[2] = (DS%1000)/100;
  digitBuffer[1] = (DS%100)/10;
  digitBuffer[0] = (DS%100)%10;
  updateDisp();
}

void humidityAvg(){
  //Serial.print("Average Humidity: ");
  //Serial.println(avgHumid);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[2] = CRGB::Yellow;
  leds[4] = CRGB::Yellow;

  int HA = avgHumid * 100;

  digitBuffer[3] = HA/1000;
  digitBuffer[2] = (HA%1000)/100;
  digitBuffer[1] = (HA%100)/10;
  digitBuffer[0] = (HA%100)%10;
  updateDisp();
}

void updateDisp(){ //writes the display
  
  for(byte j=0; j<4; j++) {digitalWrite(digitPins[j], HIGH);} // Turns the display off. Changed to HIGH
  digitalWrite(latchPin, LOW); 
  shiftOut(dataPin, clockPin, MSBFIRST, B00000000);
  digitalWrite(latchPin, HIGH);

  delayMicroseconds(2);

  digitalWrite(digitPins[digitScan], LOW); //Changed to LOW for turning the leds on.

  digitalWrite(latchPin, LOW); 
  if(digitScan==2)
    shiftOut(dataPin, clockPin, MSBFIRST, (digit[digitBuffer[digitScan]] | B10000000)); //print the decimal point on the 3rd digit
  else
    shiftOut(dataPin, clockPin, MSBFIRST, digit[digitBuffer[digitScan]]);

  digitalWrite(latchPin, HIGH);

  digitScan++;
  if(digitScan>3) digitScan=0;
}

void updateSensors(){                                           // Check the sensors and update values
  
  tempS = dht.readTemperature(true);
  humid = dht.readHumidity();

  sensors.requestTemperatures();
  tempD = sensors.getTempFByIndex(0);

  // Push values into averaging arrays
  tempDArray.push(tempD);
  tempSArray.push(tempS);
  humidArray.push(humid);

  // Calculate averages from stored arrays
  avgTempD = tempDArray.mean();
  avgTempS = tempSArray.mean();
  avgHumid = humidArray.mean();
    
}

void readbutton(){                                           // Read the button and increase the mode
  // Get current button state.
  bool newState = digitalRead(BUTTON_PIN);

  // Check if state changed from high to low (button press).
  if (newState == LOW && oldState == HIGH) {
    // Short delay to debounce button.
    delay(100);
    // Check if button is still low after debounce.
    newState = digitalRead(BUTTON_PIN);
    if (newState == LOW) {
      nextPattern();
  }
}
}

void startup(){
  
  for(byte j=0; j<4; j++) {digitalWrite(digitPins[j], HIGH);} // Turns the display off
  
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  
  for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Yellow;
        delay(300);
        FastLED.show();
    }
}

