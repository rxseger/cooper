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

  size_t count_gpio = sizeof(output_gpio) / sizeof(output_gpio[0]);
  for (size_t i = 0; i < count_gpio; ++i) {
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
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 1);
}

void setup(void){
  pinMode(led, OUTPUT);
  digitalWrite(led, 1);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");

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
}

void loop(void){
  server.handleClient();
}
