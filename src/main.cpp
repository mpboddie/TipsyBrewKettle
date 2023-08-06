#include <Arduino.h>

#include <ESPmDNS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "time.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <AsyncElegantOTA.h>
 
#include "config/userSettings.h"
#include "config/pins.h"
#include "htmlData.h"
#include "listLinked.h"   // a linked list library was used, but the naming conflicted with a linked list implementation in the async web server (web server version was missing required functionality), hence list linked

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

OneWire oneWire(ONE_WIRE);
DallasTemperature sensors(&oneWire);

Servo kettleArm;

bool pumpStatus = LOW;
bool heatStatus = LOW;
bool kettleFull = false;
bool pendingHeat = false;
float tempReading = 0;
float lastTempRead = 0;

struct temperature{
   String timeLabel;
   float temp;
};
ListLinked<temperature> tempHistory;

float lastOnHeat = 0;
float lastOnPump = 0;

// Used to debounce the float switch
// Note: at the time of writing I only have one float switch and it is very jittery at full, this may be a fault of the switch, but I added the debounce in case it is common
// Note on the previous note: Apparently it wasn't very jittery, the pin I was using seemed to have something else running on it. I regret not noting which pin it was because it caused a big headache. I am keeping the debounce in though because it may help anyways.
bool lastFloatState = LOW;
float lastDebounceTime = 0;
float debounceDelay = 50;

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

String strLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "Error";
  }
  char timeStr[38];
  strftime(timeStr, 38, "%A, %B %d %Y %H:%M", &timeinfo);
  return timeStr;
}

String justTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "Error";
  }
  char timeStr[9];
  strftime(timeStr, 9, "%H:%M:%S", &timeinfo);
  return timeStr;
}

bool isKettleFull() {   // Returns True if kettle is full, else returns False. Also, sets kettleFull boolean.
  bool currentFloat = digitalRead(FSWITCH);
  //Serial.println(currentFloat);
  if(currentFloat != lastFloatState) {
    lastDebounceTime = millis();
  }

  if((millis() - lastDebounceTime) > debounceDelay) {
    // state changed longer than the debounceDelay, so take it as an actual change in state 
    if(currentFloat == LOW) {
      if(kettleFull) {Serial.println("Kettle is low");}   // Only print on state change
      kettleFull = false;
    } else {
      if(!kettleFull) {Serial.println("Kettle is full");}   // Only print on state change
      kettleFull = true;
    }
  }

  lastFloatState = currentFloat;
  return kettleFull;
}

void pumpOff() {
  if(pumpStatus == HIGH) {    // Only turns pump off if it is currently on (prevents unnecessary relay switching)
    Serial.println("Turning pump off");
    pumpStatus = LOW;
    digitalWrite(PUMP, pumpStatus);
  }
}

bool pumpOn() {
  Serial.println("pumpOn");
  if(isKettleFull() == false) {   // DO NOT OVERFILL THE KETTLE AND FLOOD THE PLACE
    Serial.println("Turning pump on");
    if(pumpStatus == LOW) {   // Only turns on pump if it is currently off (prevents unnecessary relay switching)
      pumpStatus = HIGH;
      lastOnPump = millis();    // Used to track how long the pump has been running as a backup to a faulty float switch
      digitalWrite(PUMP, pumpStatus);
    }
    return true;
  }
  Serial.println("Kettle is too full for pump");
  pumpStatus = LOW;
  return false;
}

void kettleOff() {
  Serial.println("Turning kettle off");
  kettleArm.write(KETTLE_OFF);
  delay(250);
  kettleArm.write(KETTLE_NEUTRAL);
  heatStatus = LOW;
}

bool kettleOn() {
  Serial.println("Turning kettle on");
  if(isKettleFull()) {    // Only turn kettle on if the kettle is full
    kettleArm.write(KETTLE_ON);
    lastOnHeat = millis();    // Used to track how long the heat has been running as a backup to a faulty temp sensor
    delay(250);
    kettleArm.write(KETTLE_NEUTRAL);
    heatStatus = HIGH;
    return true;
  }
  Serial.println("Kettle is not full enough for heat");
  return false;
  
}
String statusJSON() {
  DynamicJsonDocument status(512);
  status["pump"] = pumpStatus;
  status["heat"] = heatStatus;
  status["kettle"] = kettleFull;
  status["pendingheat"] = pendingHeat;
  status["tempreading"] = tempReading;
  status["datetime"] = strLocalTime();
  status["version"] = VERSION;

  String buf;
  serializeJson(status, buf);
  Serial.println(buf);
  return buf;
}

String tempsJSON() {
  DynamicJsonDocument temps(2048);
  for(int i = 0; i < tempHistory.size(); i++) {
    temperature T = tempHistory.get(i);
    temps["data"][i]["timeLabel"] = T.timeLabel;
    temps["data"][i]["temp"] = T.temp;
  }

  String buf;
  serializeJson(temps, buf);
  Serial.println(buf);
  return buf;
}

String SendHTML(uint8_t pumpstat, uint8_t heatstat, uint8_t kettle, float temp){
  char buff[6];
  String tempLabels = "      labels: [";
  String tempData = "        data: [";
  for(int i = 0; i < tempHistory.size(); i++) {
    temperature T = tempHistory.get(i);
    tempLabels += "'";
    tempLabels += T.timeLabel;
    tempLabels += "'";
    dtostrf(T.temp, 3, 1, buff);
    tempData += buff;
    if((i+1) != tempHistory.size()) {
      tempLabels += ", ";
      tempData += ", ";
    }
  }
  tempLabels += "],\n";
  tempData += "],\n";

  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head>\n";
  ptr +="<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>TipsyBrew Kettle</title>\n";
  ptr += styleBlock;
  ptr += addJSlibraries;
  ptr += JSbehaviors;
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<h1>TipsyBrew Kettle</h1>\n";
  ptr +="<span><canvas id=\"tempChart\"></canvas></span>\n";
  ptr +="<script>\n";
  ptr +="  const ctx = document.getElementById('tempChart');\n";
  ptr +="  new Chart(ctx, {\n";
  ptr +="    type: 'line',\n";
  ptr +="    data: {\n";
  ptr += tempLabels;
  ptr +="      datasets: [{\n";
  ptr +="        label: 'degrees C',\n";
  ptr += tempData;
  ptr +="        borderWidth: 3,\n";
  ptr +="        backgroundColor: \"rgba(255,99,132,0.2)\",\n";
  ptr +="        borderColor: \"rgba(255,99,132,1)\",\n";
  ptr +="        pointRadius: 0\n";
  ptr +="      }]\n";
  ptr +="    },\n";
  ptr +="    options: {\n";
  ptr +="      scales: {\n";
  ptr +="        y: {\n";
  ptr +="          beginAtZero: true\n";
  ptr +="        }\n";
  ptr +="      },\n";
  ptr +="      plugins: {\n";
  ptr +="        legend: {\n";
  ptr +="          display: false\n";
  ptr +="        }\n";
  ptr +="      }\n";
  ptr +="    }\n";
  ptr +="  });\n";
  ptr +="</script>\n";
  ptr +="<script>\n";
  ptr +="  const status = JSON.parse('" + statusJSON() + "');\n";
  ptr +="  const temps = JSON.parse('" + tempsJSON() + "');\n";
  ptr +="</script>\n";
  ptr +="<h1>";
  dtostrf(temp, 3, 1, buff);
  ptr +=buff;
  ptr +="&#176;C</h1>\n";
  
  ptr +="<span class=\"leftCol\">\n";
  if(pumpstat)
  { // The pump is running, so the option should be given to stop it
    ptr +="<p>Pump Status: <span id=\"pumpStatus\">ON</span></p><a id=\"pumpLink\" class=\"button button-off\" nohref>OFF</a>\n";
  }
  else
  { // The pump is off, but the button status depends on other conditions
    ptr +="<p>Pump Status: <span id=\"pumpStatus\">OFF</span></p><a id=\"pumpLink\" class=\"button ";
    if(kettle || heatstat) { // Do not allow the user to pump water into kettle if it is full or actively heating
      // TODO: It may be a good idea to prevent pumping if water temp is above specified level
      ptr +="button-off\" nohref>FULL</a>\n";
    } else { // DO allow additional water pumping
      ptr +="button-on\" nohref>FILL</a>\n";
    }
  }
  ptr +="</span>\n";

  ptr +="<span class=\"rightCol\">\n";
  if(heatstat)    // Heat is on, give option to turn it off
  {ptr +="<p>Heat Status: <span id=\"heatStatus\">ON</span></p><a class=\"button button-off\" href=\"/heatoff\">OFF</a>\n";}
  else if(kettle)   // Heat is off and kettle is full, give option to turn it on
  {ptr +="<p>Heat Status: <span id=\"pumpStatus\">OFF</span></p><a class=\"button button-on\" href=\"/heaton\">ON</a>\n";}
  else
  {ptr +="<p>Heat Status: <span id=\"pumpStatus\">OFF</span></p><a class=\"button button-on\" href=\"/fillandheat\">FILL and HEAT</a>\n";}
  ptr +="</span>\n";

  ptr +="<p id=\"datetime\">";
  ptr += strLocalTime();
  ptr +="</p>\n";
  ptr +="<p>TBK version ";
  ptr +=VERSION;
  ptr +="</p>\n";
  ptr +="<div id=\"heap\" style=\"display: none;\">";
  ptr += esp_get_free_heap_size();
  ptr +="</div>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

String processor(const String& var){
  //Serial.println(var);
  if(var == "JSON_DATA"){
    String jsonData = "";
    jsonData += "const status = JSON.parse('" + statusJSON() + "');";
    jsonData += "const temps = JSON.parse('" + tempsJSON() + "');";
    return jsonData;
  }
  return String();
}

/*void handle_OnConnect() {
  server.send(200, "text/html", SendHTML(pumpStatus, heatStatus, kettleFull, tempReading)); 
}

void handle_pumpstart() {
  pumpOn();
  server.send(200, "text/html", SendHTML(pumpStatus, heatStatus, kettleFull, tempReading));
}

void handle_pumpstop() {
  pumpOff();
  server.send(200, "text/html", SendHTML(pumpStatus, heatStatus, kettleFull, tempReading));
}

void handle_heaton() {
  kettleOn();
  server.send(200, "text/html", SendHTML(pumpStatus, heatStatus, kettleFull, tempReading));
}

void handle_heatoff() {
  kettleOff();
  server.send(200, "text/html", SendHTML(pumpStatus, heatStatus, kettleFull, tempReading));
}

void handle_fillandheat() {
  pendingHeat = true;
  pumpOn();
  server.send(200, "text/html", SendHTML(pumpStatus, heatStatus, kettleFull, tempReading));
}*/

void handle_pumptoggle() {
  if(pumpStatus == HIGH) {
    pumpOff();
  } else {
    pumpOn();
  }
}

/*void handle_NotFound(){
  server.send(404, "text/plain", "Meat bag screwed up!");
}*/

void notifyClients() {
  ws.textAll(statusJSON());
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "pumptoggle") == 0) {
      handle_pumptoggle();
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

void setup() {
  Serial.begin(115200);
  delay(100);
  
  pinMode(PUMP, OUTPUT);
  pinMode(FSWITCH, INPUT_PULLUP);
  
  sensors.begin();

  Serial.println("Connecting to ");
  Serial.println(WIFI_NETWORK);

  //connect to your local wi-fi network
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD);

  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  if(!MDNS.begin(HOSTNAME)) {
    Serial.println(F("Error starting mDNS"));
    return;
  }

  kettleArm.attach(SERVO);
  kettleOff();

  //server.on("/", handle_OnConnect);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, F("text/html"), SendHTML(pumpStatus, heatStatus, kettleFull, tempReading));
  });
  //server.on("/pumptoggle", handle_pumptoggle);
  server.on("/pumptoggle", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_pumptoggle();
    request->send(200, F("application/json"), statusJSON());
  });

  /*server.on("/heaton", handle_heaton);
  server.on("/heatoff", handle_heatoff);
  server.on("/fillandheat", handle_fillandheat);*/
  
  //server.onNotFound(handle_NotFound);
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, F("text/plain"), F("Meat bag screwed up!"));
  });
  
  server.begin();
  Serial.println(F("HTTP server started"));

  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  AsyncElegantOTA.setID(OTA_ID);
  AsyncElegantOTA.begin(&server, OTA_USER, OTA_PASS);

}

void loop() {
  // 1. Check water level
  if (isKettleFull()) {           // a. Kettle is full, stop the pump
    pumpOff();
    if (pendingHeat) {
      pendingHeat = false;
      kettleOn();
    }
  } else {                        // b. Kettle is not full, kill the heat if needed
    if (heatStatus == HIGH) {
      kettleOff();
    }
    if(pumpStatus == HIGH && (millis() - lastOnPump) >= timeoutPump) {    // c. Safety check, if the pump fails to turn off for a failure of the float switch, leak, or lack of water, it will time out
      pumpOff();
    }
    // Check if we want to refill the kettle later, that is a lower priority action
  }

  // 2. Check heat
  sensors.requestTemperatures();
  tempReading = sensors.getTempCByIndex(0);
  if(heatStatus == HIGH) {
    if(tempReading >= targetTemp || (millis() - lastOnHeat) >= timeoutHeat) {   // a. Turn off heat if it is at the target temp or if it has been running for too long
      kettleOff();
    }
  }
  if((millis() - lastTempRead) >= TEMP_READ_FREQ) {
    lastTempRead = millis();
    temperature T = {justTime(), tempReading};
    tempHistory.add(T);
    if(tempHistory.size() > NUM_TEMP_READINGS) {
      tempHistory.remove(0);
    }
  }

  ws.cleanupClients();
  
}