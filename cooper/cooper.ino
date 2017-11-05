#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

/*
const char* ssid = "........";
const char* password = "........";
const char* broker = "192.168.1.1"; // receives UDP packets, running homebridge-udp-contactsensor
*/
#include "wifi-password.h"

const uint16_t udp_port_gpio = 8266; // of broker IP address above, for UDP datagrams on input GPIO transitions
const int adc_min_delta = 5; // only report changes if ADC differs by this amount from last poll value
const int adc_poll_interval = 1000; // milliseconds to check after
const int udp_port_adc = 8267; // of broker IP address above, for UDP datagrams on ADC readings

ESP8266WebServer server(80);

const int led = LED_BUILTIN;

struct {
  int pin;
  char *name;
  char *on_path;
  char *off_path;
  int pwm;
} output_gpio[] = {
  { 14, "AC Outlet", "/outlet/on", "/outlet/off", 0 },    // D5
  { 16, "Internal Green LED", "/led/on", "/led/off", 0 }, // D0
  { 15, "Buzzer", "/buzzer/on", "/buzzer/off", 1 },       // D8?
};

struct {
  int pin;
  char *name;
  char on_bytes[2];
  char off_bytes[2];
  int last_state;
} input_gpio[] = {
     // Switch #1 - not available here, hardwired to ADC input through 3-input selector switch (slider, digital switch, light sensor)
  { 4, "Switch #2", { 2, 0xff }, { 2, 0x00 }, 1 }, // D2, blue wire
  { 2, "Switch #3", { 3, 0xff }, { 3, 0x00 }, 1 }, // D4, green wire
  { 5, "Switch #4", { 4, 0xff }, { 4, 0x00 }, 1 }, // D1, red wire - black switch
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

  for (size_t i = 0; i < sizeof(input_gpio) / sizeof(input_gpio[0]); ++i) {
    html += "<tr>";
    html += " <td>" + String(input_gpio[i].name) + "</td>";
    //const char *value = digitalRead(input_gpio[i].pin) ? "Closed" : "Open";
    const char *value = digitalRead(input_gpio[i].pin) ? "Off" : "On";
    html += " <td colspan=\"2\" align=\"center\">" + String(value) + "</td>";
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

      if (output_gpio[i].pwm) {
        // PWM
        if (new_value) {
          // TODO: parse these values from query string, /buzzer/on?freq=200&duty=512
          // duty cycle, in steps up to 1023
          analogWrite(output_gpio[i].pin, 512);
          // frequency, 1 kHz default
          analogWriteFreq(200);
        } else {
          analogWrite(output_gpio[i].pin, 0);
        }
      } else {
        digitalWrite(output_gpio[i].pin, new_value);
      }

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
    Serial.printf("Configuring output: pin #%d = %s\n", output_gpio[i].pin, output_gpio[i].name);
    pinMode(output_gpio[i].pin, OUTPUT);
  }

  for (size_t i = 0; i < sizeof(input_gpio) / sizeof(input_gpio[0]); ++i) {
    pinMode(input_gpio[i].pin, INPUT_PULLUP);
    Serial.printf("Configuring input: pin #%d = %s\n", input_gpio[i].pin, input_gpio[i].name);
  }
}

static WiFiUDP udp;
static int old_analog_value = 0;
static unsigned long analog_last_read_at = 0;

void loop(void){
  server.handleClient();

  // Switch inputs
  for (size_t i = 0; i < sizeof(input_gpio) / sizeof(input_gpio[0]); ++i) {
    int pin = input_gpio[i].pin;
    int new_state = digitalRead(pin);
    int old_state = input_gpio[i].last_state;

    if (new_state != old_state) { // TODO: edge triggering interrupts?
      Serial.printf("Input '%s' changed from %d to %d\n", input_gpio[i].name, old_state, new_state);

      // Send UDP packet to homebridge-udp-contactsensor
      udp.beginPacket(broker, udp_port_gpio);
      if (!new_state) { // active low
        udp.write(input_gpio[i].on_bytes, sizeof input_gpio[i].on_bytes);
      } else {
        udp.write(input_gpio[i].off_bytes, sizeof input_gpio[i].off_bytes);
      }
      udp.endPacket();
    }
    
    input_gpio[i].last_state = new_state;
  }

  // Analog input
  unsigned long duration = millis() - analog_last_read_at;
  if (duration > adc_poll_interval) {
    int analog_value = analogRead(A0);
    int delta = abs(analog_value - old_analog_value);
    if (delta > adc_min_delta) {
      Serial.printf("Analog input changed from %d to %d (delta %d)\n", old_analog_value, analog_value, delta);
      udp.beginPacket(broker, udp_port_adc);
      String s = String(analog_value);
      udp.write(s.c_str(), s.length());
      udp.endPacket();
    }
    old_analog_value = analog_value;
    analog_last_read_at = millis();
  }
}

