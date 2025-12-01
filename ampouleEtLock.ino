#include <MQTT.h>
#include "Particle.h"

SYSTEM_MODE(AUTOMATIC);
SerialLogHandler logHandler(LOG_LEVEL_INFO);

int lock = A0;
int buzzer = A2;
int ampoule = D19;
int alerte = 0;
int status = 0;

const char* mqtt_server = "io.adafruit.com";
const int mqtt_port = 1883;
const char* mqtt_user = "NHolzman";
const char* mqtt_password = "";
char client_id[64] = "rocky";
const char* publish_topic = "NHolzman/feeds/led";
const char* subscribe_topic = "NHolzman/feeds/led";

void callback(char* topic, byte* payload, unsigned int length);
MQTT client(mqtt_server, mqtt_port, callback);

unsigned long previousMillisUnlock = 0;
const unsigned long intervalUnlock = 3000;

unsigned long buzzerMillis = 0;
bool buzzerState = LOW;

unsigned long buzzerPulseMillis = 0;
bool buzzerPulseActive = false;

const unsigned long BUZZ_LONG_ON = 1250;
const unsigned long BUZZ_LONG_OFF = 625;
const unsigned long BUZZ_SHORT_PULSE = 200;

void setup() {
  Serial.begin(9600);
  pinMode(buzzer, OUTPUT);
  pinMode(ampoule, OUTPUT);
  pinMode(lock, OUTPUT);
  digitalWrite(buzzer, LOW);
  digitalWrite(ampoule, LOW);
  digitalWrite(lock, LOW);
  Serial.println("Photon 2 MQTT + Serial Ready");
}

void loop() {
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

  unsigned long now = millis();

  if (alerte == 2) {
    digitalWrite(buzzer, HIGH);
    digitalWrite(ampoule, HIGH);
  } 
  else if (alerte == 1) {
    if (now - buzzerMillis >= (buzzerState ? BUZZ_LONG_ON : BUZZ_LONG_OFF)) {
      buzzerState = !buzzerState;
      digitalWrite(buzzer, buzzerState);
      digitalWrite(ampoule, !buzzerState);
      buzzerMillis = now;
    }
  } 
  else {
    digitalWrite(ampoule, (status == 1) ? LOW : HIGH);
    if (buzzerPulseActive) {
      if (now - buzzerPulseMillis >= BUZZ_SHORT_PULSE) {
        buzzerPulseActive = false;
        digitalWrite(buzzer, LOW);
      } else {
        digitalWrite(buzzer, HIGH);
      }
    } else {
      digitalWrite(buzzer, LOW);
    }
  }

  if (status == 1) {
    unsigned long currentMillis = now;
    static bool actionDone = false;

    if (!actionDone) {
      digitalWrite(ampoule, LOW);
      digitalWrite(lock, HIGH);
      Serial.println("> correctPassword");
      previousMillisUnlock = currentMillis;
      actionDone = true;
    }

    if (currentMillis - previousMillisUnlock >= intervalUnlock) {
      status = 0;
      actionDone = false;
    }
  }

  if (status == 0 && alerte == 0) {
    digitalWrite(ampoule, HIGH);
    digitalWrite(lock, LOW);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  char buffer[length + 1];
  memcpy(buffer, payload, length);
  buffer[length] = '\0';

  String received = String(buffer);
  received.trim();

  Serial.print("MQTT Received: [");
  Serial.print(received);
  Serial.println("]");

  if (received == "#close") {
    Serial.println("> Buzzer Pulse Triggered");
    buzzerPulseActive = true;
    buzzerPulseMillis = millis();
    digitalWrite(buzzer, HIGH);
  } 
  else if (received == "#incorrectPassword" && alerte != 2) {
    alerte = 1;
    buzzerState = LOW;
    buzzerMillis = millis();
    Serial.println("> Incorrect Password → Flashing Alert");
  } 
  else if (received == "#buzzerBuzz") {
    alerte = 2;
    Serial.println("> Buzz Buzzer + Light ON (continuous)");
  } 
  else if (received == "#correctPassword") {
    alerte = 0;
    status = 1;
    digitalWrite(buzzer, LOW);
    digitalWrite(ampoule, LOW);
    Serial.println("> Correct Password");
  } 
  else if (received == "#ampoule") {
    digitalWrite(ampoule, HIGH);
    Serial.println("> Ampoule ON (manual)");
  }
}
