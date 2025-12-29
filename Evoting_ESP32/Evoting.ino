/*******************************************************************************
* ESP32 WiFi Bridge + LCD Controller + HTTP Client - PRODUCTION v3.0 + DEBUG
* ✅ HTTPS support for GitHub Codespaces
* ✅ API key and terminal ID header injection
* ✅ Robust error handling and buffer management
* ✅ Proper debugging output
* Firmware Version: 3.0.1
*******************************************************************************/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define STM32_RX_PIN 16
#define STM32_TX_PIN 17
#define LED_PIN 2

// LCD Setup
LiquidCrystal_I2C lcd(0x27, 16, 2);
bool lcdInitialized = false;

HardwareSerial STM32Serial(2);

// HTTP timeout (increased for HTTPS)
const int HTTP_TIMEOUT = 30000; // 30 seconds for GitHub Codespaces

// Command list
const char* commands[] = {
  "PING", "RESET", "LED_ON", "LED_OFF", "LED_BLINK",
  "WIFI_CONNECT", "WIFI_DISCONNECT", "WIFI_STATUS", "WIFI_IP",
  "HTTP_GET", "HTTP_POST",
  "LCD_INIT", "LCD_CLEAR", "LCD_PRINT", "LCD_CURSOR", "LCD_BACKLIGHT"
};
const int numCommands = 16;

void setup() {
  // START UART FIRST!
  STM32Serial.begin(115200, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  
  // Clear buffer
  delay(100);
  while (STM32Serial.available()) {
    STM32Serial.read();
  }
  
  // USB Serial
  Serial.begin(115200);
  delay(100);
  
  Serial.printf("Total Heap: %d bytes\n", ESP.getHeapSize());
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Min Free Heap: %d bytes\n", ESP.getMinFreeHeap());
  
  Serial.println("\n╔════════════════════════════════════════════════╗");
  Serial.println("║ ESP32 WiFi Bridge v3.0.1 - PRODUCTION READY   ║");
  Serial.println("║ ✅ HTTPS Support (GitHub Codespaces)          ║");
  Serial.println("║ ✅ API Key Authentication Headers             ║");
  Serial.println("║ ✅ Debug Mode Enabled                          ║");
  Serial.println("╚════════════════════════════════════════════════╝");
  Serial.printf("📡 UART: RX=%d TX=%d @ 115200\n", STM32_RX_PIN, STM32_TX_PIN);
  Serial.println("🖥️  LCD: Ready for init command");
  Serial.println("🌐 HTTP/HTTPS: Client ready");
  Serial.println("⏳ Listening for STM32...\n");
  
  // LED setup
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // I2C setup (for LCD)
  Wire.begin(21, 22);
  
  // Signal ready
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(150);
    digitalWrite(LED_PIN, LOW);
    delay(150);
  }
}

void loop() {
  if (STM32Serial.available()) {
    String message = STM32Serial.readStringUntil('\n');
    message.trim();
    
    if (message.length() > 0) {
      if (isCommand(message)) {
        Serial.print("📥 [CMD] ");
        Serial.println(message);
        processCommand(message);
      } else {
        Serial.print("💬 [STM32] ");
        Serial.println(message);
      }
    }
  }
}

bool isCommand(String msg) {
  for (int i = 0; i < numCommands; i++) {
    if (msg.startsWith(commands[i])) {
      return true;
    }
  }
  return false;
}

void processCommand(String cmd) {
  // ========== BASIC COMMANDS ==========
  if (cmd == "PING") {
    STM32Serial.println("PONG");
    Serial.println("✅ → PONG\n");
  }
  
  else if (cmd == "RESET") {
    STM32Serial.println("RESTARTING");
    Serial.println("🔄 → RESTARTING");
    delay(100);
    ESP.restart();
  }
  
  // ========== LED COMMANDS ==========
  else if (cmd == "LED_ON") {
    digitalWrite(LED_PIN, HIGH);
    STM32Serial.println("OK");
    Serial.println("💡 LED ON → OK\n");
  }
  
  else if (cmd == "LED_OFF") {
    digitalWrite(LED_PIN, LOW);
    STM32Serial.println("OK");
    Serial.println("💡 LED OFF → OK\n");
  }
  
  else if (cmd.startsWith("LED_BLINK,")) {
    int times = cmd.substring(10).toInt();
    Serial.printf("💡 LED BLINK x%d\n", times);
    for (int i = 0; i < times; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
    }
    STM32Serial.println("OK");
    Serial.println("✅ → OK\n");
  }
  
  // ========== WIFI COMMANDS ==========
  else if (cmd.startsWith("WIFI_CONNECT,")) {
    handleWiFiConnect(cmd);
  }
  
  else if (cmd == "WIFI_DISCONNECT") {
    WiFi.disconnect();
    STM32Serial.println("OK");
    Serial.println("📡 Disconnected → OK\n");
  }
  
  else if (cmd == "WIFI_STATUS") {
    if (WiFi.status() == WL_CONNECTED) {
      STM32Serial.println("CONNECTED");
      Serial.println("✅ → CONNECTED\n");
    } else {
      STM32Serial.println("DISCONNECTED");
      Serial.println("❌ → DISCONNECTED\n");
    }
  }
  
  else if (cmd == "WIFI_IP") {
    if (WiFi.status() == WL_CONNECTED) {
      STM32Serial.print("IP:");
      STM32Serial.println(WiFi.localIP().toString());
      Serial.printf("🌐 → IP:%s\n\n", WiFi.localIP().toString().c_str());
    } else {
      STM32Serial.println("ERROR:NOT_CONNECTED");
      Serial.println("❌ → ERROR:NOT_CONNECTED\n");
    }
  }
  
  // ========== HTTP COMMANDS ==========
  else if (cmd.startsWith("HTTP_GET,")) {
    handleHTTPGet(cmd);
  }
  
  else if (cmd.startsWith("HTTP_POST,")) {
    handleHTTPPost(cmd);
  }
  
  // ========== LCD COMMANDS ==========
  else if (cmd == "LCD_INIT") {
    Serial.println("🖥️  Initializing LCD...");
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcdInitialized = true;
    STM32Serial.println("OK");
    Serial.println("✅ → LCD initialized → OK\n");
  }
  
  else if (cmd == "LCD_CLEAR") {
    if (!checkLCDInit()) return;
    lcd.clear();
    STM32Serial.println("OK");
    Serial.println("🖥️  LCD cleared → OK\n");
  }
  
  else if (cmd.startsWith("LCD_PRINT,")) {
    if (!checkLCDInit()) return;
    String text = cmd.substring(10);
    lcd.print(text);
    STM32Serial.println("OK");
    Serial.printf("🖥️  LCD print: \"%s\" → OK\n\n", text.c_str());
  }
  
  else if (cmd.startsWith("LCD_CURSOR,")) {
    if (!checkLCDInit()) return;
    int firstComma = cmd.indexOf(',');
    int secondComma = cmd.indexOf(',', firstComma + 1);
    int row = cmd.substring(firstComma + 1, secondComma).toInt();
    int col = cmd.substring(secondComma + 1).toInt();
    lcd.setCursor(col, row);
    STM32Serial.println("OK");
    Serial.printf("🖥️  LCD cursor: (%d,%d) → OK\n\n", row, col);
  }
  
  else if (cmd.startsWith("LCD_BACKLIGHT,")) {
    if (!checkLCDInit()) return;
    int state = cmd.substring(14).toInt();
    if (state) {
      lcd.backlight();
      Serial.println("💡 LCD backlight ON");
    } else {
      lcd.noBacklight();
      Serial.println("💡 LCD backlight OFF");
    }
    STM32Serial.println("OK");
    Serial.println();
  }
  
  else {
    STM32Serial.println("ERROR:UNKNOWN");
    Serial.println("❌ Unknown command\n");
  }
}

// ========== HELPER FUNCTIONS ==========

bool checkLCDInit() {
  if (!lcdInitialized) {
    STM32Serial.println("ERROR:NOT_INIT");
    Serial.println("❌ LCD not initialized!\n");
    return false;
  }
  return true;
}

void handleWiFiConnect(String cmd) {
  // Format: WIFI_CONNECT,ssid,password
  int firstComma = cmd.indexOf(',');
  int secondComma = cmd.indexOf(',', firstComma + 1);
  
  String ssid = cmd.substring(firstComma + 1, secondComma);
  String password = cmd.substring(secondComma + 1);
  
  Serial.printf("📶 Connecting to: %s\n", ssid.c_str());
  
  // Disconnect if already connected
  if (WiFi.status() != WL_DISCONNECTED) {
    WiFi.disconnect(true);
    delay(100);
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  // Wait max 10 seconds
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  // Send response immediately
  if (WiFi.status() == WL_CONNECTED) {
    STM32Serial.println("CONNECTED");
    Serial.printf("✅ Connected! IP: %s\n\n", WiFi.localIP().toString().c_str());
  } else {
    STM32Serial.println("ERROR");
    Serial.println("❌ Connection failed!\n");
  }
}

void handleHTTPGet(String cmd) {
  // Format: HTTP_GET,host,port,path,api_key,terminal_id
  if (WiFi.status() != WL_CONNECTED) {
    STM32Serial.println("ERROR:NO_WIFI");
    Serial.println("❌ Not connected to WiFi!\n");
    return;
  }
  
  // Parse parameters
  int comma1 = cmd.indexOf(',');
  int comma2 = cmd.indexOf(',', comma1 + 1);
  int comma3 = cmd.indexOf(',', comma2 + 1);
  int comma4 = cmd.indexOf(',', comma3 + 1);
  int comma5 = cmd.indexOf(',', comma4 + 1);
  
  String host = cmd.substring(comma1 + 1, comma2);
  int port = cmd.substring(comma2 + 1, comma3).toInt();
  String path = cmd.substring(comma3 + 1, comma4);
  String apiKey = cmd.substring(comma4 + 1, comma5);
  String terminalId = cmd.substring(comma5 + 1);
  
  // Build URL
  String url;
  if (host.endsWith(".app.github.dev") || port == 443) {
    url = "https://" + host + path;
  } else if (port == 80) {
    url = "http://" + host + path;
  } else {
    url = "http://" + host + ":" + String(port) + path;
  }
  
  Serial.println("🌐 HTTP GET Request:");
  Serial.printf("  URL: %s\n", url.c_str());
  Serial.printf("  API Key: %s\n", apiKey.c_str());
  Serial.printf("  Terminal ID: %s\n", terminalId.c_str());
  
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT);
  
  // HTTPS support
  if (url.startsWith("https://")) {
    WiFiClientSecure *client = new WiFiClientSecure;
    client->setInsecure();
    http.begin(*client, url);
  } else {
    http.begin(url);
  }
  
  // ADD AUTHENTICATION HEADERS
  http.addHeader("x-api-key", apiKey);
  http.addHeader("x-terminal-id", terminalId);
  
  // GitHub Codespaces bypass header
  if (host.endsWith(".app.github.dev")) {
    http.addHeader("ngrok-skip-browser-warning", "true");
  }
  
  int httpCode = http.GET();
  Serial.printf("  Response Code: %d\n", httpCode);
  
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.printf("  Payload Length: %d bytes\n", payload.length());
      
      // ✅ Send response to STM32 ALL AT ONCE (no chunking!)
      STM32Serial.println("HTTP_RESPONSE:" + String(httpCode));
      STM32Serial.println("BODY:" + payload);
      STM32Serial.println("HTTP_END");
      
      Serial.printf("✅ Response sent to STM32 (%d bytes)\n\n", payload.length());
      
      // ✅ DEBUG: Show exact response
      Serial.println("🔍 [ESP32 DEBUG] Exact response sent:");
      Serial.println("─────────────────────────────────────");
      Serial.print("HTTP_RESPONSE:");
      Serial.println(httpCode);
      Serial.print("BODY:");
      Serial.println(payload);
      Serial.println("HTTP_END");
      Serial.println("─────────────────────────────────────");
      Serial.printf("Total lines: 3 | Payload: %d bytes\n\n", payload.length());
      
    } else {
      Serial.printf("❌ HTTP Error: %d\n\n", httpCode);
      STM32Serial.printf("ERROR:HTTP_%d\n", httpCode);
    }
  } else {
    Serial.printf("❌ Connection failed: %s\n\n", http.errorToString(httpCode).c_str());
    STM32Serial.println("ERROR:CONNECTION");
  }
  
  http.end();
}

void handleHTTPPost(String cmd) {
  // Format: HTTP_POST,host,port,path,json_data,api_key,terminal_id
  Serial.println("🔍 [DEBUG] handleHTTPPost started");
  Serial.printf("  Command length: %d\n", cmd.length());
  Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
  
  if (WiFi.status() != WL_CONNECTED) {
    STM32Serial.println("ERROR:NO_WIFI");
    Serial.println("❌ Not connected to WiFi!\n");
    return;
  }
  
  Serial.println("🔍 [DEBUG] WiFi check passed");
  
  // Parse from RIGHT to get api_key and terminal_id (last 2 params)
  int lastComma = cmd.lastIndexOf(',');
  int secondLastComma = cmd.lastIndexOf(',', lastComma - 1);
  
  if (lastComma == -1 || secondLastComma == -1) {
    Serial.println("❌ Missing api_key or terminal_id!");
    STM32Serial.println("ERROR:INVALID_FORMAT");
    return;
  }
  
  String terminalId = cmd.substring(lastComma + 1);
  String apiKey = cmd.substring(secondLastComma + 1, lastComma);
  
  // Parse first 3 commas (host, port, path)
  int comma1 = cmd.indexOf(',');
  int comma2 = cmd.indexOf(',', comma1 + 1);
  int comma3 = cmd.indexOf(',', comma2 + 1);
  
  if (comma1 == -1 || comma2 == -1 || comma3 == -1) {
    Serial.println("❌ Invalid command format!");
    STM32Serial.println("ERROR:INVALID_FORMAT");
    return;
  }
  
  String host = cmd.substring(comma1 + 1, comma2);
  int port = cmd.substring(comma2 + 1, comma3).toInt();
  
  // Find the 4th comma (after path, before JSON)
  int comma4 = cmd.indexOf(',', comma3 + 1);
  
  if (comma4 == -1 || comma4 >= secondLastComma) {
    Serial.println("❌ Cannot find path/JSON separator!");
    STM32Serial.println("ERROR:INVALID_FORMAT");
    return;
  }
  
  String path = cmd.substring(comma3 + 1, comma4);
  String jsonData = cmd.substring(comma4 + 1, secondLastComma);
  
  Serial.printf("  Host: %s\n", host.c_str());
  Serial.printf("  Port: %d\n", port);
  Serial.printf("  Path: %s\n", path.c_str());
  Serial.printf("  JSON Length: %d bytes\n", jsonData.length());
  Serial.printf("  API Key: %s\n", apiKey.c_str());
  Serial.printf("  Terminal ID: %s\n", terminalId.c_str());
  
  // BUILD URL
  String url;
  if (host.endsWith(".app.github.dev") || port == 443) {
    url = "https://" + host + path;
    Serial.println("🔍 [DEBUG] Using HTTPS");
  } else if (port == 80) {
    url = "http://" + host + path;
  } else {
    url = "http://" + host + ":" + String(port) + path;
  }
  
  Serial.println("🌐 HTTP POST Request:");
  Serial.printf("  URL: %s\n", url.c_str());
  
  if (jsonData.length() > 200) {
    Serial.print("  Data Preview: ");
    Serial.print(jsonData.substring(0, 100));
    Serial.println("...");
  } else {
    Serial.printf("  Data: %s\n", jsonData.c_str());
  }
  
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT);
  
  // HTTPS support
  if (url.startsWith("https://")) {
    WiFiClientSecure *client = new WiFiClientSecure;
    client->setInsecure();
    http.begin(*client, url);
    Serial.println("🔍 [DEBUG] Using HTTPS (insecure mode)");
  } else {
    http.begin(url);
    Serial.println("🔍 [DEBUG] Using HTTP");
  }
  
  // ADD HEADERS
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", apiKey);
  http.addHeader("x-terminal-id", terminalId);
  Serial.println("🔍 [DEBUG] Headers set");
  
  // GitHub Codespaces bypass header
  if (host.endsWith(".app.github.dev")) {
    http.addHeader("ngrok-skip-browser-warning", "true");
  }
  
  Serial.println("🔍 [DEBUG] Calling http.POST()...");
  Serial.printf("  Free heap before POST: %d bytes\n", ESP.getFreeHeap());
  
  int httpCode = http.POST(jsonData);
  
  Serial.printf("🔍 [DEBUG] POST returned! Code: %d\n", httpCode);
  
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED ||
        httpCode == HTTP_CODE_ACCEPTED) {
      String payload = http.getString();
      Serial.printf("  ✅ Success! Payload: %d bytes\n", payload.length());
      
      // ✅ Send response to STM32 ALL AT ONCE (no chunking!)
      // This matches handleHTTPGet behavior
      STM32Serial.println("HTTP_RESPONSE:" + String(httpCode));
      STM32Serial.println("BODY:" + payload);
      STM32Serial.println("HTTP_END");
      
      Serial.printf("✅ Response sent to STM32 (%d bytes)\n\n", payload.length());
      
      // ✅ DEBUG: Show exact response
      Serial.println("🔍 [ESP32 DEBUG] Exact response sent:");
      Serial.println("─────────────────────────────────────");
      Serial.print("HTTP_RESPONSE:");
      Serial.println(httpCode);
      Serial.print("BODY:");
      // Only show first 200 chars for long responses
      if (payload.length() > 200) {
        Serial.print(payload.substring(0, 200));
        Serial.println("...");
      } else {
        Serial.println(payload);
      }
      Serial.println("HTTP_END");
      Serial.println("─────────────────────────────────────");
      Serial.printf("Total lines: 3 | Payload: %d bytes\n\n", payload.length());
      
    } else {
      Serial.printf("❌ HTTP Error: %d\n\n", httpCode);
      STM32Serial.printf("ERROR:HTTP_%d\n", httpCode);
    }
  } else {
    Serial.printf("❌ Connection failed: %s\n\n", http.errorToString(httpCode).c_str());
    STM32Serial.println("ERROR:CONNECTION");
  }
  
  http.end();
  Serial.println("🔍 [DEBUG] handleHTTPPost completed");
}
