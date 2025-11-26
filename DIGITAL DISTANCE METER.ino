/************************************************************************************
   🌐 STC CREATIVE CLUB – SMART DIGITAL DISTANCE METER (WEB + LCD + BUZZER + PNG EXPORT)
   -----------------------------------------------------------------
   PROJECT : STC – DIGITAL DISTANCE MEASUREMENT TOOL (WiFi + LCD)
   BOARD   : ESP8266 NodeMCU
   SENSOR  : Ultrasonic HC-SR04
   DISPLAY : 16x2 I2C LCD
   INPUT   : 1 x PUSH BUTTON (SAVE)
   OUTPUT  : 1 x BUZZER (BEEP ON SAVE)
   AUTHOR  : PRAVIN SINH RANA
   CONTACT : 9313057803
   INSTAGRAM : @S.T.C_CREATIVE_CLUB
************************************************************************************/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/* -------------------------------------------------------------------
   🔹 LCD CONFIGURATION (16x2 I2C)
   ------------------------------------------------------------------- */
LiquidCrystal_I2C lcd(0x27, 16, 2);  // If LCD not working, try 0x3F instead of 0x27

/* -------------------------------------------------------------------
   🔹 PIN DEFINITIONS (ESP8266 NODEMCU)
   ------------------------------------------------------------------- */
// 👉 Yahi pin par aapka button hona chahiye (button → pin & GND)
const int TRIG_PIN    = D5;   // Ultrasonic Trigger
const int ECHO_PIN    = D6;   // Ultrasonic Echo
const int BUTTON_PIN  = D7;   // Save Button (to GND, INPUT_PULLUP)
const int BUZZER_PIN  = D0;   // Buzzer pin (active HIGH)

/* -------------------------------------------------------------------
   🔹 WIFI AP CONFIGURATION
   ------------------------------------------------------------------- */
const char* ap_ssid = "STC_DISTANCE_AP";
const char* ap_pass = "stc12345";  // min 8 chars

IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

ESP8266WebServer server(80);

/* -------------------------------------------------------------------
   🔹 GLOBAL VARIABLES
   ------------------------------------------------------------------- */
float liveDistanceCm   = 0.0;   // Current live distance
float savedDistanceCm  = 0.0;   // Last saved distance
bool  hasSavedDistance = false; // Flag: saved value available

// Saved list (RAM)
const int MAX_SAVED = 20;
float savedList[MAX_SAVED];
int   savedCount = 0;

// Simple button state (no complex debounce)
bool lastButtonPressed = false;  // previous loop state

/* -------------------------------------------------------------------
   🔹 FORWARD DECLARATIONS
   ------------------------------------------------------------------- */
void setupPins();
void setupLCD();
void setupSerialBranding();
void setupWiFiAP();
float readDistanceCm();
void updateLCD();
void handleButton();
void saveCurrentDistance();
void clearAllData();
void handleRootPage();
void handleDataAPI();
void handleSaveAPI();
void handleClearAPI();
void beepBuzzer(unsigned int durationMs = 120);

/* -------------------------------------------------------------------
   🔹 HTML PAGE (WEB DASHBOARD + PNG & CLEAR)
   ------------------------------------------------------------------- */
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>STC Distance Meter</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  body{
    font-family: Arial, sans-serif;
    background: #0f172a;
    color: #e5e7eb;
    margin:0;
    padding:0;
  }
  header{
    background:#111827;
    padding:12px 16px;
    text-align:center;
    font-size:18px;
    font-weight:bold;
    letter-spacing:1px;
  }
  header span{
    color:#22c55e;
  }
  .notify{
    display:none;
    padding:8px 12px;
    text-align:center;
    font-size:13px;
    background:#22c55e;
    color:#0b1120;
  }
  .container{
    padding:16px;
  }
  .card{
    background:#1f2937;
    border-radius:10px;
    padding:16px;
    margin-bottom:16px;
    box-shadow:0 2px 8px rgba(0,0,0,0.4);
  }
  .card-main .label,
  .card-main .value{
    text-align:center;
  }
  .label{
    font-size:14px;
    color:#9ca3af;
  }
  .value{
    font-size:40px;
    font-weight:bold;
    margin-top:8px;
  }
  .unit{
    font-size:20px;
    color:#a5b4fc;
    margin-left:4px;
  }
  button{
    padding:10px 16px;
    border:none;
    border-radius:6px;
    font-size:14px;
    cursor:pointer;
    margin-top:10px;
  }
  .btn-save{
    background:#22c55e;
    color:#0b1120;
    font-weight:600;
    width:100%;
  }
  .btn-save:active{
    transform:scale(0.98);
  }
  .btn-download{
    background:#38bdf8;
    color:#0b1120;
    font-weight:600;
    width:100%;
    margin-top:8px;
  }
  .btn-download:active{
    transform:scale(0.98);
  }
  .btn-clear{
    background:#f97316;
    color:#0b1120;
    font-weight:600;
    width:100%;
    margin-top:8px;
  }
  .btn-clear:active{
    transform:scale(0.98);
  }
  table{
    width:100%;
    border-collapse:collapse;
    margin-top:8px;
    font-size:14px;
  }
  th,td{
    border-bottom:1px solid #374151;
    padding:6px 4px;
    text-align:left;
  }
  th{
    background:#111827;
  }
  .badge{
    display:inline-block;
    padding:2px 8px;
    border-radius:999px;
    font-size:11px;
    background:#4b5563;
    color:#e5e7eb;
  }
  footer{
    text-align:center;
    font-size:11px;
    color:#6b7280;
    padding:8px 0 12px 0;
  }
</style>
</head>
<body>
<header>
  STC CREATIVE CLUB – <span>Digital Distance Meter</span>
</header>

<div id="notify" class="notify">Distance Saved ✅</div>

<div class="container">

  <div class="card card-main">
    <div class="label">Live Distance</div>
    <div class="value">
      <span id="liveVal">--</span><span class="unit">cm</span>
    </div>
    <div class="label" style="margin-top:6px;">
      Last Saved:
      <span id="savedVal" class="badge">-- cm</span>
    </div>
    <button class="btn-save" onclick="saveDistance()">
      📍 Save Current Distance
    </button>
  </div>

  <div class="card">
    <div class="label">Saved Measurements (Max 20)</div>
    <table>
      <thead>
        <tr>
          <th>#</th>
          <th>Distance (cm)</th>
        </tr>
      </thead>
      <tbody id="savedBody">
        <tr><td colspan="2">No saved data yet.</td></tr>
      </tbody>
    </table>

    <button class="btn-download" onclick="downloadPNG()">
      📄 Download PNG Report
    </button>
    <button class="btn-clear" onclick="clearData()">
      🗑 Clear All Data
    </button>
  </div>

</div>

<footer>
  STC Creative Club • Distance Tool • ESP8266 WiFi AP • 192.168.4.1
</footer>

<script>
function showNotify(msg){
  const n = document.getElementById('notify');
  n.textContent = msg;
  n.style.display = 'block';
  n.style.opacity = '1';
  setTimeout(() => {
    n.style.opacity = '0';
  }, 1200);
  setTimeout(() => {
    n.style.display = 'none';
  }, 1500);
}

function fetchData(){
  fetch('/data')
    .then(r => r.json())
    .then(d => {
      const liveEl = document.getElementById('liveVal');
      if(d.live < 0){
        liveEl.textContent = 'No Obj';
      } else if(d.live > 400){
        liveEl.textContent = '>400';
      } else {
        liveEl.textContent = d.live.toFixed(1);
      }

      const savedBadge = document.getElementById('savedVal');
      if(d.hasSaved){
        savedBadge.textContent = d.lastSaved.toFixed(1) + ' cm';
      } else {
        savedBadge.textContent = '-- cm';
      }

      const body = document.getElementById('savedBody');
      body.innerHTML = '';
      if(d.count === 0){
        const row = document.createElement('tr');
        const cell = document.createElement('td');
        cell.colSpan = 2;
        cell.textContent = 'No saved data yet.';
        row.appendChild(cell);
        body.appendChild(row);
      }else{
        d.saved.forEach((val, idx) => {
          const row = document.createElement('tr');
          const c1 = document.createElement('td');
          const c2 = document.createElement('td');
          c1.textContent = idx + 1;
          c2.textContent = val.toFixed(1) + ' cm';
          row.appendChild(c1);
          row.appendChild(c2);
          body.appendChild(row);
        });
      }
    })
    .catch(err => {
      console.log('fetch error', err);
    });
}

function saveDistance(){
  fetch('/save')
    .then(r => r.text())
    .then(t => {
      if(t === 'OK'){
        showNotify('Distance Saved ✅');
      } else {
        showNotify('Invalid Distance ❌');
      }
      fetchData();
    })
    .catch(err => console.log('save error', err));
}

function clearData(){
  fetch('/clear')
    .then(r => r.text())
    .then(t => {
      showNotify('All data cleared 🗑');
      fetchData();
    })
    .catch(err => console.log('clear error', err));
}

/* 🔽 PNG DOWNLOAD – Draw data on canvas & download as PNG */
function downloadPNG(){
  fetch('/data')
    .then(r => r.json())
    .then(d => {
      const rows = d.count || 0;
      const width = 700;
      const rowHeight = 30;
      const headerHeight = 80;
      const bottomMargin = 40;
      const height = headerHeight + rowHeight * (rows + 2) + bottomMargin;

      const canvas = document.createElement('canvas');
      canvas.width = width;
      canvas.height = height;
      const ctx = canvas.getContext('2d');

      // Background
      ctx.fillStyle = '#0f172a';
      ctx.fillRect(0, 0, width, height);

      // Title (center)
      const title = 'STC CREATIVE CLUB – Digital Distance Report';
      ctx.font = '20px Arial';
      ctx.fillStyle = '#22c55e';
      let titleWidth = ctx.measureText(title).width;
      ctx.fillText(title, (width - titleWidth) / 2, 30);

      // Subtitle (center)
      const subtitle = 'Saved Measurements (cm)';
      ctx.font = '14px Arial';
      ctx.fillStyle = '#e5e7eb';
      let subWidth = ctx.measureText(subtitle).width;
      ctx.fillText(subtitle, (width - subWidth) / 2, 50);

      // Table header
      const marginX = 40;
      const startXIndex = marginX + 10;
      const startXValue = marginX + 120;
      let y = headerHeight;
      ctx.fillStyle = '#1f2937';
      ctx.fillRect(marginX, y - 22, width - 2*marginX, rowHeight);
      ctx.fillStyle = '#e5e7eb';
      ctx.font = '14px Arial';
      ctx.fillText('#', startXIndex, y - 2);
      ctx.fillText('Distance (cm)', startXValue, y - 2);

      // Rows
      for(let i = 0; i < rows; i++){
        y += rowHeight;
        const isEven = (i % 2 === 0);
        ctx.fillStyle = isEven ? '#111827' : '#020617';
        ctx.fillRect(marginX, y - 22, width - 2*marginX, rowHeight);

        ctx.fillStyle = '#e5e7eb';
        ctx.fillText(String(i+1), startXIndex, y - 2);
        ctx.fillText(d.saved[i].toFixed(1) + ' cm', startXValue, y - 2);
      }

      // If no data, show message
      if(rows === 0){
        ctx.fillStyle = '#e5e7eb';
        ctx.fillText('No saved data yet.', marginX, headerHeight + 30);
      }

      // Bottom total line
      const totalText = 'Total Entries: ' + rows;
      ctx.font = '14px Arial';
      ctx.fillStyle = '#e5e7eb';
      let totalWidth = ctx.measureText(totalText).width;
      ctx.fillText(totalText, (width - totalWidth) / 2, height - 15);

      // Download
      const link = document.createElement('a');
      link.download = 'STC_Distance_Data.png';
      link.href = canvas.toDataURL('image/png');
      link.click();
    })
    .catch(err => {
      console.log('PNG error', err);
    });
}

setInterval(fetchData, 1000);
window.onload = fetchData;
</script>

</body>
</html>
)rawliteral";

/* ===================================================================
   🔹 SETUP
   =================================================================== */
void setup() {
  setupPins();
  setupLCD();

  Serial.begin(115200);
  delay(200);
  setupSerialBranding();

  setupWiFiAP();

  server.on("/", handleRootPage);
  server.on("/data", handleDataAPI);
  server.on("/save", handleSaveAPI);
  server.on("/clear", handleClearAPI);
  server.begin();

  Serial.println("[STC_WIFI] Web Server started at http://192.168.4.1");
}

/* ===================================================================
   🔹 LOOP
   =================================================================== */
void loop() {
  server.handleClient();

  liveDistanceCm = readDistanceCm();
  updateLCD();

  Serial.print("[STC_DISTANCE] Live: ");
  Serial.print(liveDistanceCm);
  Serial.println(" cm");

  handleButton();   // physical button

  delay(200);
}

/* ===================================================================
   🔹 FUNCTIONS
   =================================================================== */

void setupPins() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP); // button → GND
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);     // buzzer OFF
}

void setupLCD() {
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(" STC CREATIVE ");
  lcd.setCursor(0, 1);
  lcd.print("DISTANCE METER");
  delay(1500);
  lcd.clear();
}

void setupSerialBranding() {
  Serial.println("************************************************");
  Serial.println("*         STC CREATIVE CLUB – PROJECT          *");
  Serial.println("*     DIGITAL DISTANCE MEASUREMENT TOOL        *");
  Serial.println("*   BOARD  : ESP8266 NODEMCU                   *");
  Serial.println("*   SENSOR : ULTRASONIC HC-SR04                *");
  Serial.println("*   LCD    : 16x2 I2C                          *");
  Serial.println("*   AUTHOR : PRAVIN SINH RANA                  *");
  Serial.println("*   CONTACT: 9313057803                        *");
  Serial.println("*   INSTA  : @S.T.C_CREATIVE_CLUB              *");
  Serial.println("************************************************");
  Serial.println();
}

void setupWiFiAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(ap_ssid, ap_pass);

  Serial.print("[STC_WIFI] AP SSID : ");
  Serial.println(ap_ssid);
  Serial.print("[STC_WIFI] AP PASS : ");
  Serial.println(ap_pass);
  Serial.print("[STC_WIFI] AP IP   : ");
  Serial.println(WiFi.softAPIP());
}

float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms

  if (duration == 0) {
    return -1; // no echo
  }

  float distanceCm = (duration / 2.0) / 29.1;
  return distanceCm;
}

void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("Live:           ");
  lcd.setCursor(6, 0);

  if (liveDistanceCm < 0) {
    lcd.print("No Obj ");
  } else if (liveDistanceCm > 400) {
    lcd.print(">400cm");
  } else {
    lcd.print(liveDistanceCm, 1);
    lcd.print("cm ");
  }

  lcd.setCursor(0, 1);
  if (hasSavedDistance) {
    lcd.print("Saved:");
    lcd.setCursor(7, 1);
    lcd.print(savedDistanceCm, 1);
    lcd.print("cm   ");
  } else {
    lcd.print("Press Btn ToSave");
  }
}

/* 🔔 BUZZER */
void beepBuzzer(unsigned int durationMs) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(BUZZER_PIN, LOW);
}

/* 🔘 SIMPLE BUTTON HANDLER (NO COMPLEX DEBOUNCE) */
void handleButton() {
  bool pressedNow = (digitalRead(BUTTON_PIN) == LOW);  // active LOW

  // For debug
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    Serial.print("[STC_BUTTON] Raw state = ");
    Serial.println(pressedNow ? "LOW (pressed?)" : "HIGH (released)");
    lastPrint = millis();
  }

  // Edge detect: HIGH → LOW = press
  if (pressedNow && !lastButtonPressed) {
    Serial.println("[STC_BUTTON] Press detected from physical button");

    if (liveDistanceCm >= 0 && liveDistanceCm <= 400) {
      saveCurrentDistance();
    } else {
      Serial.println("[STC_SAVE] Invalid distance, not saved");
      beepBuzzer(50); // small error beep
    }
  }

  lastButtonPressed = pressedNow;
}

void saveCurrentDistance() {
  savedDistanceCm  = liveDistanceCm;
  hasSavedDistance = true;

  Serial.print("[STC_SAVE] Saved Distance: ");
  Serial.print(savedDistanceCm);
  Serial.println(" cm");

  if (savedCount < MAX_SAVED) {
    savedList[savedCount] = savedDistanceCm;
    savedCount++;
  } else {
    Serial.println("[STC_SAVE] List full (max 20 entries)");
  }

  // Beep on every successful save
  beepBuzzer(120);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Distance Saved! ");
  lcd.setCursor(0, 1);
  lcd.print(savedDistanceCm, 1);
  lcd.print(" cm        ");
  delay(800);
  lcd.clear();
}

void clearAllData() {
  savedCount = 0;
  hasSavedDistance = false;
  savedDistanceCm = 0;
  for (int i = 0; i < MAX_SAVED; i++) {
    savedList[i] = 0;
  }
  Serial.println("[STC_CLEAR] All saved data cleared");
}

void handleRootPage() {
  server.send(200, "text/html", MAIN_page);
}

void handleDataAPI() {
  String json = "{";

  json += "\"live\":";
  json += String(liveDistanceCm, 1);
  json += ",";

  json += "\"hasSaved\":";
  json += hasSavedDistance ? "true" : "false";
  json += ",";

  json += "\"lastSaved\":";
  json += String(savedDistanceCm, 1);
  json += ",";

  json += "\"count\":";
  json += String(savedCount);
  json += ",";

  json += "\"saved\":[";
  for (int i = 0; i < savedCount; i++) {
    json += String(savedList[i], 1);
    if (i < savedCount - 1) json += ",";
  }
  json += "]";

  json += "}";

  server.send(200, "application/json", json);
}

void handleSaveAPI() {
  if (liveDistanceCm >= 0 && liveDistanceCm <= 400) {
    saveCurrentDistance();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "INVALID_DISTANCE");
  }
}

void handleClearAPI() {
  clearAllData();
  server.send(200, "text/plain", "CLEARED");
}
