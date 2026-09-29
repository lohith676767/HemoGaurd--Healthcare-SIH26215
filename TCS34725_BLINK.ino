#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>

// Pin Definitions
const int IR_LED_PIN    = 18;
const int RED_LED_PIN   = 19;
const int GREEN_LED_PIN = 4;

// Non-blocking Timer Variables for LED Blinking Sequence
unsigned long lastLedSwitchTime = 0;
const long ledInterval = 1000; // 1 second per LED
int currentLedState = 0;       // 0 = IR, 1 = Red, 2 = Green

// Color Sensor
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// Web Server
WebServer server(80);
const char* ssid = "ESP32_Color_Sensor";
const char* password = "password123";

// Live UI Web Page
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Dynamic Color Monitor</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background: #121212; color: #fff; padding: 20px; }
    .card { background: #1e1e1e; padding: 25px; border-radius: 16px; display: inline-block; box-shadow: 0 4px 15px rgba(0,0,0,0.6); max-width: 340px; width: 100%; }
    .color-box { width: 180px; height: 180px; border-radius: 12px; margin: 20px auto; border: 4px solid #fff; transition: background-color 0.2s ease; }
    .hex { font-size: 1.8rem; font-weight: bold; letter-spacing: 2px; margin-bottom: 10px; }
    .val { font-size: 1rem; color: #aaa; margin: 6px 0; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Live TCS34725 Detector</h2>
    <div id="box" class="color-box" style="background-color: #000000;"></div>
    <div id="hex" class="hex">#000000</div>
    <div id="rgb" class="val">RGB: (0, 0, 0)</div>
    <div id="clear" class="val">Clear / Intensity: 0</div>
  </div>

  <script>
    // Asynchronously fetch live color data every 200ms
    setInterval(function() {
      fetch('/data')
        .then(res => res.json())
        .then(data => {
          document.getElementById('box').style.backgroundColor = data.hex;
          document.getElementById('hex').innerText = data.hex;
          document.getElementById('rgb').innerText = `RGB: (${data.r}, ${data.g}, ${data.b})`;
          document.getElementById('clear').innerText = `Clear Intensity: ${data.c}`;
        })
        .catch(err => console.error(err));
    }, 200);
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

void handleData() {
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  // Normalize raw 16-bit values to 8-bit RGB (0-255)
  uint8_t red = 0, green = 0, blue = 0;
  if (c > 0) {
    red   = (uint32_t)r * 255 / c;
    green = (uint32_t)g * 255 / c;
    blue  = (uint32_t)b * 255 / c;
  }

  // Format to CSS Hex Code
  char hexColor[8];
  snprintf(hexColor, sizeof(hexColor), "#%02X%02X%02X", red, green, blue);

  // Return JSON data payload
  String json = "{";
  json += "\"r\":" + String(red) + ",";
  json += "\"g\":" + String(green) + ",";
  json += "\"b\":" + String(blue) + ",";
  json += "\"c\":" + String(c) + ",";
  json += "\"hex\":\"" + String(hexColor) + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

// Function to control LED state step without using delay()
void updateLedSequence() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastLedSwitchTime >= ledInterval) {
    lastLedSwitchTime = currentMillis;

    // Turn off all LEDs first
    digitalWrite(IR_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);

    // Turn on current state LED
    if (currentLedState == 0) {
      digitalWrite(IR_LED_PIN, HIGH);
    } else if (currentLedState == 1) {
      digitalWrite(RED_LED_PIN, HIGH);
    } else if (currentLedState == 2) {
      digitalWrite(GREEN_LED_PIN, HIGH);
    }

    // Cycle to next LED (0 -> 1 -> 2 -> 0)
    currentLedState = (currentLedState + 1) % 3;
  }
}

void setup() {
  Serial.begin(115200);

  // Configure LED pins
  pinMode(IR_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  // Initialize TCS34725
  if (!tcs.begin()) {
    Serial.println("Sensor error! Check I2C connections on GPIO 21 (SDA) / GPIO 22 (SCL).");
    while (1);
  }

  // Start Access Point
  WiFi.softAP(ssid, password);
  Serial.print("AP Active. IP: ");
  Serial.println(WiFi.softAPIP());

  // Setup Web Routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();   // Keeps the web server responsive
  updateLedSequence();     // Handles the 1-second LED toggling in background
}
