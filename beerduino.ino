// This #include statement was automatically added by the Particle IDE.
#include "ntp-time.h"

// This #include statement was automatically added by the Particle IDE.
#include "SparkFunMicroOLED.h"

// what's a log?
#include <cmath>

#define TEMP_DELAY_IN_mS 60000
#define BUBBLE_DELAY_IN_mS 1000

#define TEMPERATURE_SETPOINT 21.1

#define ANALOG_INPUT_TEMPERATURE A0
#define ANALOG_INPUT_BUBBLE A1
#define DIGITAL_OUTPUT_RELAY 0
#define DIGITAL_OUTPUT_BUBBLE 1

//////////////////////////////////
// MicroOLED Object Declaration //
//////////////////////////////////
// Declare a MicroOLED object. If no parameters are supplied, default pins are
// used, which will work for the Photon Micro OLED Shield (RST=D7, DC=D6, CS=A2)
MicroOLED oled;

static float average_reading = 0;
bool is_bubble = false;
int n_bubbles = 0;
int heater = LOW;

NtpTime* ntpTime;

void setup() {
  // MicroOLED initialization
  oled.begin();    // Initialize the OLED
  oled.clear(ALL); // Clear the display's internal memory
  oled.display();  // Display what's in the buffer (splashscreen)

  // Do an ntp update every 60 minutes;
  //ntpTime = new NtpTime(60);
  //ntpTime->start();
  Time.zone(-5);

  // Hello world
  oled.clear(PAGE);
  oled.setCursor(0, 0);
  oled.println("BeerDuino!");
  oled.println(Time.format(Time.now(),"%I:%M%p"));
  oled.display();

  // Pin to LED
  pinMode(DIGITAL_OUTPUT_BUBBLE,OUTPUT);
  // Pin to relay board
  pinMode(DIGITAL_OUTPUT_RELAY,OUTPUT);

  // Setup the average of the bubble signal (assuming no bubble)
  average_reading = analogRead(ANALOG_INPUT_BUBBLE);
  
  // Wait a bit
  delay(1000);
}


void loop() {
  
  // Clear the display
  oled.clear(PAGE);
  int line = 0;
  // Smallest font
  oled.setFontType(0);
  // Time
  oled.setCursor(0, 8*line++);
  oled.println(Time.format(Time.now(),"%I:%M%p"));
  // Print ADC read
  oled.setCursor(0, 8*line++);
  oled.print("A0=");
  oled.print(analogRead(ANALOG_INPUT_TEMPERATURE));
  // Print temperature
  oled.setCursor(0, 8*line++);
  oled.print("T[C]=");
  oled.print(temp_cal(analogRead(ANALOG_INPUT_TEMPERATURE)));
  // Print heater status
  oled.setCursor(0, 8*line++);
  oled.print("heat=");
  oled.print(heater? "HI": "LO");
  // Display
  oled.display();
  delay(100);

  // Check for bubble
  if (false || timeForBubble()) {
    static const int delta = 5;

    // Take a reading from the photogate
    int current_reading = analogRead(ANALOG_INPUT_BUBBLE);
    if (current_reading < average_reading - delta) {

      // we have a bubble!
      digitalWrite(DIGITAL_OUTPUT_BUBBLE,HIGH);
      is_bubble = true;
      n_bubbles++;

      // Figure out how big the bubble is by measuring the next samples
      int minimum_in_bubble = 1023; // largest possible reading      
      for (int i = 0; i < 10; i++) {
        int reading_while_in_bubble = analogRead(ANALOG_INPUT_BUBBLE);
        if (reading_while_in_bubble < minimum_in_bubble) {
          minimum_in_bubble = reading_while_in_bubble;
        }
        // wait a bit
        delay(10);
      }
      float depth = average_reading - minimum_in_bubble;

    } else {
      // no bubble
      digitalWrite(DIGITAL_OUTPUT_BUBBLE,LOW);
      is_bubble = false;
    }

    // Update moving average (outside the loop so it doesn't get stuck in bubble-land)
    average_reading = 0.95 * average_reading + 0.05 * current_reading;
  }

  // Check temperature
  if (timeForTemp()) {

    // Read temperature
    int temp_reading = analogRead(ANALOG_INPUT_TEMPERATURE);  

    // Convert to deg C
    float tempc = temp_cal(temp_reading); //read from A0 pin on board for temperature

    // Turn heater ON or OFF depending on temperature
    if (tempc < TEMPERATURE_SETPOINT) {
      heater = HIGH;
      digitalWrite(DIGITAL_OUTPUT_RELAY,heater);
    } else {
      heater = LOW;
      digitalWrite(DIGITAL_OUTPUT_RELAY,heater);
    }

    // Publish values  
    String temperature = String(tempc);
    Particle.publish("temperature", temperature, PRIVATE);

    // Reset number of bubbles
    n_bubbles = 0;
  }
}

//****************functions*************************
float temp_cal(float bits_t) {
  float r2_t = 10000; //10k fixed resistor
  float a = 3.354016E-3; //Steinhart-Hart Coefficients
  float b = 2.569850E-4;
  float c = 2.620131E-6;
  float d = 6.383091E-8;
  float rref = 10000;

  float v2_t = (bits_t*3.3)/4096; //voltage after thermisor
  float i_t = v2_t/r2_t; //current through thermistor loop
  float v1_t = 3.3-v2_t; //voltage dropped across thermistor
  float r1_t = v1_t/i_t; //resistance of thermistor
  float x = r1_t/rref;
  float bc = log(x);
  float cc = log(x)*log(x);
  float dc = log(x)*log(x)*log(x);
  float tempk = 1/(a+b*bc+c*cc+d*dc); //Steinhart-Hart Eqn Need Coeff A, B, C
  float tempc = tempk - 273.15;
  return tempc;
}

// Function to indicate when a temp measurement is due
bool timeForTemp() {
  static long delta = TEMP_DELAY_IN_mS;
  static long previous = -delta;
  if (millis() > previous + delta) {
    previous = millis();
    return true;
  }
  return false;
}

// Function to indicate when a temp measurement is due
bool timeForBubble() {
  static long delta = BUBBLE_DELAY_IN_mS;
  static long previous = -delta;
  if (millis() > previous + delta) {
    previous = millis();
    return true;
  }
  return false;
}

// Center and print a small title
// This function is quick and dirty. Only works for titles one
// line long.
void printTitle(String title, int font)
{
  int middleX = oled.getLCDWidth() / 2;
  int middleY = oled.getLCDHeight() / 2;

  oled.clear(PAGE);
  oled.setFontType(font);
  // Try to set the cursor in the middle of the screen
  oled.setCursor(middleX - (oled.getFontWidth() * (title.length()/2)),
                 middleY - (oled.getFontWidth() / 2));
  // Print the title:
  oled.print(title);
  oled.display();
  delay(1500);
  oled.clear(PAGE);
}

