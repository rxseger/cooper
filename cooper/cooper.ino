void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW); // on (active-low)
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
}
