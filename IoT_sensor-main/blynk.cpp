/*************************************************************
  Blynk is a platform with iOS and Android apps to control
  ESP32, Arduino, Raspberry Pi and the likes over the Internet.
  You can easily build mobile and web interfaces for any
  projects by simply dragging and dropping widgets.

    Downloads, docs, tutorials: https://www.blynk.io
    Sketch generator:           https://examples.blynk.cc
    Blynk community:            https://community.blynk.cc
    Follow us:                  https://www.fb.com/blynkapp
                                https://twitter.com/blynk_app

  Blynk library is licensed under MIT license
 *************************************************************
  Blynk.Edgent implements:
  - Blynk.Inject - Dynamic WiFi credentials provisioning
  - Blynk.Air    - Over The Air firmware updates
  - Device state indication using a physical LED
  - Credentials reset using a physical Button
 *************************************************************/

/* Fill in information from your Blynk Template here */
/* Read more: https://bit.ly/BlynkInject */
#define BLYNK_TEMPLATE_ID "TMPL66owXtvQX"
#define BLYNK_TEMPLATE_NAME "Swinburne IoT"

#define BLYNK_FIRMWARE_VERSION        "0.1.0"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#define APP_DEBUG
// Uncomment your board, or configure a custom board in Settings.h
//#define USE_ESP32_DEV_MODULE
//#define USE_ESP32C3_DEV_MODULE
//#define USE_ESP32S2_DEV_KIT
//#define USE_WROVER_BOARD
//#define USE_TTGO_T7
//#define USE_TTGO_T_OI
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <string>

#include "BlynkEdgent.h"

#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ2_PIN 34         // Analog input from MQ2
#define FLAME_PIN 26       // Digital input from flame sensor
#define LED_PIN 2          // Onboard LED
#define BUZZER_PIN 27      // Buzzer pin

// ----------- Thresholds ---------------
#define TEMP_THRESHOLD 23       // °C
#define GAS_THRESHOLD 1      // Analog value

// ----------- OLED Setup ---------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ----------- Sensor Objects -----------
DHT dht(DHTPIN, DHTTYPE);

void setup()
{
  Serial.begin(9600);
  Serial.println("Starting ESP32 with DHT11, MQ2, Flame, OLED, LED & Buzzer...");

  // Initialize sensors type
  dht.begin();
  pinMode(FLAME_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  display.println("System Ready!");
  display.display();
  delay(2000);

  BlynkEdgent.begin();
}

void loop() {
  BlynkEdgent.run();
// Read sensor data
  float temperature = dht.readTemperature();
  int gasValue = analogRead(MQ2_PIN);
  int flameDetected = digitalRead(FLAME_PIN); // 0 = flame detected

  // Trigger alerts
  bool tempAlert = temperature >= TEMP_THRESHOLD;
  bool gasAlert = gasValue >= GAS_THRESHOLD;
  bool flameAlert = (flameDetected == 0); // flame detected

  // Activate LED and Buzzer if any alert is active
  if (tempAlert || gasAlert || flameAlert) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Serial Monitor Output
  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print(" C | Gas: ");
  Serial.print(gasValue);
  Serial.print(" Gas level: ");
  Serial.print(gasAlert ? "Unsafe" : "Safe");  
  Serial.print(" | Flame: ");
  Serial.println(flameAlert ? "YES" : "NO");

  // OLED Display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  if (isnan(temperature)) {
    display.println("Temp: -- C");
  } else {
    display.print("Temp: ");
    display.print(temperature, 1);
    display.println(" C");
  }

  display.print("Gas : ");
  display.println(gasValue);
  display.print("Gas level: ");
  display.println(gasAlert ? "Unsafe" : "Safe");
  display.print("Flame: ");
  display.println(flameAlert ? "YES" : "NO");

  if (tempAlert || gasAlert || flameAlert) {
    display.setCursor(0, 55);
    display.print("Alert detected");
  }

  // Output to Blynk
  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, gasValue);
  Blynk.virtualWrite(V2, flameAlert);

  if (tempAlert || gasAlert || flameAlert) {
    Blynk.virtualWrite(V3, true);
 
    String tempAlertMsg ("");
    String gasAlertMsg ("");
    String flameAlertMsg ("");
    String outputAlert ("");
    
    if (tempAlert) {
      tempAlertMsg = " High temperature detected! ";
      outputAlert = outputAlert + tempAlertMsg;
    }
    if (gasAlert) {
      gasAlertMsg = " High gas level detected! ";
      outputAlert = outputAlert + gasAlertMsg;
    }
    if (flameAlert) {
      flameAlertMsg = " Flame detected! ";
      outputAlert = outputAlert + flameAlertMsg;
    }
    Blynk.logEvent("app_prmopt", outputAlert);
  } else {
    Blynk.virtualWrite(V3, false);
  }

  display.display();
  delay(2000);
}