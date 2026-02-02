#include "secrets.h"                  // AWS_CERT_CA, AWS_CERT_CRT, AWS_CERT_PRIVATE, AWS_IOT_ENDPOINT, THINGNAME
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// --- AP mode for credential intake ---
const char* ap_ssid     = "MA4_IoT_device";
const char* ap_password = "12345678";
IPAddress local_IP(192,168,4,1), gateway(192,168,4,1), subnet(255,255,255,0);
// WebSocket server
WebSocketsServer webSocket = WebSocketsServer(81);

String ssidFromApp     = "";
String passwordFromApp = "";
String tempThresFromApp = "";
bool wifimode = false;
bool receivedWiFiCredentialFromApp = false;

// Global variables for sensor reading
float temperature;
int gasValue;
int flameDetected; // 0 = flame detected
int TEMP_THRESHOLD = 40; // set default value if not received.
bool flameAlert;
bool tempAlert;
bool gasAlert;

WiFiClientSecure net;
PubSubClient client(net);

// --- AWS IoT setup ---
#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

// ----------- Pin Definitions -----------
#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ2_PIN 34         // Analog input from MQ2
#define FLAME_PIN 26       // Digital input from flame sensor
#define LED_PIN 2          // Onboard LED
#define BUZZER_PIN 27      // Buzzer pin

// ----------- Thresholds ---------------
// #define TEMP_THRESHOLD 40       // °C
#define GAS_THRESHOLD 400      // Analog value

// ----------- OLED Setup ---------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ----------- Sensor Objects -----------
DHT dht(DHTPIN, DHTTYPE);

// WebSocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      }
      break;
    case WStype_TEXT:
      {
        Serial.printf("[%u] Get text: %s\n", num, payload);
        String msg = String((char*)payload);
        // Extract SSID
        int ssidKey = msg.indexOf("ssid");
        int ssidQuote1 = msg.indexOf("\"", ssidKey + 5);
        int ssidQuote2 = msg.indexOf("\"", ssidQuote1 + 1);
        ssidFromApp = msg.substring(ssidQuote1 + 1, ssidQuote2);
        // Extract Password
        int passKey = msg.indexOf("password");
        int passQuote1 = msg.indexOf("\"", passKey + 9);
        int passQuote2 = msg.indexOf("\"", passQuote1 + 1);
        passwordFromApp = msg.substring(passQuote1 + 1, passQuote2);
        // Extract Temp
        int tempKey = msg.indexOf("tempThre");
        int tempQuote1 = msg.indexOf("\"", tempKey + 9);
        int tempQuote2 = msg.indexOf("\"", tempQuote1 + 1);
        tempThresFromApp = msg.substring(tempQuote1 + 1, tempQuote2);
        int convertTempToInt = tempThresFromApp.toInt();
        if (ssidFromApp.length() > 0 && passwordFromApp.length() > 0 && convertTempToInt > 0) {
            receivedWiFiCredentialFromApp = true; // trigger WiFi mode
            TEMP_THRESHOLD = convertTempToInt;
            Serial.println("Temp threshold updated to: " + TEMP_THRESHOLD);
        }
      }
      break;
  }
}

void messageHandler(char* topic, byte* payload, unsigned int length)
{
  Serial.print("incoming: ");
  Serial.println(topic); 
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];
  Serial.println(message);
}

void connectAWS() {
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
  client.setServer(AWS_IOT_ENDPOINT, 8883);
  client.setCallback(messageHandler);

  Serial.println("Connecting to AWS IoT");
  while(!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(200);
  }
  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("AWS IoT Connected!");
}

void sendSensorData() {
  StaticJsonDocument<256> doc;
  doc["temperature"] = temperature;
  doc["gasValue"] = gasValue;
  doc["flameAlert"] = flameAlert;
  doc["tempAlert"] = tempAlert;
  doc["gasAlert"] = gasAlert;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);
  bool publishStatus = client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
  if (publishStatus) {
    Serial.println("MQTT sent {Temperature: " + String(temperature) + " Temp alert? " + String(tempAlert) + " Gas valve: " + String(gasValue) + " Gas Alert? " +  String(gasAlert) + "Flame alert? " + String(flameAlert)+ "}");
  } else {
    Serial.println("MQTT sent fails");
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Starting ESP32 in AP mode with WebSocket...");
  
  // Initialize sensors
  dht.begin();
  pinMode(FLAME_PIN, INPUT);
  // Output pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Connect App to WiFi: ");
  display.println(ap_ssid);
  display.println("---------------------");
  display.println("Then enter IP: 192.168.4.1 on App:");
  display.display();

  // configure AP static IP
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("AP started: " + String(ap_ssid));
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  // start WebSocket to receive SSID/password
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  delay(200);
}


void loop() {

	if (!wifimode) {
	webSocket.loop(); // local webSocket before WiFi mode
		// trigger WiFi after received text from App
		if (receivedWiFiCredentialFromApp) {
			Serial.println("Disconnecting from AT mode and swithing to Wi-Fi");
			display.println("Swith to Wi-Fi");
			WiFi.disconnect(true);
			delay(1000);

			WiFi.mode(WIFI_STA);
			WiFi.begin(ssidFromApp, passwordFromApp);

			while (WiFi.status() != WL_CONNECTED) {
				delay(500);
				Serial.print(".");
			}
			Serial.println("WiFi reconnected");
			Serial.print("New IP address: ");
			Serial.println(WiFi.localIP());
			wifimode = true;

			// NTP Sync time, then connect to AWS
			configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
			while (time(nullptr) < 57600) {
				delay(500);
			}
			Serial.println("NTP Time synced. Start AWS connection");
			connectAWS();
		}
		return;
	}
  
  if (!client.connected()) {
    client.connect(THINGNAME);
  }
	client.loop();

	// Read sensor data
	temperature = dht.readTemperature();
	gasValue = analogRead(MQ2_PIN);
	flameDetected = digitalRead(FLAME_PIN); // 0 = flame detected
	flameAlert = (flameDetected == 0);
	tempAlert = temperature >= TEMP_THRESHOLD;
	gasAlert = gasValue >= GAS_THRESHOLD;

	// Activate LED and Buzzer if any alert is active
	if (tempAlert || gasAlert || flameAlert) {
		digitalWrite(LED_PIN, HIGH);
		digitalWrite(BUZZER_PIN, HIGH);
	} else {
		digitalWrite(LED_PIN, LOW);
		digitalWrite(BUZZER_PIN, LOW);
	}
	// Send sensor data
	sendSensorData();

	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0, 0);

	// for app to connect to MQTT
	display.println("WiFi connected.");
    display.println("SerialID: a1gls53");
    display.println("DeviceID: esp32/pub");
    display.println("---------------------");

	display.print("Temp: ");
	display.print(temperature, 1);
	display.println(" C");

	display.print("Temp alert? ");
	display.println(gasAlert ? "Caution" : "Safe");

	display.print("Gas level: ");
	display.println(gasAlert ? "Caution" : "Safe");

	display.print("Flame? ");
	display.println(flameAlert ? "YES" : "NO");

	if (tempAlert || gasAlert || flameAlert) {
		// display.setCursor(0, 55);
		display.print("!! ALERT ACTIVE !!");
	}

	display.display();
	delay(2000);
}