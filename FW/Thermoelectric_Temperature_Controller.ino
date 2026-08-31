#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =========================
// BUTTONS
// =========================
#define B1 2
#define B2 3
#define B3 4

// =========================
// L298N
// =========================
#define PELTIER_IN 5
#define HEATER_IN 6

// =========================
// THERMISTORS
// =========================
#define HEATER_THERMISTOR A0
#define PELTIER_THERMISTOR A1

// =========================
// POTENTIOMETER
// =========================
#define POT A2

// =========================
// MODE
// =========================
String mode = "OFF";

// =========================
// THERMISTOR SETTINGS
// =========================
const float SERIES_RESISTOR = 10000.0;
const float BETA = 3950.0;

// Powered room-temperature ADC readings
const float A0_ROOM_ADC = 859.0;
const float A1_ROOM_ADC = 871.0;

const float ROOM_TEMP = 25.0;


// =====================================================
// READ TEMPERATURE
// =====================================================

float readTemperature(int pin) {

  int adc = analogRead(pin);

  if (adc <= 0 || adc >= 1023) {
    return -999.0;
  }

  float referenceADC;

  if (pin == A0) {
    referenceADC = A0_ROOM_ADC;
  }
  else {
    referenceADC = A1_ROOM_ADC;
  }


  // REVERSED THERMISTOR DIVIDER
  //
  // 5V
  // |
  // 10K resistor
  // |
  // +---- A0/A1
  // |
  // NTC
  // |
  // GND

  float resistance =
    SERIES_RESISTOR *
    ((float)adc / (1023.0 - adc));


  float referenceResistance =
    SERIES_RESISTOR *
    (referenceADC / (1023.0 - referenceADC));


  // Beta equation

  float temperatureK =
    1.0 /
    (
      (1.0 / (ROOM_TEMP + 273.15)) +
      (1.0 / BETA) *
      log(resistance / referenceResistance)
    );


  return temperatureK - 273.15;
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(9600);

  pinMode(B1, INPUT_PULLUP);
  pinMode(B2, INPUT_PULLUP);
  pinMode(B3, INPUT_PULLUP);

  pinMode(PELTIER_IN, OUTPUT);
  pinMode(HEATER_IN, OUTPUT);

  digitalWrite(PELTIER_IN, LOW);
  digitalWrite(HEATER_IN, LOW);


  // OLED

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C
      )) {

    Serial.println("OLED NOT FOUND!");

    while (1);
  }

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("TEMP CONTROLLER");

  display.setCursor(0, 12);
  display.println("Starting...");

  display.display();

  delay(1500);
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  // =========================
  // BUTTONS
  // =========================

  if (digitalRead(B1) == LOW) {

    mode = "OFF";

    digitalWrite(HEATER_IN, LOW);
    digitalWrite(PELTIER_IN, LOW);

    delay(200);
  }

  else if (digitalRead(B2) == LOW) {

    mode = "COLD";

    digitalWrite(HEATER_IN, LOW);

    delay(200);
  }

  else if (digitalRead(B3) == LOW) {

    mode = "HOT";

    digitalWrite(PELTIER_IN, LOW);

    delay(200);
  }


  // =========================
  // READ TEMPERATURES
  // =========================

  float heaterTemp =
    readTemperature(A0);

  float peltierTemp =
    readTemperature(A1);


  // =========================
  // POTENTIOMETER
  // 10–50°C
  // =========================

  int potValue = analogRead(POT);

  float targetTemp =
    10.0 +
    ((float)potValue / 1023.0) * 40.0;


  // =========================
  // OUTPUT STATES
  // =========================

  bool heaterON = false;
  bool peltierON = false;


  // =========================
  // HOT
  // =========================

  if (mode == "HOT") {

    peltierON = false;

    // Heater ON below target - 1°C

    if (heaterTemp < targetTemp - 1.0) {
      heaterON = true;
    }

    // Heater OFF at target

    else if (heaterTemp >= targetTemp) {
      heaterON = false;
    }
  }


  // =========================
  // COLD
  // =========================

  else if (mode == "COLD") {

    heaterON = false;

    // Peltier ON above target + 1°C

    if (peltierTemp > targetTemp + 1.0) {
      peltierON = true;
    }

    // Peltier OFF at target

    else if (peltierTemp <= targetTemp) {
      peltierON = false;
    }
  }


  // =========================
  // OFF
  // =========================

  else {

    heaterON = false;
    peltierON = false;
  }


  // =========================
  // SAFETY
  // =========================

  if (heaterON) {
    peltierON = false;
  }

  if (peltierON) {
    heaterON = false;
  }


  // =========================
  // L298N
  // =========================

  digitalWrite(
    HEATER_IN,
    heaterON ? HIGH : LOW
  );

  digitalWrite(
    PELTIER_IN,
    peltierON ? HIGH : LOW
  );


  // =========================
  // SERIAL
  // =========================

  Serial.print("MODE: ");
  Serial.print(mode);

  Serial.print(" | SET: ");
  Serial.print(targetTemp, 1);

  Serial.print("C | A0: ");
  Serial.print(heaterTemp, 1);

  Serial.print("C | HEATER: ");
  Serial.print(heaterON ? "ON" : "OFF");

  Serial.print(" | A1: ");
  Serial.print(peltierTemp, 1);

  Serial.print("C | PELTIER: ");
  Serial.println(peltierON ? "ON" : "OFF");


  // =========================
  // OLED
  // =========================

  display.clearDisplay();


  // LINE 1

  display.setCursor(0, 0);

  display.print(mode);

  display.print(" SET:");

  display.print(targetTemp, 1);

  display.print("C");


  // LINE 2

  display.setCursor(0, 10);

  display.print("H:");

  display.print(heaterTemp, 1);

  display.print("C ");

  display.print("P:");

  display.print(peltierTemp, 1);

  display.print("C");


  // LINE 3

  display.setCursor(0, 21);

  if (mode == "HOT") {

    display.print("HEATER: ");
    display.print(heaterON ? "ON" : "OFF");

  }

  else if (mode == "COLD") {

    display.print("PELTIER: ");
    display.print(peltierON ? "ON" : "OFF");

  }

  else {

    display.print("OUTPUT: OFF");
  }


  display.display();

  delay(500);
}
