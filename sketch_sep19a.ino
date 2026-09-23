#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer server(80);

const char* ssid = "ESP32-OLED";
const char* password = "12345678";

void showText(String text) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println(text);
  display.display();
}

void handleRoot() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta charset="UTF-8">
<title>ESP32 OLED Control</title>
<style>
  body { font-family: Arial, sans-serif; margin: 0; padding: 15px; background: #f0f0f0; color: #222; }
  h1 { font-size: 22px; margin-bottom: 15px; }
  h2 { font-size: 16px; margin: 0 0 10px 0; }
  .section { background: white; padding: 15px; margin-bottom: 15px; border-radius: 8px; box-shadow: 0 2px 6px rgba(0,0,0,0.1); }
  input[type="text"] { width: 100%; box-sizing: border-box; padding: 10px; font-size: 16px; border: 1px solid #ccc; border-radius: 5px; margin-bottom: 10px; }
  button { padding: 10px 16px; margin: 4px 4px 0 0; font-size: 14px; border: none; border-radius: 5px; background: #4CAF50; color: white; cursor: pointer; }
  button:active { background: #398c3a; }
  button.secondary { background: #607d8b; }
  button.danger { background: #d9534f; }
  #canvas { border: 2px solid #333; background: black; display: block; margin: 10px 0; width: 100%; max-width: 384px; aspect-ratio: 2 / 1; touch-action: none; cursor: crosshair; }
  .status { font-size: 13px; color: #666; margin-top: 6px; min-height: 16px; }
</style>
</head>
<body>
<h1>ESP32 OLED Control</h1>

<div class="section">
  <h2>Text</h2>
  <input type="text" id="textInput" placeholder="Type text..." value="Hello">
  <button onclick="sendText()">Show on OLED</button>
</div>

<div class="section">
  <h2>Draw</h2>
  <canvas id="canvas" width="128" height="64"></canvas>
  <button onclick="sendDrawing()">Send Drawing to OLED</button>
  <button class="secondary" onclick="clearCanvas()">Clear Canvas (phone only)</button>
  <div class="status" id="drawStatus"></div>
</div>

<div class="section">
  <h2>Animations</h2>
  <button onclick="scrollText()">Scroll Text</button>
  <button onclick="blinkText()">Blink Text</button>
  <button class="danger" onclick="clearOLED()">Clear OLED</button>
  <div class="status" id="animStatus"></div>
</div>

<script>
const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');
let isDrawing = false;

ctx.fillStyle = 'black';
ctx.fillRect(0, 0, canvas.width, canvas.height);

function posFromEvent(e, touch) {
  const rect = canvas.getBoundingClientRect();
  const clientX = touch ? e.touches[0].clientX : e.clientX;
  const clientY = touch ? e.touches[0].clientY : e.clientY;
  return {
    x: (clientX - rect.left) * (canvas.width / rect.width),
    y: (clientY - rect.top) * (canvas.height / rect.height)
  };
}

function startDraw(x, y) {
  isDrawing = true;
  ctx.strokeStyle = 'white';
  ctx.lineWidth = 2;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  ctx.beginPath();
  ctx.moveTo(x, y);
}

function moveDraw(x, y) {
  if (!isDrawing) return;
  ctx.lineTo(x, y);
  ctx.stroke();
}

canvas.addEventListener('mousedown', (e) => { const p = posFromEvent(e, false); startDraw(p.x, p.y); });
canvas.addEventListener('mousemove', (e) => { const p = posFromEvent(e, false); moveDraw(p.x, p.y); });
window.addEventListener('mouseup', () => { isDrawing = false; });

canvas.addEventListener('touchstart', (e) => { e.preventDefault(); const p = posFromEvent(e, true); startDraw(p.x, p.y); }, {passive:false});
canvas.addEventListener('touchmove', (e) => { e.preventDefault(); const p = posFromEvent(e, true); moveDraw(p.x, p.y); }, {passive:false});
canvas.addEventListener('touchend', (e) => { e.preventDefault(); isDrawing = false; }, {passive:false});

// Clears only the drawing pad on your phone — does not touch the OLED
function clearCanvas() {
  ctx.fillStyle = 'black';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  document.getElementById('drawStatus').innerText = '';
}

function sendText() {
  const text = document.getElementById('textInput').value;
  fetch('/text?msg=' + encodeURIComponent(text));
}

function sendDrawing() {
  const status = document.getElementById('drawStatus');
  status.innerText = 'Sending...';
  const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
  const d = imageData.data;
  let pix = '';
  // Check actual RGB (not alpha) — the black background is opaque too,
  // so alpha alone can't tell drawn pixels from empty ones
  for (let i = 0; i < d.length; i += 4) {
    if (d[i] > 100 || d[i+1] > 100 || d[i+2] > 100) {
      const idx = i / 4;
      const x = idx % canvas.width;
      const y = Math.floor(idx / canvas.width);
      pix += x + ',' + y + ';';
    }
  }
  fetch('/draw?p=' + encodeURIComponent(pix))
    .then(r => r.text())
    .then(() => { status.innerText = 'Sent to OLED'; })
    .catch(() => { status.innerText = 'Failed to send'; });
}

function scrollText() {
  const text = document.getElementById('textInput').value;
  const status = document.getElementById('animStatus');
  status.innerText = 'Scrolling...';
  fetch('/scroll?msg=' + encodeURIComponent(text)).then(() => { status.innerText = 'Done'; });
}

function blinkText() {
  const text = document.getElementById('textInput').value;
  const status = document.getElementById('animStatus');
  status.innerText = 'Blinking...';
  fetch('/blink?msg=' + encodeURIComponent(text)).then(() => { status.innerText = 'Done'; });
}

// Tells the ESP32 to actually clear the OLED screen
function clearOLED() {
  fetch('/clear').then(() => { document.getElementById('animStatus').innerText = 'OLED cleared'; });
}
</script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", page);
}

void handleText() {
  if (server.hasArg("msg")) {
    showText(server.arg("msg"));
  }
  server.send(200, "text/plain", "OK");
}

void handleDraw() {
  display.clearDisplay();
  if (server.hasArg("p")) {
    String pixels = server.arg("p");
    int i = 0;
    while (i < pixels.length()) {
      int c1 = pixels.indexOf(',', i);
      int s = pixels.indexOf(';', i);
      if (c1 == -1 || s == -1) break;

      int x = pixels.substring(i, c1).toInt();
      int y = pixels.substring(c1 + 1, s).toInt();

      if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        display.drawPixel(x, y, SSD1306_WHITE);
      }
      i = s + 1;
    }
  }
  display.display();
  server.send(200, "text/plain", "OK");
}

void handleScroll() {
  if (server.hasArg("msg")) {
    String text = server.arg("msg");
    int textWidth = text.length() * 6;
    for (int x = SCREEN_WIDTH; x > -textWidth; x -= 4) {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(x, 28);
      display.println(text);
      display.display();
      delay(20);
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleBlink() {
  if (server.hasArg("msg")) {
    String text = server.arg("msg");
    for (int i = 0; i < 6; i++) {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(2);
      display.setCursor(0, 20);
      display.println(text);
      display.display();
      delay(300);

      display.clearDisplay();
      display.display();
      delay(300);
    }
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.println(text);
    display.display();
  }
  server.send(200, "text/plain", "OK");
}

void handleClear() {
  display.clearDisplay();
  display.display();
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Starting WiFi...");
  display.display();

  WiFi.softAP(ssid, password);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connect phone to:");
  display.println("ESP32-OLED");
  display.println("Pass: 12345678");
  display.println("");
  display.println("Go to: 192.168.4.1");
  display.display();

  server.on("/", handleRoot);
  server.on("/text", handleText);
  server.on("/draw", handleDraw);
  server.on("/scroll", handleScroll);
  server.on("/blink", handleBlink);
  server.on("/clear", handleClear);

  server.begin();
}

void loop() {
  server.handleClient();
}