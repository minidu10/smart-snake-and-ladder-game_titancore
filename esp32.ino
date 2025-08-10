#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

const char* ssid = "SnakeLadderESP32";
const char* password = "12345678";

WebServer server(80);

String currentGameId = "";
bool gameActive = false;

// NEW: Communication Protocol Variables
const char MSG_START = '<';
const char MSG_END = '>';
const int SERIAL_TIMEOUT = 100;
int commErrorCount = 0;
const int MAX_COMM_ERRORS = 5;
bool communicationError = false;
unsigned long lastCommCheck = 0;

void sendGameSetupToMega(String mode, String gameId, String p1Name, String p1Color, String p2Name, String p2Color) {
  // Send game setup data to Mega via Serial2
  Serial2.println("GAME_SETUP");
  Serial2.flush(); // Ensure complete transmission
  delay(100);      // Allow Mega time to process
  
  Serial2.println("MODE:" + mode);
  Serial2.flush();
  
  Serial2.println("GAMEID:" + gameId);
  Serial2.flush();
  
  Serial2.println("P1NAME:" + p1Name);
  Serial2.flush();
  
  Serial2.println("P1COLOR:" + p1Color);
  Serial2.flush();
  
  Serial2.println("P2NAME:" + p2Name);
  Serial2.flush();
  
  Serial2.println("P2COLOR:" + p2Color);
  Serial2.flush();
  
  Serial2.println("START");  // Signal to Mega that setup is complete
  Serial2.flush();
  
  Serial.println("Game setup sent to Mega:");
  Serial.println("  Mode: " + mode + ", GameID: " + gameId);
  Serial.println("  P1: " + p1Name + " (" + p1Color + ")");
  Serial.println("  P2: " + p2Name + " (" + p2Color + ")");
}

// === Handle Game Setup from Web App ===
void handleGameSetup() {
  if (server.method() == HTTP_POST) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
      Serial.println("JSON parse failed: " + String(error.c_str()));
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    // Extract game data matching backend format
    String mode = doc["mode"].as<String>();
    String gameId = doc["gameId"].as<String>();
    currentGameId = gameId; // Store for score updates
    gameActive = true;

    JsonArray players = doc["players"];
    if (players.size() < 2) {
      server.send(400, "application/json", "{\"error\":\"Need 2 players\"}");
      return;
    }

    String p1Name = players[0]["name"].as<String>();
    String p1Color = players[0]["color"].as<String>();
    String p2Name = players[1]["name"].as<String>();
    String p2Color = players[1]["color"].as<String>();

    Serial.println("Game Setup Received from Web App:");
    Serial.println("MODE:" + mode + " GAMEID:" + gameId);
    Serial.println("PLAYER1: " + p1Name + " (" + p1Color + ")");
    Serial.println("PLAYER2: " + p2Name + " (" + p2Color + ")");

    // Forward to Mega
    sendGameSetupToMega(mode, gameId, p1Name, p1Color, p2Name, p2Color);
    
    server.send(200, "application/json", "{\"message\":\"Game setup sent to Arduino\"}");
  } else {
    server.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
  }
}

// === Send Player Score to Web App Backend ===
void sendScoreToWebApp(int player, int diceValue) {
  if (currentGameId == "") {
    Serial.println("No gameId set, score not sent to web app");
    return;
  }

  if (!gameActive) {
    Serial.println("Game not active, score not sent");
    return;
  }

  // Construct the backend URL - update this IP to match your backend server
  String backend_url = "http://192.168.4.2:5000/api/update-position/" + currentGameId;
  HTTPClient http;
  http.begin(backend_url);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload - matches backend format
  String payload = "{\"dice\":" + String(diceValue) + ",\"player\":" + String(player) + "}";
  int httpCode = http.POST(payload);
  
  Serial.print("Sent to Web App: ");
  Serial.println(payload);
  Serial.print("Response code: ");
  Serial.println(httpCode);
  
  if (httpCode > 0) {
    String response = http.getString();
    if (response.length() > 0) {
      Serial.println("Web App Response: " + response);
    }
  } else {
    Serial.println("HTTP POST failed, code: " + String(httpCode));
  }
  
  http.end();
}

// === Handle Play Again Command from Web App ===
void handlePlayAgain() {
  if (server.method() == HTTP_POST) {
    if (currentGameId != "") {
      Serial2.println("PLAY_AGAIN");
      Serial2.flush();
      Serial.println("Play Again command sent to Mega");
      gameActive = true; // Reactivate game
      server.send(200, "application/json", "{\"message\":\"Play again sent to Arduino\"}");
    } else {
      Serial.println("Play Again requested but no active game");
      server.send(400, "application/json", "{\"error\":\"No active game\"}");
    }
  } else {
    server.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
  }
}

// === Handle End Game Command from Web App ===
void handleEndGame() {
  if (server.method() == HTTP_POST) {
    if (currentGameId != "") {
      Serial2.println("END_GAME");
      Serial2.flush();
      Serial.println("End Game command sent to Mega");
      
      // Clear game state
      gameActive = false;
      currentGameId = "";
      
      server.send(200, "application/json", "{\"message\":\"End game sent to Arduino\"}");
    } else {
      Serial.println("End Game requested but no active game");
      server.send(400, "application/json", "{\"error\":\"No active game\"}");
    }
  } else {
    server.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
  }
}

// === Handle Reset Game Command from Arduino Mega ===
void handleResetFromMega() {
  Serial.println("Hardware reset signal received from Arduino Mega");
  
  if (currentGameId != "") {
    Serial.println("Forwarding reset signal to Web App...");
    
    // Send reset signal to web app
    bool resetSuccess = sendResetToWebApp();
    
    if (resetSuccess) {
      Serial.println("Reset signal successfully sent to Web App");
    } else {
      Serial.println("Failed to send reset signal to Web App");
    }
    
    // Clear ESP32 game state regardless of web app response
    String previousGameId = currentGameId;
    currentGameId = "";
    gameActive = false;
    
    Serial.println("ESP32 game state cleared");
    Serial.println("Previous Game ID: " + previousGameId + " -> Now: " + currentGameId);
  } else {
    Serial.println("Reset received but no active game ID stored");
    gameActive = false; // Ensure game is marked as inactive
  }
}

// === Send Reset Signal to Web App Backend ===
bool sendResetToWebApp() {
  if (currentGameId == "") {
    Serial.println("No gameId set, reset not sent to web app");
    return false;
  }

  // Construct the backend URL for hardware reset
  String backend_url = "http://192.168.4.2:5000/api/hardware-reset/" + currentGameId;
  HTTPClient http;
  http.begin(backend_url);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload with reset source information
  String payload = "{\"source\":\"arduino_mega\",\"timestamp\":\"" + String(millis()) + "\",\"gameId\":\"" + currentGameId + "\"}";
  int httpCode = http.POST(payload);
  
  Serial.print("Reset sent to Web App: ");
  Serial.println(payload);
  Serial.print("Response code: ");
  Serial.println(httpCode);
  
  bool success = false;
  if (httpCode > 0) {
    String response = http.getString();
    if (response.length() > 0) {
      Serial.println("Web App Response: " + response);
    }
    
    // Consider 200-299 as success
    if (httpCode >= 200 && httpCode < 300) {
      success = true;
    }
  } else {
    Serial.println("HTTP POST failed, code: " + String(httpCode));
  }
  
  http.end();
  return success;
}

// === Get Current Game Status ===
void handleGameStatus() {
  String status = "waiting";
  if (currentGameId != "" && gameActive) {
    status = "active";
  } else if (currentGameId != "" && !gameActive) {
    status = "paused";
  }
  
  String response = "{\"gameId\":\"" + currentGameId + "\",\"status\":\"" + status + "\",\"gameActive\":" + (gameActive ? "true" : "false") + "}";
  server.send(200, "application/json", response);
}

// === Health Check Endpoint ===
void handleHealthCheck() {
  String healthStatus = "{\"status\":\"ESP32 Bridge Ready\",\"gameId\":\"" + currentGameId + 
                        "\",\"gameActive\":" + (gameActive ? "true" : "false") + 
                        ",\"uptime\":" + String(millis()) + 
                        ",\"commErrors\":" + String(commErrorCount) + "}";
  server.send(200, "application/json", healthStatus);
}

// === CORS Headers for Web App ===
void handleCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(200);
}

// NEW: Parse messages with start/end markers
String parseFramedMessage(String message) {
  int startIdx = message.indexOf(MSG_START);
  int endIdx = message.indexOf(MSG_END);
  
  if (startIdx >= 0 && endIdx > startIdx) {
    return message.substring(startIdx + 1, endIdx);
  }
  return message; // Return original if not properly framed
}

// NEW: Check if message is properly framed
bool isFramedMessage(String message) {
  return (message.indexOf(MSG_START) >= 0 && message.indexOf(MSG_END) > message.indexOf(MSG_START));
}

// === Setup Function ===
void setup() {
  Serial.begin(9600);        // For Serial Monitor
  Serial2.begin(9600, SERIAL_8N1, 16, 17);  // For Arduino Mega communication (pins 16=RX, 17=TX)
  Serial2.setTimeout(SERIAL_TIMEOUT);

  // Start WiFi Access Point
  WiFi.softAP(ssid, password);
  Serial.println("WiFi Access Point Started");
  Serial.println("SSID: " + String(ssid));
  Serial.println("Password: " + String(password));
  Serial.println("ESP32 IP Address: " + WiFi.softAPIP().toString());

  // Setup HTTP Routes
  server.on("/game-setup", HTTP_POST, handleGameSetup);
  server.on("/play-again", HTTP_POST, handlePlayAgain);
  server.on("/end-game", HTTP_POST, handleEndGame);
  server.on("/game-status", HTTP_GET, handleGameStatus);
  server.on("/health", HTTP_GET, handleHealthCheck);
  
  // Handle CORS preflight requests
  server.on("/game-setup", HTTP_OPTIONS, handleCORS);
  server.on("/play-again", HTTP_OPTIONS, handleCORS);
  server.on("/end-game", HTTP_OPTIONS, handleCORS);
  server.on("/game-status", HTTP_OPTIONS, handleCORS);
  
  server.onNotFound([]() {
    server.send(404, "application/json", "{\"error\":\"Endpoint not found\"}");
  });

  server.begin();
  Serial.println("HTTP Server started on port 80");
  Serial.println("Ready to bridge Web App and Arduino Mega");
  Serial.println("Available endpoints:");
  Serial.println("  POST /game-setup - Receive game setup from web");
  Serial.println("  POST /play-again - Handle play again requests");
  Serial.println("  POST /end-game - Handle end game requests");
  Serial.println("  GET  /game-status - Get current game status");
  Serial.println("  GET  /health - Health check");
  Serial.println();
  Serial.println("Waiting for Mega connection and web app requests...");
  
  // Initialize game state
  currentGameId = "";
  gameActive = false;
  
  // Wait for Serial2 to initialize
  delay(1000);
  
  // Send initial "ready" message to Mega
  Serial2.println("ESP32_READY");
  Serial2.flush();
  Serial.println("Sent ready signal to Arduino Mega");
}

// === Main Loop ===
void loop() {
  server.handleClient();

  // === Handle incoming data from Arduino Mega ===
  if (Serial2.available()) {
    // NEW: More robust reading with timeout
    String receivedData = "";
    unsigned long startReadTime = millis();
    
    // Read until we get a complete message or timeout
    while (millis() - startReadTime < SERIAL_TIMEOUT) {
      if (Serial2.available()) {
        char c = Serial2.read();
        if (c == '\n' || c == '\r') {
          if (receivedData.length() > 0) break;
        } else {
          receivedData += c;
        }
      }
      yield();
    }
    
    receivedData.trim();
    
    if (receivedData.length() > 0) {
      // Check if it's a framed message
      if (isFramedMessage(receivedData)) {
        String content = parseFramedMessage(receivedData);
        Serial.println("From Mega (framed): " + content);
        
        // Handle reset signal from Mega
        if (content == "RESET_GAME") {
          handleResetFromMega();
          return;
        }
        
        // Handle MEGA_READY signal
        if (content == "MEGA_READY") {
          Serial.println("Arduino Mega is ready and connected");
          communicationError = false;
          commErrorCount = 0;
          return;
        }
        
        // Parse score data from Mega (format: "player,diceValue")
        int commaIndex = content.indexOf(',');
        if (commaIndex > 0) {
          int player = content.substring(0, commaIndex).toInt();
          int diceValue = content.substring(commaIndex + 1).toInt();
          
          // Validate data before sending to web app
          if (player >= 1 && player <= 2 && diceValue >= 1 && diceValue <= 6) {
            Serial.println("Valid score - Player " + String(player) + " rolled " + String(diceValue));
            sendScoreToWebApp(player, diceValue);
          } else {
            Serial.println("Invalid score data: Player=" + String(player) + ", Dice=" + String(diceValue));
          }
        } else {
          // Handle other types of messages
          Serial.println("Info from Mega: " + content);
        }
      } 
      else {
        // Legacy non-framed message handling
        Serial.println("From Mega (unframed): " + receivedData);
        
        // Handle reset signal from Mega
        if (receivedData == "RESET_GAME") {
          handleResetFromMega();
        }
        // Parse score data from Mega (format: "player,diceValue")
        else {
          int commaIndex = receivedData.indexOf(',');
          if (commaIndex > 0) {
            int player = receivedData.substring(0, commaIndex).toInt();
            int diceValue = receivedData.substring(commaIndex + 1).toInt();
            
            // Validate data before sending to web app
            if (player >= 1 && player <= 2 && diceValue >= 1 && diceValue <= 6) {
              Serial.println("Valid score - Player " + String(player) + " rolled " + String(diceValue));
              sendScoreToWebApp(player, diceValue);
            } else {
              Serial.println("Invalid score data: Player=" + String(player) + ", Dice=" + String(diceValue));
              commErrorCount++;
            }
          } else {
            // Handle other types of messages from Mega
            Serial.println("Unknown message from Mega: " + receivedData);
          }
        }
      }
    }
  }
  
  // Check communication status periodically
  if (millis() - lastCommCheck > 60000) { // Every minute
    lastCommCheck = millis();
    
    if (commErrorCount > MAX_COMM_ERRORS) {
      communicationError = true;
      Serial.println("WARNING: High communication error count: " + String(commErrorCount));
    }
    
    // Reset error count periodically
    if (commErrorCount > 0 && millis() > 300000) { // After 5 minutes uptime
      commErrorCount = 0;
    }
  }
  
  // Small delay to prevent overwhelming the system
  delay(10);
}

// === Additional Utility Functions ===

// Function to check if ESP32 is connected to backend
bool testBackendConnection() {
  HTTPClient http;
  http.begin("http://192.168.4.2:5000/api/health");
  http.setTimeout(5000); // 5 second timeout
  
  int httpCode = http.GET();
  bool connected = (httpCode == 200);
  
  Serial.println("Backend connection test: " + String(connected ? "SUCCESS" : "FAILED") + " (Code: " + String(httpCode) + ")");
  
  http.end();
  return connected;
}

// Function to send heartbeat to backend (optional)
void sendHeartbeat() {
  if (currentGameId != "" && gameActive) {
    HTTPClient http;
    http.begin("http://192.168.4.2:5000/api/heartbeat");
    http.addHeader("Content-Type", "application/json");
    
    String payload = "{\"gameId\":\"" + currentGameId + "\",\"source\":\"esp32\",\"timestamp\":" + String(millis()) + "}";
    int httpCode = http.POST(payload);
    
    if (httpCode != 200) {
      Serial.println("Heartbeat failed: " + String(httpCode));
    }
    
    http.end();
  }
}

// NEW: Diagnostic function for Serial2 issues
void dumpSerial2Buffer() {
  Serial.print("Raw Serial2 buffer bytes: ");
  int count = 0;
  while (Serial2.available() && count < 50) {
    byte b = Serial2.read();
    Serial.print(b, HEX);
    Serial.print(" ");
    count++;
  }
  Serial.println();
}

// NEW: Function to reset communication errors
void resetCommunicationErrors() {
  communicationError = false;
  commErrorCount = 0;
}

/*
 * ESP32 BRIDGE COMPLETE CODE - COMMUNICATION FIX VERSION
 * 
 * FIXES APPLIED:
 * 1. Removed all emoji characters from serial messages
 * 2. Added reliable message protocol with start/end markers
 * 3. Added serial flush after sending to ensure complete transmission
 * 4. Added timeout handling for serial communication
 * 5. Added diagnostics for troubleshooting
 * 
 * COMMUNICATION IMPROVEMENTS:
 * - Support for both framed (<message>) and unframed messages
 * - More robust message reading with timeouts
 * - Better error detection and recovery
 * 
 * This version should now reliably communicate with the fixed Arduino Mega.
 */
