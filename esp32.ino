#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

const char* ssid = "SnakeLadderESP32";
const char* password = "12345678";

WebServer server(80);

String currentGameId = "";
bool gameActive = false;
void sendGameSetupToMega(String mode, String gameId, String p1Name, String p1Color, String p2Name, String p2Color) {
  // Send game setup data to Mega via Serial2
  Serial2.println("GAME_SETUP");
  Serial2.println("MODE:" + mode);
  Serial2.println("GAMEID:" + gameId);
  Serial2.println("P1NAME:" + p1Name);
  Serial2.println("P1COLOR:" + p1Color);
  Serial2.println("P2NAME:" + p2Name);
  Serial2.println("P2COLOR:" + p2Color);
  Serial2.println("START");  // Signal to Mega that setup is complete
  
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
  String healthStatus = "{\"status\":\"ESP32 Bridge Ready\",\"gameId\":\"" + currentGameId + "\",\"gameActive\":" + (gameActive ? "true" : "false") + ",\"uptime\":" + String(millis()) + "}";
  server.send(200, "application/json", healthStatus);
}

// === CORS Headers for Web App ===
void handleCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(200);
}

// === Setup Function ===
void setup() {
  Serial.begin(9600);        // For Serial Monitor
  Serial2.begin(9600, SERIAL_8N1, 16, 17);  // For Arduino Mega communication (pins 16=RX, 17=TX)

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
}

// === Main Loop ===
void loop() {
  server.handleClient();

  // === Handle incoming data from Arduino Mega ===
  if (Serial2.available()) {
    String receivedData = Serial2.readStringUntil('\n');
    receivedData.trim();
    
    if (receivedData.length() > 0) {
      Serial.println("From Mega: " + receivedData);

      // Handle reset signal from Mega
      if (receivedData == "RESET_GAME") {
        handleResetFromMega();
      }
      // Handle other messages from Mega
      else {
        // Parse score data from Mega (format: "player,diceValue")
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
          }
        } else {
          // Handle other types of messages from Mega
          Serial.println("Info from Mega: " + receivedData);
          
          // Check for other known commands
          if (receivedData.startsWith("ERROR:")) {
            Serial.println("Error from Mega: " + receivedData);
          } else if (receivedData.startsWith("STATUS:")) {
            Serial.println("Status from Mega: " + receivedData);
          } else if (receivedData.startsWith("DEBUG:")) {
            Serial.println("Debug from Mega: " + receivedData);
          } else {
            Serial.println("Unknown message from Mega: " + receivedData);
          }
        }
      }
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

/*
 * ESP32 BRIDGE COMPLETE CODE
 * 
 * FEATURES IMPLEMENTED:
 * - Game Setup Communication (Web -> ESP32 -> Mega)
 * - Score Updates (Mega -> ESP32 -> Web)
 * - Play Again Command (Web -> ESP32 -> Mega)
 * - End Game Command (Web -> ESP32 -> Mega)
 * - Hardware Reset Handling (Mega -> ESP32 -> Web)
 * - Game State Management
 * - Error Handling & Validation
 * - CORS Support for Web App
 * - Health Check Endpoint
 * - Connection Status Monitoring
 * 
 * COMMUNICATION FLOW:
 * 1. Web App -> ESP32: Game setup, play again, end game
 * 2. ESP32 -> Mega: Forward commands via Serial2
 * 3. Mega -> ESP32: Score updates, reset signals
 * 4. ESP32 -> Web App: Score updates, reset notifications
 * 
 * ENDPOINTS:
 * - POST /game-setup: Initialize game
 * - POST /play-again: Restart game
 * - POST /end-game: End current game
 * - GET /game-status: Get current status
 * - GET /health: System health check
 * 
 * READY FOR DEPLOYMENT
 */
