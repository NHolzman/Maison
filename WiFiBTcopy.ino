#include "WiFiS3.h"
#include "arduino_secrets.h"
#include <Adafruit_MCP23X17.h>       // MCP23X17
#include <ArduinoBLE.h>              // BLE
#include <ArduinoMqttClient.h>       //mqtt
#include <Wire.h>                    //lcd
#include <LiquidCrystal_I2C.h>       //lcd
LiquidCrystal_I2C lcd(0x27, 16, 2);  //lcd

unsigned long lastAnimationTime = 0;
int animationStep = 0;
bool animating = false;
unsigned long unlockStartTime = 0;

bool mqttWait = false;

// MQTT
WiFiClient wifiClient;

MqttClient mqttClient(wifiClient);
const char broker[] = "io.adafruit.com";
int port = 1883;

const char publishTopic[] = "2474042/feeds/led";
const char subscribeTopic[] = "2474042/feeds/led";

const char username[] = "2474042";
const char adafruitActiveKey[] = "aio_Uytv54eRZayULjJPn0JwRk6ZyJkp";  // Adafruit Active Key (yellow key)
const char clientId[] = "stupidArduinothatdies";                      // Must be unique

const long intervalMQTT = 5000;
unsigned long previousMillisMQTT = 0;
int countMQTT = 0;
// MQTT fin

BLEService ledService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLEByteCharacteristic switchCharacteristic("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLEWrite);

// --- WiFi configuration ---
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
int keyIndex = 0;

int ledPin = LED_BUILTIN;
int status = WL_IDLE_STATUS;
WiFiServer server(80);

// --- Keypad + Lock ---
//#define lock 13
#define C1 11
#define C2 4
#define C3 0

#define C4 10
#define C5 14
#define C6 1

#define C7 9
#define C8 13
#define C9 2

#define C10 8
#define C11 12
#define C12 3

int sensorPorte = 12;

int countDigits(unsigned long number) {
  String str = String(number);
  return str.length();
}

int digits;
int unlocked = 0;
unsigned long password = 3;
unsigned long position = 0;
unsigned long prevPosition = 0;
byte windowStatus = 0;

Adafruit_MCP23X17 mcp;

const int keypadPins[12] = { C1, C2, C3, C4, C5, C6, C7, C8, C9, C10, C11, C12 };
const char keypadLabels[12] = { '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '0', '#' };
String displayedPassword = "";  // To display password input on the web page

// --- MCP debounce variables ---
unsigned long lastDebounceTime[12] = { 0 };
const unsigned long debounceDelay = 200;                         // ms
int digitsToAdd[12] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, 0, -2 };  // 0-9 digits, -1 '*', -2 '#'


byte locked[] = {
  B01110,
  B10001,
  B00001,
  B00001,
  B11111,
  B11011,
  B11011,
  B11111
};

byte notlocked[] = {
  B01110,
  B10001,
  B10001,
  B10001,
  B11111,
  B11011,
  B11011,
  B11111
};

byte squid[] = {
  B00000,
  B00000,
  B01001,
  B10101,
  B10101,
  B10010,
  B00000,
  B00000
};

byte internet[] = {
  B10001,
  B10001,
  B11111,
  B10101,
  B10101,
  B00100,
  B00100,
  B00100,
};

byte ble[] = {
  B00110,
  B00101,
  B10101,
  B01110,
  B01110,
  B10101,
  B00101,
  B00110,
};

byte MQTT[] = {
  B10001,
  B11011,
  B10101,
  B10001,
  B11111,
  B01010,
  B01010,
  B01010
};

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Serial.println("Point 1");
  lcd.init();  // initialize the lcd
  lcd.createChar(1, locked);
  lcd.createChar(2, notlocked);
  lcd.createChar(3, squid);
  lcd.createChar(4, internet);
  lcd.createChar(5, ble);
  lcd.createChar(6, MQTT);
  lcd.backlight();  // turn on the backlight

  lcd.setCursor(4, 0);  // row 0
  lcd.print("Wi-Fi/BLE...");

  lcd.setCursor(0, 1);  // row 1
  lcd.write(1);
  lcd.setCursor(1, 1);
  lcd.write(2);
  lcd.setCursor(2, 1);
  lcd.write(3);
  lcd.setCursor(3, 1);
  lcd.write(4);
  lcd.setCursor(4, 1);
  lcd.write(5);
  lcd.setCursor(5, 1);
  lcd.write(6);



  pinMode(ledPin, OUTPUT);

  // --- BLE setup ---
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
  }

  BLE.setLocalName("LED");
  BLE.setAdvertisedService(ledService);
  ledService.addCharacteristic(switchCharacteristic);
  BLE.addService(ledService);
  switchCharacteristic.writeValue(0);
  BLE.advertise();
  Serial.println("BLE LED Peripheral");

  // --- Wi-Fi setup ---
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Erreur: module WiFi non détecté !");
    while (true)
      ;
  }

  Serial.println("MCP23X17 Test...");
  if (!mcp.begin_I2C(0x20)) {
    Serial.println("Error connecting to MCP23X17.");
  }

  //pinMode(lock, OUTPUT);
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
    delay(3000);
  }
  // Connect to MQTT
  mqttClient.setId(clientId);
  mqttClient.setUsernamePassword(username, adafruitActiveKey);

  Serial.print("Connecting to MQTT broker...");
  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    while (1)
      ;
  }
  Serial.println("You're connected to the MQTT broker!");

  // Subscribe to topic
  mqttClient.subscribe(subscribeTopic);


  server.begin();
  printWifiStatus();

  while (!Serial)
    ;
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  lcd.setCursor(11, 1);
  lcd.print("?");
  lcd.setCursor(12, 1);
  lcd.print("?");
  lcd.setCursor(13, 1);
  lcd.write(6);
  lcd.setCursor(14, 1);
  lcd.write(5);
  lcd.setCursor(15, 1);
  lcd.write(4);
  lcd.setCursor(0, 0);
  lcd.write(2);
  lcd.print("?|");
  lcd.write(3);
  lcd.print("$");
  lcd.setCursor(14, 0);
  lcd.print("|");
  lcd.write(2);
}
//fin du setup

void loop() {
  mqttClient.poll();
  // --- BLE handling (non-blocking) ---
  BLEDevice central = BLE.central();
  BLE.poll();  // Always call frequently to keep BLE alive

  if (central && central.connected()) {
    if (switchCharacteristic.written()) {
      if (switchCharacteristic.value()) {
        //Serial.println("LED on (BLE)");
        digitalWrite(ledPin, HIGH);
        lcd.setCursor(12, 1);
        lcd.print("h");
        lcd.setCursor(12, 1);
        lcd.print(windowStatus);
        mqttClient.beginMessage(publishTopic);
        mqttClient.print("#close");
        mqttClient.endMessage();
        lcd.setCursor(15, 0);
        lcd.write(2);
        lcd.setCursor(14, 0);
        lcd.print("|");
        lcd.setCursor(0, 0);
        lcd.write(2);
        lcd.print("?|");
        lcd.write(3);
        lcd.print("$");
        lcd.setCursor(0, 1);
        lcd.print(ssid);
        lcd.setCursor(13, 1);
        lcd.write(6);
        lcd.setCursor(14, 1);
        lcd.write(5);
        lcd.setCursor(15, 1);
        lcd.write(4);
        lcd.backlight();
      } else {
        //Serial.println("LED off (BLE)");
        lcd.noBacklight();
        digitalWrite(ledPin, LOW);
        lcd.setCursor(12, 1);
        lcd.print("l");
        mqttClient.beginMessage(publishTopic);
        mqttClient.print("#away");
        mqttClient.endMessage();
        lcd.clear();
        lcd.noBacklight();
        position = 0;
      }
    }
  }
  receiveMQTTwindows();
  // --- Wi-Fi web server handling ---
  WiFiClient client = server.available();
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
            client.println("<!DOCTYPE html><html><head>");
            client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<style>");
            client.println("body{font-family:Arial;text-align:center;background-color:#f2f2f2;}");
            client.println(".keypad{display:grid;grid-template-columns:repeat(3,1fr);gap:15px;max-width:300px;margin:40px auto;}");
            client.println(".btn{display:block;padding:20px;font-size:28px;background-color:#4CAF50;color:white;text-decoration:none;border-radius:10px;box-shadow:0 4px #2e7d32;}");
            client.println(".btn:active{background-color:#45a049;box-shadow:0 2px #2e7d32;transform:translateY(2px);}");
            client.println("</style></head><body>");
            client.println("<h2>Mot de passe?</h2>");
            client.println("<h3>Mot de passe saisie: " + displayedPassword + "</h3>");
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
            client.println("</div></body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }

        // --- Handle web keypad presses ---
        if (currentLine.endsWith("GET /1")) {
          position = position * 10 + 1;
          displayedPassword += "1";
        }
        if (currentLine.endsWith("GET /2")) {
          position = position * 10 + 2;
          displayedPassword += "2";
        }
        if (currentLine.endsWith("GET /3")) {
          position = position * 10 + 3;
          displayedPassword += "3";
        }
        if (currentLine.endsWith("GET /4")) {
          position = position * 10 + 4;
          displayedPassword += "4";
        }
        if (currentLine.endsWith("GET /5")) {
          position = position * 10 + 5;
          displayedPassword += "5";
        }
        if (currentLine.endsWith("GET /6")) {
          position = position * 10 + 6;
          displayedPassword += "6";
        }
        if (currentLine.endsWith("GET /7")) {
          position = position * 10 + 7;
          displayedPassword += "7";
        }
        if (currentLine.endsWith("GET /8")) {
          position = position * 10 + 8;
          displayedPassword += "8";
        }
        if (currentLine.endsWith("GET /9")) {
          position = position * 10 + 9;
          displayedPassword += "9";
        }
        if (currentLine.endsWith("GET /0")) {
          position = position * 10 + 0;
          displayedPassword += "0";
        }

        if (currentLine.endsWith("GET /star")) { //web verification
          Serial.print("Web attempt (*): !");
          Serial.println(position);
          mqttClient.beginMessage(publishTopic);
          mqttClient.print(position);
          mqttClient.endMessage();
          mqttWait = true; 
        }

        if (currentLine.endsWith("GET /hash")) {
          Serial.println("Web input cleared (#)");
          position = 0;
          displayedPassword = "";
        }
      }
    }
    client.stop();
  }

  //mqttClient.poll();

  // ---keypad et debouncing
  unsigned long currentMillis = millis();
  if (!animating && unlocked == 0) {  // Skip physical keypad input during animation
    for (int i = 0; i < 12; i++) {
      bool buttonState = !mcp.digitalRead(keypadPins[i]);  // LOW when pressed
      if (buttonState && (currentMillis - lastDebounceTime[i] > debounceDelay)) {
        lastDebounceTime[i] = currentMillis;
        // Digit buttons 0-9
        if (digitsToAdd[i] >= 0 && digitsToAdd[i] <= 9) {
          Serial.print("Physical button ");
          Serial.print(digitsToAdd[i]);
          Serial.println(" pressed");
          position = position * 10 + digitsToAdd[i];
          displayedPassword += keypadLabels[i];  // keep web display consistent
          Serial.print("Current number: ");
          lcd.setCursor(0, 1);
          lcd.print(ssid);
          lcd.setCursor(13, 1);
          lcd.write(6);
          lcd.setCursor(14, 1);
          lcd.write(5);
          lcd.setCursor(15, 1);
          lcd.write(4);
          lcd.backlight();
          lcd.setCursor(0, 0);
          lcd.print("                ");
          lcd.setCursor(15, 0);
          lcd.write(2);
          lcd.setCursor(14, 0);
          lcd.print("|");
          lcd.setCursor(0, 0);
          lcd.write(2);
          lcd.print("?|");
          lcd.write(3);
          lcd.print("$");
          lcd.print(position);
          Serial.println(position);
        }
        // '*' button
        else if (digitsToAdd[i] == -1) {  //pressing star
          Serial.println("Physical button * pressed");
          mqttClient.beginMessage(publishTopic);
          mqttClient.print("!");
          mqttClient.print(position);
          mqttClient.endMessage();
          mqttWait = true;
          Serial.println("MQTT validation enabled");
          position = 0;
          displayedPassword = "";
        }
        // '#' button
        else if (digitsToAdd[i] == -2) {
          Serial.println("Physical button # pressed, reset");
          position = 0;
          lcd.setCursor(5, 0);
          lcd.print("[       ]");
          animationStep = 0;
          animating = true;
          lastAnimationTime = currentMillis;
          displayedPassword = "";
          unlocked = 0;
        }
      }
    }
  }

  if (mqttWait == 1) {
    int messageSize = mqttClient.parseMessage();
    if (messageSize > 0) {
      Serial.print("Received MQTT message: ");
      String message = "";
      while (mqttClient.available()) {
        char c = (char)mqttClient.read();
        message += c;
        Serial.print(c);
      }
      Serial.println();
      // Example: Process the message (e.g., control LED based on message)
      if (message == "#correctPassword") {
        Serial.println("correctPassword");
        unlocked = 1;
        mqttWait = false;
        Serial.println("mqtt is no longer waiting...");
      } else if (message == "#incorrectPassword") {
        Serial.println("incorrectPassword");
        Serial.println("Wrong password");
        lcd.setCursor(0, 0);
        lcd.write(2);
        lcd.print("X|");
        lcd.write(3);
        lcd.print("$");
        lcd.print("[       ]");
        animationStep = 0;
        animating = true;
        lastAnimationTime = currentMillis;
        displayedPassword = "";
        unlocked = 0;
        mqttWait = false;
        Serial.println("mqtt is no longer waiting...");
      }
    }
  }

  // Non-blocking LCD animation
  if (animating && currentMillis - lastAnimationTime >= 250) {
    lastAnimationTime = currentMillis;
    const char* steps[] = { "[-      ]", "[--     ]", "[---    ]", "[----   ]", "[-----  ]", "[------ ]", "[-------]" };
    if (animationStep < 7) {
      lcd.setCursor(5, 0);
      lcd.print(steps[animationStep]);
      animationStep++;
    } else {
      animating = false;
      lcd.setCursor(0, 0);
      lcd.print("                ");
      lcd.setCursor(15, 0);
      lcd.write(2);
      lcd.setCursor(14, 0);
      lcd.print("|");
      lcd.setCursor(0, 0);
      lcd.write(2);
      lcd.print("?|");
      lcd.write(3);
      lcd.print("$");
    }
  }

  //mqttClient.poll();

  // ---overflow, arrête à 8 positions et reset
  if (countDigits(position) > 8) {
    Serial.println("Too many digits, reset");
    position = 0;
    displayedPassword = "";
  }

  // --- Lock handling ---
  if (unlocked == 1) {
    Serial.println("lock unlocked");
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(0, 0);
    lcd.write(1);
    lcd.print("!|");
    lcd.write(3);
    lcd.print("$");
    //digitalWrite(lock, HIGH);
    unlocked = 2;
    unlockStartTime = currentMillis;
  } else if (unlocked == 2 && currentMillis - unlockStartTime >= 10000) {
    //digitalWrite(lock, LOW);
    Serial.println("lock relocking...");
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(15, 0);
    lcd.write(2);
    lcd.setCursor(14, 0);
    lcd.print("|");
    lcd.setCursor(0, 0);
    lcd.write(2);
    lcd.print("?|");
    lcd.write(3);
    lcd.print("$");
    unlocked = 0;
    Serial.println("locked!");
  } else {
  }

  //mqttClient.poll();

  // --- I2C reset
  static unsigned long lastReset = 0;
  unsigned long currentMillisi2c = millis();
  if (currentMillisi2c - lastReset > 15000) {
    resetI2C();
    lastReset = currentMillisi2c;
  }

  //mqttClient.poll();

  // --- Check Wi-Fi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
      //delay(500); //delay temp. removed to see if this is blocking the connection
      Serial.print(".");
    }
    Serial.println("WiFi reconnected!");
  }

  // --- Check MQTT connection
  if (!mqttClient.connected()) {
    Serial.print("MQTT disconnected, reconnecting...");
    if (mqttClient.connect(broker, port)) {
      Serial.println("Reconnected to MQTT broker!");
      mqttClient.subscribe(subscribeTopic);
    } else {
      Serial.print("Failed, error code = ");
      Serial.println(mqttClient.connectError());
    }
  }

  /*
  // ---loop mqtt logic
  unsigned long currentMillisMQTT = millis();
  if (currentMillisMQTT - previousMillisMQTT >= intervalMQTT) {
    previousMillisMQTT = currentMillisMQTT;
    // Publish a message
    mqttClient.beginMessage(publishTopic);
    mqttClient.print("hello ");
    mqttClient.print(countMQTT++);
    mqttClient.endMessage();
  }
*/
  receiveMQTTwindows();
  // ---end of loop mqtt logic
}

void resetI2C() {
  //Serial.println("Resetting I2C bus...");

  // End current I2C session
  Wire.end();
  delay(25);

  // Attempt to release stuck devices by toggling SCL 9 times
  pinMode(SCL, OUTPUT);
  pinMode(SDA, INPUT_PULLUP);  // let SDA float
  for (int i = 0; i < 9; i++) {
    digitalWrite(SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(SCL, LOW);
    delayMicroseconds(5);
  }

  // Generate a STOP condition manually
  pinMode(SDA, OUTPUT);
  digitalWrite(SDA, LOW);
  digitalWrite(SCL, HIGH);
  delayMicroseconds(5);
  digitalWrite(SDA, HIGH);
  delayMicroseconds(5);

  // Reinitialize Wire/I2C
  Wire.begin();
  // Serial.println("I2C bus reset complete!");
}

void printWifiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.println(rssi);
}

void receiveMQTTwindows() {  //petit partie qui detecte le MQTT pour gerer les fenetres type shit
  int messageSize = mqttClient.parseMessage();
  if (messageSize > 0) {
    Serial.print("Received MQTT message: ");
    String message = "";
    while (mqttClient.available()) {
      char c = (char)mqttClient.read();
      message += c;
      Serial.print(c);
    }
    Serial.println();
    // Example: Process the message (e.g., control LED based on message)
    if (message == "#WIN_0") {
      windowStatus = 0;
      digitalWrite(ledPin, HIGH);
      lcd.setCursor(11, 1);
      lcd.print("0");
      Serial.println("this");
      mqttClient.beginMessage(publishTopic);
      mqttClient.print("*WIN_0");
      mqttClient.endMessage();
    } else if (message == "#WIN_1") {
      windowStatus = 1;
      digitalWrite(ledPin, LOW);
      lcd.setCursor(11, 1);
      lcd.print("1");
      Serial.println("that");  //mqtt example
      mqttClient.beginMessage(publishTopic);
      mqttClient.print("*WIN_1");
      mqttClient.endMessage();
    } else if (message == "#WIN_2") {
      windowStatus = 2;
      digitalWrite(ledPin, LOW);
      lcd.setCursor(11, 1);
      lcd.print("2");
      Serial.println("then");  //mqtt example
      mqttClient.beginMessage(publishTopic);
      mqttClient.print("*WIN_2");
      mqttClient.endMessage();
    } else if (message == "#WIN_3") {
      windowStatus = 3;
      digitalWrite(ledPin, LOW);
      lcd.setCursor(11, 1);
      lcd.print("3");
      Serial.println("there");  //mqtt example
      mqttClient.beginMessage(publishTopic);
      mqttClient.print("*WIN_3");
      mqttClient.endMessage();
    }
  }
  //mqttClient.poll();
}