#if defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  WebServer server(80);
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  ESP8266WebServer server(80);
#endif

#include <Wire.h>
#include "Adafruit_TCS34725.h"

// Wi-Fi Access Point credentials
const char* ssid = "ESP_Color_Sensor";
const char* password = "12345678";

// Initialize TCS34725 sensor with integration time & gain
// TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// Variable to store RGB values
uint16_t r, g, b, c;
int red, green, blue;

// HTML Web Page with auto-updating JS fetch
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP Color Detector</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background-color: #1a1a1a; color: white; margin: 0; padding: 20px; }
    h1 { font-size: 24px; margin-bottom: 20px; }
    .card { background: #2b2b2b; border-radius: 15px; padding: 20px; max-width: 350px; margin: 0 auto; box-shadow: 0 4px 10px rgba(0,0,0,0.5); }
    .color-box { width: 100%; height: 150px; border-radius: 10px; margin: 20px 0; border: 3px solid #ffffff33; transition: background-color 0.3s; }
    .val-container { font-size: 18px; line-height: 1.8; text-align: left; margin: 0 auto; width: 80%; }
    .hex { font-size: 22px; font-weight: bold; margin-top: 10px; }
  </style>
</head>
<body>
  <div class="card">
    <h1>TCS34725 Detector</h1>
    <div id="box" class="color-box"></div>
    <div class="val-container">
      <div>Red: <b id="rVal">0</b></div>
      <div>Green: <b id="gVal">0</b></div>
      <div>Blue: <b id="bVal">0</b></div>
      <div>Clear: <b id="cVal">0</b></div>
      <div class="hex">HEX: <span id="hexVal">#000000</span></div>
    </div>
  </div>

  <script>
    function updateColor() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('rVal').innerText = data.r;
          document.getElementById('gVal').innerText = data.g;
          document.getElementById('bVal').innerText = data.b;
          document.getElementById('cVal').innerText = data.c;
          document.getElementById('hexVal').innerText = data.hex;
          document.getElementById('box').style.backgroundColor = data.hex;
        })
        .catch(err => console.error(err));
    }
    setInterval(updateColor, 800); // Fetch new color data every 800ms
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

void handleData() {
  tcs.getRawData(&r, &g, &b, &c);
  
  // Normalize RGB values (0-255) relative to Clear lux value
  if (c == 0) {
    red = green = blue = 0;
  } else {
    red = (int)((float)r / c * 255.0);
    green = (int)((float)g / c * 255.0);
    blue = (int)((float)b / c * 255.0);
  }
  
  // Constrain to 0-255 range
  red = constrain(red, 0, 255);
  green = constrain(green, 0, 255);
  blue = constrain(blue, 0, 255);

  char hexStr[8];
  snprintf(hexStr, sizeof(hexStr), "#%02X%02X%02X", red, green, blue);

  String json = "{";
  json += "\"r\":" + String(red) + ",";
  json += "\"g\":" + String(green) + ",";
  json += "\"b\":" + String(blue) + ",";
  json += "\"c\":" + String(c) + ",";
  json += "\"hex\":\"" + String(hexStr) + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!tcs.begin()) {
    Serial.println("Error: TCS34725 sensor not found. Check wiring!");
    while (1);
  }
  Serial.println("TCS34725 sensor initialized.");

  // Configure ESP in Access Point mode
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("Access Point Started. IP address: ");
  Serial.println(myIP);

  // Setup Web Server Routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("HTTP Server Started.");
}

void loop() {
  server.handleClient();
}
