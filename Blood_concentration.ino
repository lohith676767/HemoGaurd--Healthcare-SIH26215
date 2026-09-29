#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <math.h>

// Pin Definitions
const int IR_LED_PIN    = 18;
const int RED_LED_PIN   = 19;
const int GREEN_LED_PIN = 4;

// Non-blocking LED Sequencing
unsigned long lastLedSwitchTime = 0;
const long ledInterval = 1000; // 1 second per LED
int currentLedState = 0;       // 0 = IR, 1 = Red, 2 = Green

// Calibration & Beer-Lambert Variables
uint16_t water_I0 = 0;          // Reference blank intensity (I_0)
bool isCalibrated = false;      
const float K_FACTOR = 100.0;   // Scaling factor (k) to convert Absorbance to Concentration %

// Hardware Sensor & Server
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
WebServer server(80);

const char* ssid = "ESP32_Color_Sensor";
const char* password = "password123";

// Web UI Template
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Blood Concentration Photometer</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background: #121212; color: #fff; padding: 20px; margin: 0; }
    .container { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; max-width: 750px; margin: 0 auto; }
    .card { background: #1e1e1e; padding: 20px; border-radius: 16px; box-shadow: 0 4px 15px rgba(0,0,0,0.6); flex: 1; min-width: 280px; }
    .color-box { width: 160px; height: 160px; border-radius: 12px; margin: 15px auto; border: 3px solid #fff; transition: background-color 0.2s ease; }
    .hex { font-size: 1.5rem; font-weight: bold; letter-spacing: 2px; }
    .val-large { font-size: 2.2rem; font-weight: bold; color: #00e676; margin: 10px 0; }
    .val-sub { font-size: 1rem; color: #aaa; margin: 5px 0; }
    button { background: #007bff; color: white; border: none; padding: 12px 24px; font-size: 1rem; border-radius: 8px; cursor: pointer; transition: 0.2s; font-weight: bold; }
    button:hover { background: #0056b3; }
    button:active { transform: scale(0.98); }
    .status { font-size: 0.9rem; color: #ffca28; margin-top: 10px; }
  </style>
</head>
<body>
  <h2>Blood Concentration Photometer (Beer-Lambert)</h2>
  
  <div style="margin-bottom: 20px;">
    <button onclick="calibrateWater()">Calibrate with Water (I&#8320;)</button>
    <div id="cal-status" class="status">Status: Not Calibrated (Place Water Cuvette)</div>
  </div>

  <div class="container">
    <!-- Color Box Card -->
    <div class="card">
      <h3>Detected Color</h3>
      <div id="box" class="color-box" style="background-color: #000000;"></div>
      <div id="hex" class="hex">#000000</div>
      <div id="rgb" class="val-sub">RGB: (0, 0, 0)</div>
    </div>

    <!-- Concentration & Absorbance Card -->
    <div class="card">
      <h3>Beer-Lambert Analysis</h3>
      <div class="val-sub">Absorbance (A)</div>
      <div id="absorbance" class="val-large">0.000</div>
      <div class="val-sub">Relative Concentration</div>
      <div id="concentration" class="val-large" style="color: #ff5252;">0.00 %</div>
      <div id="raw-c" class="val-sub">Light Intensity (I): 0</div>
    </div>
  </div>

  <script>
    function calibrateWater() {
      fetch('/calibrate')
        .then(res => res.json())
        .then(data => {
          document.getElementById('cal-status').innerText = `Calibrated! Baseline I\u2080: ${data.i0}`;
          document.getElementById('cal-status').style.color = '#00e676';
        })
        .catch(err => alert('Calibration failed! Check connection.'));
    }

    setInterval(function() {
      fetch('/data')
        .then(res => res.json())
        .then(data => {
          document.getElementById('box').style.backgroundColor = data.hex;
          document.getElementById('hex').innerText = data.hex;
          document.getElementById('rgb').innerText = `RGB: (${data.r}, ${data.g}, ${data.b})`;
          document.getElementById('absorbance').innerText = data.absorbance.toFixed(3);
          document.getElementById('concentration').innerText = data.concentration.toFixed(2) + ' %';
          document.getElementById('raw-c').innerText = `Light Intensity (I): ${data.c}`;
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

void handleCalibrate() {
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);
  
  water_I0 = c; // Save baseline intensity I_0
  isCalibrated = true;

  String json = "{\"status\":\"ok\",\"i0\":" + String(water_I0) + "}";
  server.send(200, "application/json", json);
}

void handleData() {
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  // 1. Color normalization
  uint8_t red = 0, green = 0, blue = 0;
  if (c > 0) {
    red   = (uint32_t)r * 255 / c;
    green = (uint32_t)g * 255 / c;
    blue  = (uint32_t)b * 255 / c;
  }

  char hexColor[8];
  snprintf(hexColor, sizeof(hexColor), "#%02X%02X%02X", red, green, blue);

  // 2. Beer-Lambert Calculation: A = log10(I_0 / I)
  float absorbance = 0.0;
  float concentration = 0.0;

  if (isCalibrated && water_I0 > 0 && c > 0) {
    if (c < water_I0) {
      absorbance = log10((float)water_I0 / (float)c);
    } else {
      absorbance = 0.0; // Sample clearer or equal to baseline water
    }
    concentration = absorbance * K_FACTOR;
  }

  // Construct JSON response
  String json = "{";
  json += "\"r\":" + String(red) + ",";
  json += "\"g\":" + String(green) + ",";
  json += "\"b\":" + String(blue) + ",";
  json += "\"c\":" + String(c) + ",";
  json += "\"hex\":\"" + String(hexColor) + "\",";
  json += "\"absorbance\":" + String(absorbance, 4) + ",";
  json += "\"concentration\":" + String(concentration, 2);
  json += "}";

  server.send(200, "application/json", json);
}

void updateLedSequence() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastLedSwitchTime >= ledInterval) {
    lastLedSwitchTime = currentMillis;

    digitalWrite(IR_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);

    if (currentLedState == 0)      digitalWrite(IR_LED_PIN, HIGH);
    else if (currentLedState == 1) digitalWrite(RED_LED_PIN, HIGH);
    else if (currentLedState == 2) digitalWrite(GREEN_LED_PIN, HIGH);

    currentLedState = (currentLedState + 1) % 3;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(IR_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  if (!tcs.begin()) {
    Serial.println("Sensor connection failed! Check SDA (21) and SCL (22).");
    while (1);
  }

  WiFi.softAP(ssid, password);
  Serial.print("Access Point IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/calibrate", handleCalibrate);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();
  updateLedSequence();
}
