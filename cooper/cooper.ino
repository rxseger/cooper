#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

/*
const char* ssid = "........";
const char* password = "........";
*/
#include "wifi-password.h"

ESP8266WebServer server(80);

const int led = LED_BUILTIN;

struct {
  int pin;
  char *name;
  char *on_path;
  char *off_path;
} output_gpio[] = {
  { 14, "AC Outlet", "/outlet/on", "/outlet/off" },
  { 16, "Internal Green LED", "/led/on", "/led/off" },
  { 15, "Buzzer", "/buzzer/on", "/buzzer/off" },
};

struct {
  int pin;
  char *name;
  char on_bytes[2];
  char off_bytes[2];
  int last_state;
} input_gpio[] = {
  { 4, "Switch #2", { 2, 0xff }, { 2, 0x00 }, 1 },
  { 2, "Switch #3", { 3, 0xff }, { 3, 0x00 }, 1 },
  { 5, "Switch #4", { 4, 0xff }, { 4, 0x00 }, 1 },
};

void handleRoot() {
  digitalWrite(led, 0);
  String html = "<html>"
"<head>"
"<title>ESP8266</title>"
"</head>"
"<body>"
"<h1>ESP8266</h1>"
"<table>"
" <tr>"
"  <td>Analog Sensor</td>";
  html += " <td>";
  double analog_value = analogRead(0);
  html += String(analog_value);
  html += "</td>";

  for (size_t i = 0; i < sizeof(output_gpio) / sizeof(output_gpio[0]); ++i) {
    html += "<tr>";
    html += " <td>" + String(output_gpio[i].name) + "</td>";
    html += " <td><a href=\"" + String(output_gpio[i].on_path) + "\">On</td>";
    html += " <td><a href=\"" + String(output_gpio[i].off_path) + "\">Off</td>";
    html += "</tr>";    
  }

  html += "</table>"
"</body>"
"</html>";

  server.send(200, "text/html", html);
  

 
  digitalWrite(led, 1);
}

void handleNotFound(){
  digitalWrite(led, 0);

  String uri = server.uri();
  // Request to change GPIO?
  for (size_t i = 0; i < sizeof(output_gpio) / sizeof(output_gpio[0]); ++i) {
    bool new_value;
    bool change_value = false;

    if (uri.equals(output_gpio[i].on_path)) {
      new_value = true;
      change_value = true;
    }

    if (uri.equals(output_gpio[i].off_path)) {
      new_value = false;
      change_value = true;
    }

    if (change_value) {
      Serial.println("Request via " + server.uri() + " to change value of GPIO output: " + String(output_gpio[i].name) + " to " + String(new_value));

      // TODO: pwm
      // analogWrite

      int pin = output_gpio[i].pin;
      digitalWrite(pin, new_value);

      server.send(200, "text/plain", "Request to change value of GPIO output: " + String(output_gpio[i].name) + " to " + String(new_value));
      
      return;
    }
  }
  
  String message = "Not found: " + server.uri(); + "\n\n";

  server.send(404, "text/plain", message);

  digitalWrite(led, 1);
}

void setup(void){
  pinMode(led, OUTPUT);
  digitalWrite(led, 1);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("\nWaiting to connect to Wi-Fi...");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/inline", [](){
    server.send(200, "text/plain", "this works as well");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  // Setup pins
  for (size_t i = 0; i < sizeof(output_gpio) / sizeof(output_gpio[0]); ++i) {
    int pin = output_gpio[i].pin;
    pinMode(pin, OUTPUT);
  }

  for (size_t i = 0; i < sizeof(input_gpio) / sizeof(input_gpio[0]); ++i) {
    int pin = input_gpio[i].pin;
    pinMode(pin, INPUT);
  }
}

void loop(void){
  server.handleClient();

  for (size_t i = 0; i < sizeof(input_gpio) / sizeof(input_gpio[0]); ++i) {
    int pin = input_gpio[i].pin;
    int new_state = digitalRead(pin);
    int old_state = input_gpio[i].last_state;

    if (new_state != old_state) { // TODO: edge triggering interrupts?
      Serial.println("Switch " + String(i) + " changed from " + String(old_state) + " to " + String(new_state));
      // TODO: send UDP packet
    }
    
    input_gpio[i].last_state = new_state;
  }
}

