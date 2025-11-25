#include <MQTT.h>
#include "Particle.h"

SYSTEM_MODE(AUTOMATIC);
SerialLogHandler logHandler(LOG_LEVEL_INFO);

int lock = A0;
int buzzer = A2;
int ampoule = D19;
int alerte = 0;

String inputString = "";
bool stringComplete = false;

const char* mqtt_server = "io.adafruit.com";
const int mqtt_port = 1883;
const char* mqtt_user = "cak47";
const char* mqtt_password = "aio_izpf17WdFaECMYOyFMuRTnri0BBa";
char client_id[64] = "rocky";
const char* publish_topic = "cak47/Feeds/Ampoulelock";
const char* subscribe_topic = "cak47/Feeds/Ampoulelock";

void callback(char* topic, byte* payload, unsigned int length);
MQTT client(mqtt_server, mqtt_port, callback);

long lastMsg = 0;
int value = 0;

String currentMQTT = "";  // Will store clean MQTT value

void setup() {
  Serial.begin(9600);
  inputString.reserve(200);
  pinMode(buzzer, OUTPUT);
  pinMode(ampoule, OUTPUT);
  pinMode(lock, OUTPUT);
  digitalWrite(buzzer, LOW);
  digitalWrite(ampoule, LOW);
  digitalWrite(lock, LOW);
  Serial.println("Photon 2 MQTT + Serial Ready");
}

void loop() {
  // === MQTT Connection Handling ===
  if (client.isConnected()) {
    client.loop();
  } else {
    Serial.println("MQTT Disconnected. Reconnecting...");
    if (client.connect(client_id, mqtt_user, mqtt_password)) {
      Serial.println("MQTT Connected!");
      client.subscribe(subscribe_topic);
    } else {
      Serial.println("MQTT Connect Failed. Retry in 5s...");
    }
    delay(5000);
  }

  // === Publish every 5 seconds ===
  //unsigned long now = millis();
  //if (now - lastMsg > 5000) {
    //lastMsg = now;
    //char msg[50];
    //snprintf(msg, sizeof(msg), "Photon 2 Hello #%d", value++);
    //client.publish(publish_topic, msg);
  //}

  // === Process Serial Input ===
  if (stringComplete) {
    inputString.trim();
    Serial.println("Serial: " + inputString);

    if (inputString == "buzzerPulse") {
      Serial.println("> Buzzer Pulse Triggered");
      digitalWrite(buzzer, HIGH);
      delay(200);
      digitalWrite(buzzer, LOW);
    } 
    else if (inputString == "buzzerLong") {
      alerte = 1;
      Serial.println("> Long Buzzer ON");
    } 
    else if (inputString == "buzzerLongOff") {
      alerte = 0;
      Serial.println("> Long Buzzer OFF");
      digitalWrite(buzzer, LOW);
    } 
    else if (inputString == "ampoule") {
      digitalWrite(ampoule, HIGH);
      Serial.println("> Ampoule ON");
    } 
    else if (inputString == "ampouleOff") {
      digitalWrite(ampoule, LOW);
      Serial.println("> Ampoule OFF");
    }

    inputString = "";
    stringComplete = false;
  }

  // === Long Buzzer Alert (Non-blocking inside loop) ===
  if (alerte == 1) {
    digitalWrite(buzzer, HIGH);
    delay(1250);
    digitalWrite(buzzer, LOW);
    delay(625);
  } else if (alerte == 2) {
    digitalWrite(buzzer, HIGH); //earrape
  }

  // === Clear currentMQTT after use (optional, prevents repeat) ===
  // Remove this if you want "dog" to keep triggering until new message
  // currentMQTT = "";
}

void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}

// === MQTT CALLBACK: Clean payload and store ===
void callback(char* topic, byte* payload, unsigned int length) {
  char buffer[length + 1];
  memcpy(buffer, payload, length);
  buffer[length] = '\0';  // Null terminate

  // Convert to String and TRIM newlines/spaces
  String received = String(buffer);
  received.trim();  // <-- Removes \n, \r, spaces

  Serial.print("MQTT Received: [");
  Serial.print(received);
  Serial.println("]");
    if (received == "buzzerPulse") {
      Serial.println("> Buzzer Pulse Triggered");
      digitalWrite(buzzer, HIGH);
      delay(200);
      digitalWrite(buzzer, LOW);
    } 
    else if (received == "buzzerLong") {
      alerte = 1;
      Serial.println("> Long Buzzer ON");
    } 
    else if (received == "buzzerBuzz") {
      alerte = 2;
      Serial.println("> Buzz Buzzer ON");
    } 
    else if (received == "buzzerOff") {
      alerte = 0;
      Serial.println("> Buzzer OFF");
      digitalWrite(buzzer, LOW);
    } 
    else if (received == "ampoule") {
      digitalWrite(ampoule, HIGH);
      Serial.println("> Ampoule ON");
    } 
    else if (received == "ampouleOff") {
      digitalWrite(ampoule, LOW);
      Serial.println("> Ampoule OFF");
    } else if (received == "#unlock") {
      digitalWrite(lock, HIGH);
      Serial.println("> unlock");
    }
  currentMQTT = received;  // Store cleaned value
}
