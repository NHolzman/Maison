/*********************************************************************
  BLE + Ultrasonic + Servo
  - Sends BLE ON only once when object < 40 cm
  - Sends BLE OFF only once when object >= 40 cm
  - Servo sweeps only when no object is detected
  - Includes BLE rate-limit protection (cooldown + state logic)
*********************************************************************/

#include <ArduinoBLE.h>
#include <Servo.h>

// ---------- Pin definitions ----------
const int trigPin   = 3;
const int echoPin   = 4;
const int ledPin    = 13;
const int servoPin  = 7; //modified from 11

// ---------- BLE UUIDs ----------
const char* serviceUUID = "19b10000-e8f2-537e-4f6c-d104768a1214";
const char* charUUID    = "19b10001-e8f2-537e-4f6c-d104768a1214";

// ---------- Objects ----------
Servo myServo;
BLEDevice peripheral;
BLECharacteristic ledChar;

// ---------- Servo sweep vars ----------
int angle     = 0;
int direction = 1;
const int stepSize = 7;

// ---------- Detection threshold ----------
const int DETECT_DISTANCE = 40;

// ---------- State tracking ----------
bool lastState = false;
bool firstSend = true;

// ---------- Anti-spam timing control ----------
unsigned long lastSendTime = 0;
const unsigned long MIN_SEND_INTERVAL = 1500; // ms
 
void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ledPin, OUTPUT);

  myServo.attach(servoPin);
  myServo.write(angle);

  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    while (1);
  }

  Serial.println("Scanning for BLE device...");
  BLE.scanForUuid(serviceUUID);
}

void loop() {
  peripheral = BLE.available();

  if (peripheral) {
    Serial.print("Found device: ");
    Serial.println(peripheral.localName());

    if (peripheral.localName() != "LED") return;

    BLE.stopScan();
    controlPeripheral(peripheral);
    BLE.scanForUuid(serviceUUID);
  }
}

void controlPeripheral(BLEDevice peri) {
  Serial.println("Connecting...");

  if (!peri.connect()) {
    Serial.println("Connection failed.");
    return;
  }

  Serial.println("Connected!");

  if (!peri.discoverAttributes()) {
    Serial.println("Discovery failed.");
    peri.disconnect();
    return;
  }

  ledChar = peri.characteristic(charUUID);
  if (!ledChar || !ledChar.canWrite()) {
    Serial.println("Characteristic unavailable.");
    peri.disconnect();
    return;
  }

  Serial.println("Ready — monitoring distance.");

  while (peri.connected()) {

    // ---- Ultrasonic measurement ----
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duration = pulseIn(echoPin, HIGH, 30000);
    long distance = (duration == 0) ? 999 : duration * 0.034 / 2;

    bool objectDetected = (distance <= DETECT_DISTANCE);

    // ---- BLE send only on state change + cooldown ----
    if ((firstSend || objectDetected != lastState) &&
        millis() - lastSendTime > MIN_SEND_INTERVAL) {

      byte value = objectDetected ? 0x01 : 0x00;
      ledChar.writeValue(value);

      Serial.print("BLE SEND → ");
      Serial.println(objectDetected ? "ON" : "OFF");

      lastState = objectDetected;
      firstSend = false;
      lastSendTime = millis();
    }

    // ---- Local LED feedback ----
    digitalWrite(ledPin, objectDetected ? HIGH : LOW);

    // ---- Servo logic ----
    if (!objectDetected) {
      angle += direction * stepSize;
      if (angle >= 165) { angle = 165; direction = -1; }
      if (angle <= 15)   { angle = 15;   direction = 1; }
      myServo.write(angle);
    }

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    delay(10);
  }

  Serial.println("Disconnected.");
}
