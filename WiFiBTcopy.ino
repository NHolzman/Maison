#include "WiFiS3.h"
#include "arduino_secrets.h"
#include <ArduinoBLE.h>
#include <Adafruit_MCP23X17.h>
 
// --- WiFi configuration ---
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
int keyIndex = 0;
 
int ledPin = LED_BUILTIN;
int status = WL_IDLE_STATUS;
WiFiServer server(80);
 
// --- Bluetooth configuration ---
const int buttonPin = 4;
 
BLEService ledService("19B10010-E8F2-537E-4F6C-D104768A1214");
BLEByteCharacteristic ledCharacteristic("19B10011-E8F2-537E-4F6C-D104768A1214", BLERead | BLEWrite);
BLEByteCharacteristic buttonCharacteristic("19B10012-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify);
 
// Keypad + Lock
#define lock 13
#define C1 8
#define C2 9
#define C3 10
#define C4 11
#define C5 12
#define C6 13
#define C7 14
#define C8 0
#define C9 1
#define C10 2
#define C11 3
#define C12 4
 
int countDigits(unsigned long number) {
  String str = String(number);
  return str.length();
}
 
int digits;
int unlocked = 0;
int sensorPorte = 12;
 
unsigned long password = 3;
unsigned long position = 0;
unsigned long prevPosition = 0;
 
Adafruit_MCP23X17 mcp;
 
const int keypadPins[12] = { C1, C2, C3, C4, C5, C6, C7, C8, C9, C10, C11, C12 };
const char keypadLabels[12] = { '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '0', '#' };
 
String displayedPassword = "";  // To display password input on the web page
 
void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);
 
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Erreur: module WiFi non détecté !");
    while (true);
  }
 
  Serial.println("MCP23X17 Test...");
  if (!mcp.begin_I2C(0x20)) {
    Serial.println("Error connecting to MCP23X17.");
    //while (1);
  }
 
  pinMode(lock, OUTPUT);
  pinMode(sensorPorte, INPUT_PULLUP);
 
  for (int i = 0; i < 12; i++) {
    mcp.pinMode(keypadPins[i], INPUT_PULLUP);
  }
 
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Veuillez mettre à jour le firmware WiFi.");
  }
 
  while (status != WL_CONNECTED) {
    Serial.print("Connexion au réseau : ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }
 
  server.begin();
  printWifiStatus();
 
  while (!Serial);
 
  pinMode(buttonPin, INPUT);
 
  if (!BLE.begin()) {
    Serial.println("Erreur: le module Bluetooth® Low Energy ne démarre pas !");
    while (1);
  }
 
  BLE.setLocalName("Rocky");
  BLE.setDeviceName("Rocky");
  BLE.setAdvertisedService(ledService);
 
  ledService.addCharacteristic(ledCharacteristic);
  ledService.addCharacteristic(buttonCharacteristic);
  BLE.addService(ledService);
 
  ledCharacteristic.writeValue(0);
  buttonCharacteristic.writeValue(0);
 
  BLE.advertise();
  Serial.println("Bluetooth® actif, en attente de connexion...");
}
 
void loop() {
  WiFiClient client = server.available(); //allo
 
  if (client) {
    String currentLine = "";
 
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
 
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
 
            client.println("<!DOCTYPE html>");
            client.println("<html>");
            client.println("<head>");
            client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<style>");
            client.println("body { font-family: Arial; text-align: center; background-color: #f2f2f2; }");
            client.println(".keypad { display: grid; grid-template-columns: repeat(3, 1fr); gap: 15px; max-width: 300px; margin: 40px auto; }");
            client.println(".btn { display: block; padding: 20px; font-size: 28px; background-color: #4CAF50; color: white; text-decoration: none; border-radius: 10px; box-shadow: 0 4px #2e7d32; }");
            client.println(".btn:active { background-color: #45a049; box-shadow: 0 2px #2e7d32; transform: translateY(2px); }");
            client.println("</style>");
            client.println("</head>");
            client.println("<body>");
            client.println("<h2>Contrôle du Clavier</h2>");
            client.println("<h3>Current Password: " + displayedPassword + "</h3>");  // Password display
            client.println("<div class=\"keypad\">");
 
            client.println("<a class=\"btn\" href=\"/1\">1</a>");
            client.println("<a class=\"btn\" href=\"/2\">2</a>");
            client.println("<a class=\"btn\" href=\"/3\">3</a>");
            client.println("<a class=\"btn\" href=\"/4\">4</a>");
            client.println("<a class=\"btn\" href=\"/5\">5</a>");
            client.println("<a class=\"btn\" href=\"/6\">6</a>");
            client.println("<a class=\"btn\" href=\"/7\">7</a>");
            client.println("<a class=\"btn\" href=\"/8\">8</a>");
            client.println("<a class=\"btn\" href=\"/9\">9</a>");
            client.println("<a class=\"btn\" href=\"/star\">*</a>");
            client.println("<a class=\"btn\" href=\"/0\">0</a>");
            client.println("<a class=\"btn\" href=\"/hash\">#</a>");
 
            client.println("</div>");
            client.println("</body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
 
        // === Web keypad input handling ===
        if (currentLine.endsWith("GET /1")) { position = position * 10 + 1; digits = countDigits(position); displayedPassword += "1"; }
        if (currentLine.endsWith("GET /2")) { position = position * 10 + 2; digits = countDigits(position); displayedPassword += "2"; }
        if (currentLine.endsWith("GET /3")) { position = position * 10 + 3; digits = countDigits(position); displayedPassword += "3"; }
        if (currentLine.endsWith("GET /4")) { position = position * 10 + 4; digits = countDigits(position); displayedPassword += "4"; }
        if (currentLine.endsWith("GET /5")) { position = position * 10 + 5; digits = countDigits(position); displayedPassword += "5"; }
        if (currentLine.endsWith("GET /6")) { position = position * 10 + 6; digits = countDigits(position); displayedPassword += "6"; }
        if (currentLine.endsWith("GET /7")) { position = position * 10 + 7; digits = countDigits(position); displayedPassword += "7"; }
        if (currentLine.endsWith("GET /8")) { position = position * 10 + 8; digits = countDigits(position); displayedPassword += "8"; }
        if (currentLine.endsWith("GET /9")) { position = position * 10 + 9; digits = countDigits(position); displayedPassword += "9"; }
        if (currentLine.endsWith("GET /0")) { position = position * 10 + 0; digits = countDigits(position); displayedPassword += "0"; }
 
        // When web '*' pressed: always "send" (print) the entered password, then reset website field and position.
        if (currentLine.endsWith("GET /star")) {
          // Always report attempt (send password even if wrong)
          Serial.print("Web attempt (star) - entered: ");
          Serial.println(position);
 
          // Check password and unlock if correct
          if (position == password) {
            Serial.println("You got it right!");
            unlocked = 1;
          } else {
            Serial.println("Wrong password");
          }
 
          // Reset the entry and the displayed password for the website
          position = 0;
          displayedPassword = "";
        }
 
        // When web '#' pressed: print that an attempt was made and reset field on website
        if (currentLine.endsWith("GET /hash")) {
          Serial.println("Web: attempt made (hash) - entry discarded");
          position = 0;
          displayedPassword = "";
        }
      }
    }
 
    client.stop();
  }
 
  // === Bluetooth ---
  BLE.poll();
 
  char buttonValue = digitalRead(buttonPin);
  bool buttonChanged = (buttonCharacteristic.value() != buttonValue);
 
  if (buttonChanged) {
    ledCharacteristic.writeValue(buttonValue);
    buttonCharacteristic.writeValue(buttonValue);
  }
 
  if (ledCharacteristic.written() || buttonChanged) {
    digitalWrite(ledPin, ledCharacteristic.value());
  }
 
  if (position != prevPosition) {
    prevPosition = position;
    digits = countDigits(position);
    // Print only when '*' is pressed (physical) - (we print on '*' press now)
    if (position == password) {
      Serial.print("Current number: ");
      Serial.println(position);
    }
  }
 
  if (digits > 8) {
    Serial.println("too big");
    position = 0;
    displayedPassword = ""; // reset the field on website as requested
  }
 
  if (digitalRead(sensorPorte) == LOW) {
    Serial.println("front door open");
  }
 
  // === Physical keypad handling ===
  for (int i = 0; i < 12; i++) {
    if (mcp.digitalRead(keypadPins[i]) == LOW) {
      char key = keypadLabels[i];
      Serial.print("Current ");
      Serial.print(key);
      Serial.println(" code");
 
      if (key == '*') {
        // Always send/print the entered password even if wrong
        Serial.print("Physical attempt (star) - entered: ");
        Serial.println(position);
 
        if (position == password) {
          Serial.println("You got it right!");
          unlocked = 1;
        } else {
          Serial.println("Wrong password");
        }
 
        // Reset the entry and the displayed password for the website
        position = 0;
        displayedPassword = "";
      } else if (key == '#') {
        Serial.println("Physical: attempt made (hash) - entry discarded");
        position = 0;
        displayedPassword = "";
      } else {
        int digit = key - '0';
        position = position * 10 + digit;
        displayedPassword += key;  // Add to the displayed password
        Serial.print("Current number: ");
        Serial.println(position);
      }
 
      while (mcp.digitalRead(keypadPins[i]) == LOW) {
        delay(10);
      }
    }
  }
 
  // === Unlock logic ===
  if (unlocked == 1) {
    digitalWrite(lock, HIGH);
    Serial.println("Unlock triggered");
    if (mcp.digitalRead(C12) != LOW) {
      Serial.println("Lock reset");
      position = 0;
      displayedPassword = "";
      unlocked = 0;
      digitalWrite(lock, LOW);
    }
  }
}
 
void printWifiStatus() {
  Serial.print("SSID : ");
  Serial.println(WiFi.SSID());
 
  IPAddress ip = WiFi.localIP();
  Serial.print("Adresse IP : ");
  Serial.println(ip);
 
  long rssi = WiFi.RSSI();
  Serial.print("Puissance du signal (RSSI) : ");
  Serial.print(rssi);
  Serial.println(" dBm");
 
  Serial.print("Ouvrez un navigateur sur : http://");
  Serial.println(ip);
}
