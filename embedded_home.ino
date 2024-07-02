/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-websocket-server-arduino/
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// Import required libraries
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <DHT.h>
#include <TroykaMQ.h>

// Replace with your network credentials
const char* ssid = "Oreo Dog";
const char* password = "Oreo20201029?";

// DHT Sensor
#define DHTPIN 33
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// MQ135 Sensor
#define MQ135PIN 32
MQ135 mq135(MQ135PIN);

bool ledState26 = false;
bool ledState25 = false;
const int ledPin26 = 26;
const int ledPin25 = 25;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

unsigned long lastSendTime = 0;
const unsigned long interval = 2000; // 2 seconds

void notifyClients() {
  String message = "{\"gpio26\":\"" + String(ledState26) + "\", \"gpio25\":\"" + String(ledState25) + "\"}";
  ws.textAll(message);
}

void sendSensorData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
  float co2 = mq135.readCO2();
  
  String message = "{\"temperature\":\"" + String(temperature) +
                   "\", \"humidity\":\"" + String(humidity) +
                   "\", \"heatindex\":\"" + String(heatIndex) +
                   "\", \"co2\":\"" + String(co2) + "\"}";
  ws.textAll(message);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle26") == 0) {
      ledState26 = !ledState26;
      digitalWrite(ledPin26, ledState26 ? HIGH : LOW);
      notifyClients();
    } else if (strcmp((char*)data, "toggle25") == 0) {
      ledState25 = !ledState25;
      digitalWrite(ledPin25, ledState25 ? HIGH : LOW);
      notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String& var){
  if (var == "TEMPERATURE") {
    return String(dht.readTemperature());
  } else if (var == "HUMIDITY") {
    return String(dht.readHumidity());
  } else if (var == "HEATINDEX") {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    return String(dht.computeHeatIndex(t, h, false));
  } else if (var == "CO2") {
    return String(mq135.readCO2());
  }
  return String();
}

void initSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  } else {
    Serial.println("SPIFFS mounted successfully");
  }
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

  pinMode(ledPin26, OUTPUT);
  pinMode(ledPin25, OUTPUT);
  digitalWrite(ledPin26, LOW);
  digitalWrite(ledPin25, LOW);

  dht.begin();
  mq135.calibrate(); 
  Serial.print("Ro = ");
  Serial.println(mq135.getRo());
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  initSPIFFS();
  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  // Route for sensor data page
  server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/sensors.html", "text/html", false, processor);
  });

  // Start server
  server.begin();
}

void loop() {
  ws.cleanupClients();
  digitalWrite(ledPin26, ledState26 ? HIGH : LOW);
  digitalWrite(ledPin25, ledState25 ? HIGH : LOW);

  unsigned long currentMillis = millis();
  if (currentMillis - lastSendTime >= interval) {
    lastSendTime = currentMillis;
    sendSensorData();

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    float co2 = mq135.readCO2();
    Serial.print("temp: ");
    Serial.println(temperature);
    Serial.print("hum: ");
    Serial.println(humidity);
    Serial.print("heat: ");
    Serial.println(heatIndex);
    Serial.print("co2: ");
    Serial.println(co2);
  }
}
