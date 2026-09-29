// Pin Definitions
const int IR_LED_PIN    = 18;
const int RED_LED_PIN   = 19;
const int GREEN_LED_PIN = 21;

void setup() {
  // Configure pins as outputs
  pinMode(IR_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  // Turn off all LEDs initially
  digitalWrite(IR_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
}

void loop() {
  // 1. Turn on IR LED
  digitalWrite(IR_LED_PIN, HIGH);
  delay(1000);
  digitalWrite(IR_LED_PIN, LOW);

  // 2. Turn on Red LED
  digitalWrite(RED_LED_PIN, HIGH);
  delay(1000);
  digitalWrite(RED_LED_PIN, LOW);

  // 3. Turn on Green LED
  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(1000);
  digitalWrite(GREEN_LED_PIN, LOW);
}
