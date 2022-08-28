/*
* BeeMonitor V1.5
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
#include "LowPower.h"
#include "FastLED.h"
#include "Button.h"                                           // Button library. Includes press, long press, double press detection.
#include <DHT.h>
#include <DHT_U.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Average.h>
#include <LiquidCrystal.h>

// FastLED Setup
#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN   8
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS    5
#define BRIGHTNESS  10
CRGB leds[NUM_LEDS];

#define FRAMES_PER_SECOND  30

// DHT11 setup
#define DHTTYPE DHT11   // DHT 11
#define DHTPIN A2
DHT dht(DHTPIN, DHTTYPE);

// DS18B20 setup
#define ONE_WIRE_BUS A1
OneWire ds(ONE_WIRE_BUS);
DallasTemperature sensors(&ds);

// Pushbutton pin definition
#define BUTTON_PIN 2
bool oldState = HIGH;
int showType = 0;

// Display setup
LiquidCrystal lcd(10, 9, 6, 5, 3, A0);
#define lcdBL 7
int lcdBLState;
int lcdState;

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
  pinMode(lcdBL, OUTPUT);

  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness( BRIGHTNESS );

  lcd.begin(16, 2);
  digitalWrite(lcdBL, HIGH);
  lcdBLState = 1;
  
  Serial.begin(9600);
  Serial.println("Bee Monitor");

  dht.begin();
  sensors.begin();
  startup();
}

// List of modes to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = {idle, tempBrood, tempTop, humidity };

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current



void loop(){
  // Check sensors periodically
  const unsigned long minutes = 5 * 60 * 1000UL;
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

  if (lcdBLState == 1){
    digitalWrite(lcdBL, LOW);
    lcd.noDisplay();
    lcdBLState = 0;
  }
  
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

  attachInterrupt(0, wakeUp, LOW);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(0);
  
}



void tempBrood(){
  
  Serial.print("Deep Temperature F");
  Serial.println(tempD);

  if (lcdBLState == 0){
    digitalWrite(lcdBL, HIGH);
    lcd.display();
    lcdBLState = 1;
  }

  lcd.setCursor(0, 0);
  lcd.println("Brood Temp F    ");
  lcd.setCursor(0, 1);
  lcd.print(tempD);
  lcd.print(" ");
  lcd.setCursor(6, 1);
  lcd.print(" Avg:");
  lcd.print(avgTempD);
  
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[0] = CRGB::Yellow;

  int D = tempD * 100;

}



void tempTop(){
  
  Serial.print("Top Temperature F");
  Serial.println(tempS);

  if (lcdBLState == 0){
    digitalWrite(lcdBL, HIGH);
    lcd.display();
    lcdBLState = 1;
  }

  lcd.setCursor(0, 0);
  lcd.println("Top Temp F      ");
  lcd.setCursor(0, 1);
  lcd.print(tempS);
  lcd.print(" ");
  lcd.setCursor(6, 1);
  lcd.print(" Avg:");
  lcd.print(avgTempS);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[1] = CRGB::Yellow;

  int S = tempS * 100;
  
}



void humidity(){
  
  Serial.print("Humidity");
  Serial.println(humid);

  if (lcdBLState == 0){
    digitalWrite(lcdBL, HIGH);
    lcd.display();
    lcdBLState = 1;
  }

  lcd.setCursor(0, 0);
  lcd.println("Humidity %      ");
  lcd.setCursor(0, 1);
  lcd.print(humid);
  lcd.print(" ");
  lcd.setCursor(6, 1);
  lcd.print(" Avg:");
  lcd.print(avgHumid);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[2] = CRGB::Yellow;

  int H = humid * 100;

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
  
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  lcd.clear();
  lcd.print("Bee Monitor");
  lcd.setCursor(0,1);
  lcd.print("Starting Up");
  
  for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Yellow;
        lcd.print(".");
        delay(300);
        FastLED.show();
    }

  lcd.clear();
  
}



void wakeUp()
{
    // Just a handler for the pin interrupt.
}
