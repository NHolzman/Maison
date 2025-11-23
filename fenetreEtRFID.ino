#include <MFRC522.h>
#define SS_PIN SS
#define RST_PIN D2
MFRC522 mfrc522(SS_PIN, RST_PIN);
#include <MQTT.h>
SYSTEM_MODE(AUTOMATIC);
SerialLogHandler logHandler(LOG_LEVEL_INFO);

int sensorLeft = 13;
int sensorRight = 12;
int sensorStateLeft = 0;
int sensorStateRight = 0;
int lastState = -1;

const char* mqtt_server = "io.adafruit.com";
const int mqtt_port = 1883;
const char* mqtt_user = "2474042";
const char* mqtt_password = "aio_Uytv54eRZayULjJPn0JwRk6ZyJkp";
char client_id[64] = "silver";
const char* publish_topic = "2474042/feeds/led";
const char* subscribe_topic = "2474042/feeds/led";

void callback(char* topic, byte* payload, unsigned int length);
MQTT client(mqtt_server, mqtt_port, callback);

unsigned long prevMillisRFID = 0;
unsigned long prevMillisSensors = 0;
unsigned long prevReconnect = 0;
const long intervalRFID = 5000;
const long intervalSensors = 5000;
const long reconnectInterval = 5000;

String currentMQTT = "";

void setup() {
  pinMode(sensorLeft, INPUT_PULLUP);
  pinMode(sensorRight, INPUT_PULLUP);
  Serial.begin(9600);
  mfrc522.setSPIConfig();
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Scan PICC to see UID and type...");
}

void loop() {
  unsigned long currentMillis = millis();

  if (!client.isConnected()) {
    if (currentMillis - prevReconnect > reconnectInterval) {
      prevReconnect = currentMillis;
      if (client.connect(client_id, mqtt_user, mqtt_password)) {
        client.subscribe(subscribe_topic);
        Serial.println("MQTT Connected");
      } else {
        Serial.println("MQTT Reconnect Failed");
      }
    }
  } else {
    client.loop();
  }

 // if (currentMillis - prevMillisRFID >= intervalRFID) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      Serial.print("Card UID:");
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print(mfrc522.uid.uidByte[i], HEX);
      }
      Serial.println();
      client.publish(publish_topic, (String(mfrc522.uid.uidByte[0], HEX) + String(mfrc522.uid.uidByte[1], HEX) + String(mfrc522.uid.uidByte[2], HEX) + String(mfrc522.uid.uidByte[3], HEX)).toUpperCase().c_str());
      mfrc522.PICC_HaltA();
      //delay(250);
    }
    //prevMillisRFID = currentMillis;
  //}

  //if (currentMillis - prevMillisSensors >= intervalSensors) {
    sensorStateLeft = digitalRead(sensorLeft);
    sensorStateRight = digitalRead(sensorRight);
    int state = (sensorStateLeft << 1 | sensorStateRight);
    if (state != lastState) {
      lastState = state;
      switch (state) {
        case 0:
          Serial.println("Fenêtre fermé (Gauche)");
          Serial.println("Fenêtre fermé (Droite)");
          client.publish(publish_topic, "#WIN_0");
          break;
        case 1:
          Serial.println("Fenêtre fermé (Gauche)");
          Serial.println("Fenêtre ouverte (Droite)");
          client.publish(publish_topic, "#WIN_1");
          break;
        case 2:
          Serial.println("Fenêtre ouverte (Gauche)");
          Serial.println("Fenêtre fermé (Droite)");
          client.publish(publish_topic, "#WIN_2");
          break;
        case 3:
          Serial.println("Fenêtre ouverte (Gauche)");
          Serial.println("Fenêtre ouverte (Droite)");
          client.publish(publish_topic, "#WIN_3");
          break;
      }
      Serial.println("/////");
    }
    //prevMillisSensors = currentMillis;
  //}
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
  currentMQTT = received;
}
