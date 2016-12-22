// This #include statement was automatically added by the Particle IDE.
#include "ntp-time.h"

// This #include statement was automatically added by the Particle IDE.
#include "SparkFunMicroOLED.h"

// what's a log?
#include <cmath>

#define TEMP_DELAY_IN_mS 60000
#define BUBBLE_DELAY_IN_mS 1000

#define TEMPERATURE_DEFAULT_SETPOINT 20.0

#define ANALOG_INPUT_TEMPERATURE A0
#define ANALOG_INPUT_BUBBLE A1
#define DIGITAL_OUTPUT_RELAY 2
#define DIGITAL_OUTPUT_BUBBLE 3

#define HEATER_POWER 20

//////////////////////////////////
// MicroOLED Object Declaration //
//////////////////////////////////
// Declare a MicroOLED object. If no parameters are supplied, default pins are
// used, which will work for the Photon Micro OLED Shield (RST=D7, DC=D6, CS=A2)
MicroOLED oled;

// Bubble counter variables
float average_photodiode_reading = 0;
float bubble_depth = 0;
bool is_bubble = false;
int number_bubbles = 0;

// Temperature variables
float temperature_setpoint = TEMPERATURE_DEFAULT_SETPOINT;
int heater = LOW;
float heater_power = 0;
float average_heater_power = 0;
unsigned int heater_history = 0;
unsigned int heater_history_data = 0; // filled values
unsigned int heater_history_mask = 0xffff; // number of minutes

// API key for channel in ThingSpeak
const String key = "WWKPLDK8B1197QID";
//
// JSON for particle API webhook
// {
//     "event": "thingSpeakWrite_",
//     "url": "https://api.thingspeak.com/update",
//     "requestType": "POST",
//     "form": {
//         "api_key": "{{k}}",
//         "field1": "{{1}}",
//         "field2": "{{2}}",
//         "field3": "{{3}}",
//         "field4": "{{4}}",
//         "field5": "{{5}}",
//         "field6": "{{6}}",
//         "field7": "{{7}}",
//         "field8": "{{8}}",
//         "lat": "{{a}}",
//         "long": "{{o}}",
//         "elevation": "{{e}}",
//         "status": "{{s}}"
//     },
//     "mydevices": true,
//     "noDefaults": true
// }

// Time
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
  oled.println("BeerDuino");
  oled.println(" v2.0");
  oled.println(" ");
  oled.println(Time.format(Time.now(),"%I:%M %p"));
  oled.display();

  // Pin to LED
  pinMode(DIGITAL_OUTPUT_BUBBLE,OUTPUT);
  // Pin to relay board
  pinMode(DIGITAL_OUTPUT_RELAY,OUTPUT);

  // Setup the average of the bubble signal (assuming no bubble)
  average_photodiode_reading = analogRead(ANALOG_INPUT_BUBBLE);
  
  // Register setpoint function
  Particle.function("setpoint", setTemperatureSetpoint);
  
  // Wait a bit
  delay(1000);
}

float setTemperatureSetpoint(String command)
{
  temperature_setpoint = command.toFloat();
  return temperature_setpoint;
}

void loop() {
  
  // Clear the display
  oled.clear(PAGE);
  int line = 0;
  // Smallest font
  oled.setFontType(0);
  // Time
  oled.setCursor(0, 8*line++);
  oled.println(Time.format(Time.now(),(Time.now()%2)?"[%I:%M %p]":"[%I %M %p]"));
  // Print ADC read
  oled.setCursor(0, 8*line++);
  oled.print("A0=");
  oled.print(analogRead(ANALOG_INPUT_TEMPERATURE));
  // Print ADC read
  oled.setCursor(0, 8*line++);
  oled.print("A1=");
  oled.print(analogRead(ANALOG_INPUT_BUBBLE));
  // Print temperature
  oled.setCursor(0, 8*line++);
  oled.print("T[C]=");
  oled.print(temp_cal(analogRead(ANALOG_INPUT_TEMPERATURE)));
  // Print heater status
  oled.setCursor(0, 8*line++);
  oled.print("heater=");
  oled.print((heater>0)? "HI": "LO");
  // Print number of bubbles
  oled.setCursor(0, 8*line++);
  oled.print("#blub=");
  oled.print(number_bubbles);
  // Display
  oled.display();
  delay(100);

  // Check for bubble
  if (timeForBubble()) {
    static const int delta = 5;

    // Take a reading from the photogate
    int current_photodiode_reading = analogRead(ANALOG_INPUT_BUBBLE);
    if (current_photodiode_reading > average_photodiode_reading + delta) {

      // we have a bubble!
      digitalWrite(DIGITAL_OUTPUT_BUBBLE,HIGH);
      is_bubble = true;
      number_bubbles++;

      // Figure out how big the bubble is by measuring the next samples
      int maximum_in_bubble = 0; // smallest possible reading      
      for (int i = 0; i < 10; i++) {
        int reading_while_in_bubble = analogRead(ANALOG_INPUT_BUBBLE);
        if (reading_while_in_bubble > maximum_in_bubble) {
          maximum_in_bubble = reading_while_in_bubble;
        }
        // wait a bit
        delay(10);
      }
      bubble_depth = maximum_in_bubble - average_photodiode_reading;

    } else {
      // no bubble
      digitalWrite(DIGITAL_OUTPUT_BUBBLE,LOW);
      is_bubble = false;
    }

    // Update moving average (outside the loop so it doesn't get stuck in bubble-land)
    average_photodiode_reading = 0.95 * average_photodiode_reading + 0.05 * current_photodiode_reading;
  }

  // Check temperature
  if (timeForTemp()) {

    // Convert to deg C
    // read from A0 pin on board for temperature
    float temperature_celsius = temp_cal(analogRead(ANALOG_INPUT_TEMPERATURE));

    // Turn heater ON or OFF depending on temperature
    if (temperature_celsius < temperature_setpoint) {
      heater = HIGH;
    } else {
      heater = LOW;
    }
    heater_power = heater * HEATER_POWER;
    heater_history = ((heater_history << 1) + heater);
    heater_history &= heater_history_mask;
    heater_history_data = (heater_history_data << 1) + 1;
    heater_history_data &= heater_history_mask;
    digitalWrite(DIGITAL_OUTPUT_RELAY,heater);
    
    // Update moving average
    // exponential average
    average_heater_power = 0.95 * average_heater_power + 0.05 * heater_power;
    // average of last bits
    average_heater_power = float(bitCount(heater_history)) / float(bitCount(heater_history_data)) * HEATER_POWER;

    // Publish values  
    String BeerDuino_data = String("{ ") +
        "\"1\": \"" + String(analogRead(ANALOG_INPUT_TEMPERATURE)) + "\"," +
        "\"2\": \"" + String(temperature_celsius) + "\"," +
        "\"3\": \"" + String(temperature_setpoint) + "\"," +
        "\"4\": \"" + String(heater_power) + "\"," +
        "\"5\": \"" + String(average_photodiode_reading) + "\"," +
        "\"6\": \"" + String(bubble_depth) + "\"," +
        "\"7\": \"" + String(number_bubbles) + "\"," +
        "\"8\": \"" + String(average_heater_power) + "\"," +
        "\"k\": \"" + key + "\" }";
    Particle.publish("BeerDuino_data",BeerDuino_data,PRIVATE);

    // Reset number of bubbles
    number_bubbles = 0;
    bubble_depth = 0;
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

// Function to count bits in an unsigned int
int bitCount(unsigned int u) {
  // https://blogs.msdn.microsoft.com/jeuge/2005/06/08/bit-fiddling-3/
  unsigned int count = u - ((u >> 1) & 033333333333) - ((u >> 2) & 011111111111);
  return ((count + (count >> 3)) & 030707070707) % 63;
}
