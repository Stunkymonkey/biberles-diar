#include <Arduino.h>
#include <ESP_WiFiManager.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#define ONBOARD_LED 2
#define SERVO 13
#define LIGHT_SENSOR 15
#define LIMIT_SWITCH_LEFT 5
#define LIMIT_SWITCH_RIGHT 4

#define MEDIAN_SIZE 50
bool LIGHT_SENSOR_READINGS[MEDIAN_SIZE];
int LIGHT_SENSOR_READINGS_SUM = 0;
int LIGHT_SENSOR_READINGS_INDEX = 0;
float LIGHT_SENSOR_READINGS_AVG = 0;

bool DOOR_status = LOW;
bool LIGHT_SENSOR_status = LOW;

Servo servo;

WebServer server(80);

void setup() {
  Serial.begin(115200);
  while (!Serial); delay(200);
  Serial.print(F("\nStarting Biberles-Diar on ")); Serial.println(ARDUINO_BOARD);
  Serial.println(ESP_WIFIMANAGER_VERSION);
  ESP_WiFiManager ESP_wifiManager("Biberles-Diar");
  ESP_wifiManager.autoConnect("Biberles-Diar");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("Connected. Local IP: "));
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println(ESP_wifiManager.getStatus(WiFi.status()));
  }

  for (int r = 0; r < MEDIAN_SIZE; r++) {
    LIGHT_SENSOR_READINGS[r] = 0;
  }

  pinMode(LIGHT_SENSOR, INPUT);
  pinMode(LIMIT_SWITCH_LEFT, INPUT_PULLDOWN); // connect to 3.3V and pin
  pinMode(LIMIT_SWITCH_RIGHT, INPUT_PULLDOWN); // connect to 3.3V and pin
  pinMode(ONBOARD_LED, OUTPUT);

  digitalWrite(ONBOARD_LED, LOW);

  servo.attach(SERVO);

  server.on("/", HTTP_GET, handle_home);
  server.on("/openDoor", HTTP_POST, handle_open);
  server.on("/closeDoor", HTTP_POST, handle_close);
  server.on("/restart", HTTP_POST, handle_restart);
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");

  if (!MDNS.begin("Biberles-Diar")) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  MDNS.addService("http", "tcp", 80);
}

void loop() {
  server.handleClient();
  read_ligh_sensor();
  handle_motor();
}

void handle_motor() {
  if (DOOR_status){
    // open door
    if (!digitalRead(LIMIT_SWITCH_LEFT)) {
      // not reached limitswitch yet
      servo.write(0);
    } else{
      servo.write(0);
    }
  } else {
    // close door
    if (!digitalRead(LIMIT_SWITCH_RIGHT)) {
      // not reached limitswitch yet
      servo.write(2500);
    } else {
      servo.write(0);
    }
  }
}

void read_ligh_sensor() {
  const unsigned long oneMinute = 60 * 1000UL;
  static unsigned long lastSampleTime = 0 - oneMinute;  // initialize such that a reading is due the first time through loop()

  unsigned long now = millis();
  if (now - lastSampleTime >= oneMinute)
  {
    lastSampleTime += oneMinute;
    LIGHT_SENSOR_READINGS_SUM = LIGHT_SENSOR_READINGS_SUM - LIGHT_SENSOR_READINGS[LIGHT_SENSOR_READINGS_INDEX];  // Remove the oldest entry from the sum
    LIGHT_SENSOR_status = digitalRead(LIGHT_SENSOR);                                                                    // Read the next sensor value
    LIGHT_SENSOR_READINGS[LIGHT_SENSOR_READINGS_INDEX] = LIGHT_SENSOR_status;                                           // Add the newest reading to the window
    LIGHT_SENSOR_READINGS_SUM = LIGHT_SENSOR_READINGS_SUM + LIGHT_SENSOR_status;                                        // Add the newest reading to the sum
    LIGHT_SENSOR_READINGS_INDEX = (LIGHT_SENSOR_READINGS_INDEX + 1) % MEDIAN_SIZE;                               // Increment the index, and wrap to 0 if it exceeds the window size
    LIGHT_SENSOR_READINGS_AVG = float(LIGHT_SENSOR_READINGS_SUM) / float(MEDIAN_SIZE);                           // Divide the sum of the window by the window size for the result
  }
}

void handle_home() {
  server.send(200, "text/html", SendHTML(DOOR_status, LIGHT_SENSOR_status));
}

void handle_open() {
  DOOR_status = HIGH;
  digitalWrite(ONBOARD_LED, HIGH);
  Serial.println("open");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handle_close() {
  DOOR_status = LOW;
  digitalWrite(ONBOARD_LED, LOW);
  Serial.println("close");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handle_restart() {
  Serial.println("restart");
  server.send(200, "text/plain", "Restarting");
  delay(200);
  ESP.restart();
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(uint8_t DOOR_status, uint8_t SENSOR_status) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\"><link rel=\"shortcut icon\" href=\"#\">\n";
  ptr += "<title>Biberles-Diar</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px; background-color: #111;} h1 {color: #fff;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr += ".button {display: block;background-color: #888;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-on {background-color: #181;}\n";
  ptr += ".button-on:active {background-color: #3a3;}\n";
  ptr += ".button-off {background-color: #811;}\n";
  ptr += ".button-off:active {background-color: #a33;}\n";
  ptr += "p {font-size: 14px;color: #aaa;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Biberles-Diar</h1>\n";
  if (SENSOR_status) {
    ptr += "<h3>Lightsensor: Night-time</h3>\n";
  } else {
    ptr += "<h3>Lightsensor: Day-time</h3>\n";
  }

  if (DOOR_status) {
    ptr += "<p>Door Status: Opened</p> <form action=\"/closeDoor\" method=\"POST\"><input class=\"button button-off\" type=\"submit\" value=\"CLOSE\"></form>";
  }
  else {
    ptr += "<p>Door Status: Closed</p> <form action=\"/openDoor\" method=\"POST\"><input class=\"button button-on\" type=\"submit\" value=\"OPEN\"></form>";
  }

  ptr += "<form action=\"/restart\" method=\"POST\"><input type=\"submit\" value=\"Restart\"></form>";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
