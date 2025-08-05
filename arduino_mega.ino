#include <FastLED.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <math.h>
#define GRID_PIN 23
#define DECO_PIN 29
#define RING_PIN 31
#define NUM_GRID_LEDS 100
#define NUM_DECO_LEDS 75
#define NUM_RING_LEDS 32
CRGB gridLEDs[NUM_GRID_LEDS];
CRGB decoLEDs[NUM_DECO_LEDS];
CRGB ringLEDs[NUM_RING_LEDS];
#include <Adafruit_NeoPixel.h>
#define LADDER_PIN 27
#define SNAKE_PIN 25
#define NUM_LADDER_LEDS 51
#define NUM_SNAKE_LEDS 48
Adafruit_NeoPixel ladderLEDs(NUM_LADDER_LEDS, LADDER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel snakeLEDs(NUM_SNAKE_LEDS, SNAKE_PIN, NEO_GRB + NEO_KHZ800);
String mode = "";
String gameId = "";
String p1Name = "";
String p1Color = "";
String p2Name = "";
String p2Color = "";
bool receivingSetup = false;
bool setupComplete = false;
bool gameStarted = false;
LiquidCrystal_I2C lcd(0x27, 20, 4);
SoftwareSerial mp3Serial(10, 11);
DFRobotDFPlayerMini mp3;
bool soundInitialized = false;
bool backgroundMusicPlaying = false;
#define SOUND_BACKGROUND 1
#define SOUND_WIN        6
#define SOUND_SNAKE      3
#define SOUND_LADDER     4
#define SOUND_RESET      5
#define SOUND_INVALID    7
const int button1Pin = 2;
const int button2Pin = 3;
const int resetButtonPin = 4;
const int playAgainPin = 5;
const int motorPin1 = 8;
const int motorPin2 = 9;
const int motorEN = 7;
const int hallPins[6] = {41, 43, 45, 47, 49, 51};
const int MOTOR_MAX_SPEED = 90;
const int MOTOR_MIN_SPEED = 60;
const int ROBOT_MAX_SPEED = 80;
const int ROBOT_MIN_SPEED = 45;
const int PRECISION_READING_SAMPLES = 10;
int lastMotorSpeed = 0;
unsigned long motorAccelerationTime = 0;
unsigned long motorRunTime = 0;
const int GRID_BRIGHTNESS = 10;
const int PLAYER_BRIGHTNESS = 255;
struct Player {
  String name;
  String colorStr;
  CRGB colorCode;
  int position;
  int score;
};
Player players[2];
int currentPlayer = 0;
bool waitingForNext = false;
bool gameWon = false;
bool systemReady = false;
int winner = -1;
unsigned long lastButtonPress = 0;
unsigned long lastResetCheck = 0;
unsigned long lastPlayAgainCheck = 0;
int errorCount = 0;
const int MAX_ERRORS = 3;
bool singlePlayerMode = false;
bool dualPlayerMode = false;
unsigned long lastAutoTurnTime = 0;
const unsigned long AUTO_TURN_DELAY = 3000;
bool autoTurnInProgress = false;
uint8_t rainbowHue = 0;
int decoAnimationMode = 0;
int decoAnimationStep = 0;
unsigned long lastDecoUpdate = 0;
const int DECO_ANIMATION_SPEED = 50;
int decoHue = 0;
int decoBreathLevel = 30;
bool decoBreathDir = true;
unsigned long lastRingUpdate = 0;
const int RING_BLINK_SPEED = 300;
bool ringBlinkState = false;
unsigned long lastRainbowUpdate = 0;
const uint32_t DECO_COLORS[] = {
  0xFF0000, 0xFF5500, 0xFFAA00, 0xFFFF00, 0x00FF00,
  0x00FFFF, 0x0000FF, 0xAA00FF, 0xFF00FF
};
const int NUM_DECO_COLORS = sizeof(DECO_COLORS) / sizeof(DECO_COLORS[0]);
struct Jump {
  int start;
  int end;
  int ledCount;
  bool isValid;
};
Jump ladders[] = {
  {40, 42, 4, true}, {2, 36, 9, true}, {4, 14, 8, true},
  {9, 31, 8, true}, {33, 83, 17, true}, {71, 91, 5, true}
};
Jump snakes[] = {
  {99, 62, 5, true}, {64, 60, 7, true}, {95, 38, 14, true},
  {16, 6, 4, true}, {49, 11, 7, true}, {69, 51, 5, true}, {88, 67, 5, true}
};
const int numLadders = sizeof(ladders) / sizeof(ladders[0]);
const int numSnakes = sizeof(snakes) / sizeof(snakes[0]);
bool isValidMove(int currentPos, int diceRoll) {
  int targetPosition = currentPos + diceRoll;
  return targetPosition <= 100;
}
int calculateFinalPosition(int currentPos, int diceRoll) {
  int targetPosition = currentPos + diceRoll;
  if (targetPosition == 100) {
    return 100;
  }
  if (targetPosition > 100) {
    return currentPos;
  }
  return targetPosition;
}
CRGB parseHexColor(String hexColor) {
  if (hexColor.startsWith("#")) {
    hexColor = hexColor.substring(1);
  }
  long number = strtol(hexColor.c_str(), NULL, 16);
  uint8_t r = (number >> 16) & 0xFF;
  uint8_t g = (number >> 8) & 0xFF;
  uint8_t b = number & 0xFF;
  return CRGB(r, g, b);
}
void initializeSound() {
  mp3Serial.begin(9600);
  delay(500);
  if (mp3.begin(mp3Serial)) {
    soundInitialized = true;
    mp3.volume(20);
    delay(100);
    Serial.println("Sound initialized successfully");
  } else {
    Serial.println("Sound initialization failed");
  }
}
void startBackgroundMusic() {
  if (soundInitialized && !backgroundMusicPlaying) {
    mp3.stop();
    delay(50);
    mp3.loop(SOUND_BACKGROUND);
    backgroundMusicPlaying = true;
    Serial.println("Background music started");
  }
}
void stopBackgroundMusic() {
  if (soundInitialized && backgroundMusicPlaying) {
    mp3.stop();
    backgroundMusicPlaying = false;
    Serial.println("Background music stopped");
  }
}
void playEffectThenResumeMusic(int effect) {
  if (!soundInitialized) return;
  if (backgroundMusicPlaying) {
    mp3.stop();
    delay(50);
  }
  mp3.play(effect);
  Serial.println("Playing effect: " + String(effect));
  if (effect == SOUND_LADDER) {
    delay(3200);
  } else if (effect == SOUND_SNAKE) {
    delay(2200);
  } else if (effect == SOUND_WIN) {
    delay(3200);
    backgroundMusicPlaying = false;
    return;
  } else if (effect == SOUND_RESET) {
    delay(1200);
  } else if (effect == SOUND_INVALID) {
    delay(1500);
  }
  if (!gameWon && setupComplete) {
    delay(100);
    mp3.loop(SOUND_BACKGROUND);
    Serial.println("Background music resumed");
  }
}
void updatePreGameRainbow() {
  if (millis() - lastRainbowUpdate > 50) {
    for (int i = 0; i < NUM_GRID_LEDS; i++) {
      uint8_t hue = rainbowHue + (i * 255 / NUM_GRID_LEDS);
      gridLEDs[i] = CHSV(hue, 255, 255);
    }
    for (int i = 0; i < NUM_LADDER_LEDS; i++) {
      uint8_t hue = (rainbowHue + i * 10) % 255;
      ladderLEDs.setPixelColor(i, ladderLEDs.gamma32(ladderLEDs.ColorHSV(hue * 256, 255, 255)));
    }
    for (int i = 0; i < NUM_SNAKE_LEDS; i++) {
      uint8_t hue = (rainbowHue + i * 15) % 255;
      snakeLEDs.setPixelColor(i, snakeLEDs.gamma32(snakeLEDs.ColorHSV(hue * 256, 255, 255)));
    }
    fill_solid(decoLEDs, NUM_DECO_LEDS, CRGB::White);
    fill_solid(ringLEDs, NUM_RING_LEDS, CRGB::White);
    FastLED.show();
    ladderLEDs.show();
    snakeLEDs.show();
    rainbowHue++;
    lastRainbowUpdate = millis();
  }
}
uint32_t colorHSV(int hue) {
  hue = hue % 1536;
  int region = hue / 256;
  int remainder = hue % 256;
  uint8_t p = 0, q = 255 - remainder, t = remainder, v = 255;
  uint8_t r, g, b;
  switch (region) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
  }
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
void rainbowEffect() {
  for (int i = 0; i < NUM_DECO_LEDS; i++) {
    int pixelHue = decoHue + (i * 1536 / NUM_DECO_LEDS);
    decoLEDs[i] = CRGB(colorHSV(pixelHue));
  }
  decoHue += 15;
  if (decoHue >= 1536) decoHue = 0;
}
void breathingEffect() {
  if (decoBreathDir) {
    decoBreathLevel += 5;
    if (decoBreathLevel >= 255) {
      decoBreathLevel = 255;
      decoBreathDir = false;
    }
  } else {
    decoBreathLevel -= 5;
    if (decoBreathLevel <= 30) {
      decoBreathLevel = 30;
      decoBreathDir = true;
    }
  }
  uint32_t color = DECO_COLORS[decoAnimationStep % NUM_DECO_COLORS];
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  r = r * decoBreathLevel / 255;
  g = g * decoBreathLevel / 255;
  b = b * decoBreathLevel / 255;
  for (int i = 0; i < NUM_DECO_LEDS; i++) {
    decoLEDs[i] = CRGB(r, g, b);
  }
  if (decoBreathLevel == 30 && !decoBreathDir) decoAnimationStep++;
}
void chaseEffect() {
  fill_solid(decoLEDs, NUM_DECO_LEDS, CRGB::Black);
  for (int i = 0; i < 15; i++) {
    int pos = (decoAnimationStep + i) % NUM_DECO_LEDS;
    uint32_t color = DECO_COLORS[(i / 3) % NUM_DECO_COLORS];
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    decoLEDs[pos] = CRGB(r, g, b);
  }
  decoAnimationStep = (decoAnimationStep + 1) % NUM_DECO_LEDS;
}
void alternatingEffect() {
  for (int i = 0; i < NUM_DECO_LEDS; i++) {
    int colorIndex = ((i + decoAnimationStep) / 5) % NUM_DECO_COLORS;
    uint32_t color = DECO_COLORS[colorIndex];
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    decoLEDs[i] = CRGB(r, g, b);
  }
  decoAnimationStep = (decoAnimationStep + 1) % 5;
}
void gameStatePattern(bool isPlayerOneTurn, bool isGameWon, int winner) {
  if (isGameWon) {
    CRGB winnerColor = (winner == 0) ? CRGB::Red : CRGB::Green;
    CRGB celebrationColor = (decoAnimationStep % 2 == 0) ? winnerColor : CRGB::Gold;
    fill_solid(decoLEDs, NUM_DECO_LEDS, celebrationColor);
    decoAnimationStep++;
  } else {
    CRGB playerColor = isPlayerOneTurn ? CRGB(255, 50, 50) : CRGB(50, 255, 50);
    int pulseLevel = 128 + sin(decoAnimationStep * 0.1) * 127;
    playerColor.fadeToBlackBy(255 - pulseLevel);
    fill_solid(decoLEDs, NUM_DECO_LEDS, playerColor);
    decoAnimationStep++;
  }
}
void updateDecoLEDs() {
  if (millis() - lastDecoUpdate < DECO_ANIMATION_SPEED) return;
  lastDecoUpdate = millis();
  switch (decoAnimationMode) {
    case 0: rainbowEffect(); break;
    case 1: breathingEffect(); break;
    case 2: chaseEffect(); break;
    case 3: alternatingEffect(); break;
    case 4: break;
    case 5: gameStatePattern(currentPlayer == 0, gameWon, winner); break;
    default: rainbowEffect();
  }
}
void setDecoAnimationMode(int mode) {
  if (mode >= 0 && mode <= 5) {
    decoAnimationMode = mode;
    decoAnimationStep = 0;
    decoHue = 0;
    decoBreathLevel = 30;
    decoBreathDir = true;
  }
}
void decoLadderEffect() {
  for (int flash = 0; flash < 3; flash++) {
    fill_solid(decoLEDs, NUM_DECO_LEDS, CRGB::Green);
    FastLED.show();
    delay(100);
    fill_solid(decoLEDs, NUM_DECO_LEDS, CRGB(100, 255, 100));
    FastLED.show();
    delay(100);
  }
}
void decoSnakeEffect() {
  for (int flash = 0; flash < 3; flash++) {
    fill_solid(decoLEDs, NUM_DECO_LEDS, CRGB::Red);
    FastLED.show();
    delay(100);
    fill_solid(decoLEDs, NUM_DECO_LEDS, CRGB(255, 100, 100));
    FastLED.show();
    delay(100);
  }
}
void decoInvalidMoveEffect() {
  for (int flash = 0; flash < 4; flash++) {
    fill_solid(decoLEDs, NUM_DECO_LEDS, CRGB::Magenta);
    FastLED.show();
    delay(200);
    fill_solid(decoLEDs, NUM_DECO_LEDS, CRGB::Purple);
    FastLED.show();
    delay(200);
  }
}
void decoWinEffect() {
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < NUM_DECO_LEDS; j++) {
      if (random(5) == 0) {
        decoLEDs[j] = CRGB::Gold;
      } else {
        decoLEDs[j] = CRGB::Black;
      }
    }
    FastLED.show();
    delay(100);
  }
}
void updatePlayerRingIndicator(int currentPlayer) {
  if (millis() - lastRingUpdate > RING_BLINK_SPEED) {
    ringBlinkState = !ringBlinkState;
    lastRingUpdate = millis();
  }
  fill_solid(ringLEDs, NUM_RING_LEDS, CRGB::Black);
  for (int i = 0; i < 16; i++) {
    if (currentPlayer == 1) {
      if (ringBlinkState) {
        ringLEDs[i] = CRGB::White;
      } else {
        ringLEDs[i] = CRGB(50, 50, 50);
      }
    } else {
      ringLEDs[i] = CRGB(20, 20, 20);
    }
  }
  for (int i = 16; i < 32; i++) {
    if (currentPlayer == 0) {
      if (ringBlinkState) {
        ringLEDs[i] = CRGB::White;
      } else {
        ringLEDs[i] = CRGB(50, 50, 50);
      }
    } else {
      ringLEDs[i] = CRGB(20, 20, 20);
    }
  }
}
void playerRingWinEffect(int winner) {
  for (int flash = 0; flash < 10; flash++) {
    fill_solid(ringLEDs, NUM_RING_LEDS, CRGB::Black);
    if (winner == 0) {
      for (int i = 16; i < 32; i++) {
        ringLEDs[i] = CRGB::White;
      }
    } else {
      for (int i = 0; i < 16; i++) {
        ringLEDs[i] = CRGB::White;
      }
    }
    FastLED.show();
    delay(100);
    fill_solid(ringLEDs, NUM_RING_LEDS, CRGB::Black);
    FastLED.show();
    delay(100);
  }
  if (winner == 0) {
    for (int i = 16; i < 32; i++) {
      ringLEDs[i] = CRGB::White;
    }
  } else {
    for (int i = 0; i < 16; i++) {
      ringLEDs[i] = CRGB::White;
    }
  }
}
void playerRingInvalidMoveEffect(int player) {
  int startIndex = (player == 1) ? 0 : 16;
  int endIndex = startIndex + 16;
  for (int flash = 0; flash < 6; flash++) {
    for (int i = startIndex; i < endIndex; i++) {
      ringLEDs[i] = CRGB::Red;
    }
    FastLED.show();
    delay(150);
    for (int i = startIndex; i < endIndex; i++) {
      ringLEDs[i] = CRGB(128, 0, 0);
    }
    FastLED.show();
    delay(150);
  }
}
void handleESP32Communication() {
  if (Serial1.available()) {
    String line = Serial1.readStringUntil('\n');
    line.trim();
    if (line == "GAME_SETUP") {
      receivingSetup = true;
      setupComplete = false;
      mode = gameId = p1Name = p1Color = p2Name = p2Color = "";
      Serial.println("Receiving game setup...");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Receiving Setup...");
      lcd.setCursor(0, 1);
      lcd.print("From ESP32");
    }
    else if (line == "START" && receivingSetup) {
      receivingSetup = false;
      setupComplete = true;
      initializePlayersFromESP32();
      Serial.println("Game Setup Received:");
      Serial.println("  Mode: " + mode);
      Serial.println("  Game ID: " + gameId);
      Serial.println("  Player 1: " + p1Name + " (" + p1Color + ")");
      Serial.println("  Player 2: " + p2Name + " (" + p2Color + ")");
      Serial.println("Starting game...");
      gameStarted = true;
      startBackgroundMusic();
      setDecoAnimationMode(1);
      showAllLEDsGameEffect();
      showPlayerIntroduction();
      delay(3000);
      showGameStatus();
    }
    else if (line == "PLAY_AGAIN") {
      Serial.println("Play Again command received from ESP32");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PLAY AGAIN REQUEST");
      lcd.setCursor(0, 1);
      lcd.print("From Web Interface");
      lcd.setCursor(0, 2);
      lcd.print("Restarting game...");
      delay(1000);
      performPlayAgain();
      Serial.println("Play Again executed successfully");
    }
    else if (line == "END_GAME") {
      Serial.println("End Game command received from ESP32");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("END GAME REQUEST");
      lcd.setCursor(0, 1);
      lcd.print("From Web Interface");
      lcd.setCursor(0, 2);
      lcd.print("Ending game...");
      delay(1000);
      performFullReset();
      showWaitingScreen();
      Serial.println("End Game executed - System reset to waiting state");
    }
    else if (receivingSetup) {
      if (line.startsWith("MODE:")) {
        mode = line.substring(5);
      } else if (line.startsWith("GAMEID:")) {
        gameId = line.substring(7);
      } else if (line.startsWith("P1NAME:")) {
        p1Name = line.substring(7);
      } else if (line.startsWith("P1COLOR:")) {
        p1Color = line.substring(8);
      } else if (line.startsWith("P2NAME:")) {
        p2Name = line.substring(7);
      } else if (line.startsWith("P2COLOR:")) {
        p2Color = line.substring(8);
      } else {
        Serial.println("Unknown line: " + line);
      }
    }
    else {
      Serial.println("ESP32 Message: " + line);
    }
  }
}
void initializePlayersFromESP32() {
  players[0].name = p1Name.length() > 8 ? p1Name.substring(0, 8) : p1Name;
  players[1].name = p2Name.length() > 8 ? p2Name.substring(0, 8) : p2Name;
  players[0].colorStr = p1Color;
  players[1].colorStr = p2Color;
  players[0].colorCode = parseHexColor(p1Color);
  players[1].colorCode = parseHexColor(p2Color);
  for (int i = 0; i < 2; i++) {
    players[i].position = 1;
    players[i].score = 0;
  }
  if (mode.equalsIgnoreCase("single")) {
    singlePlayerMode = true;
    dualPlayerMode = false;
  } else {
    singlePlayerMode = false;
    dualPlayerMode = true;
  }
  Serial.println("Players initialized with custom colors:");
  Serial.println("Player 1: " + players[0].name + " (" + players[0].colorStr + ")");
  Serial.println("Player 2: " + players[1].name + " (" + players[1].colorStr + ")");
  Serial.println("Mode: " + String(singlePlayerMode ? "Single" : "Dual"));
}
void sendDiceResultToESP32(int playerNumber, int diceValue) {
  String dataToSend = String(playerNumber) + "," + String(diceValue);
  Serial1.println(dataToSend);
  Serial.println("Sent to ESP32: " + dataToSend);
}
int calculateSpeedBasedWaitTime(int maxSpeed, unsigned long accelerationTime, unsigned long runTime) {
  int baseWait = 5;
  int maxWait = 8;
  float speedFactor = (float)maxSpeed / MOTOR_MAX_SPEED;
  float accelFactor = min(1.0, (float)accelerationTime / 2000.0);
  float runFactor = min(1.0, (float)runTime / 5000.0);
  float combinedFactor = (speedFactor * 0.5) + (accelFactor * 0.3) + (runFactor * 0.2);
  int waitTime = baseWait + (int)((maxWait - baseWait) * combinedFactor);
  Serial.println("Speed-based timing:");
  Serial.println("  Max Speed: " + String(maxSpeed) + " (" + String(speedFactor * 100, 1) + "%)");
  Serial.println("  Wait Time: " + String(waitTime) + "s");
  return waitTime;
}
void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
  Serial.println("=== SNAKE & LADDER GAME - INTEGRATED EFFECTS ===");
  Serial.println("Mega ready to receive game setup from ESP32...");
  FastLED.addLeds<WS2812B, GRID_PIN, GRB>(gridLEDs, NUM_GRID_LEDS);
  FastLED.addLeds<WS2812B, DECO_PIN, GRB>(decoLEDs, NUM_DECO_LEDS);
  FastLED.addLeds<WS2812B, RING_PIN, GRB>(ringLEDs, NUM_RING_LEDS);
  FastLED.setBrightness(GRID_BRIGHTNESS);
  ladderLEDs.begin();
  snakeLEDs.begin();
  ladderLEDs.setBrightness(255);
  snakeLEDs.setBrightness(255);
  showStartupLEDSequence();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  pinMode(motorEN, OUTPUT);
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  pinMode(resetButtonPin, INPUT_PULLUP);
  pinMode(playAgainPin, INPUT_PULLUP);
  for (int i = 0; i < 6; i++) pinMode(hallPins[i], INPUT_PULLUP);
  digitalWrite(motorPin1, LOW);
  digitalWrite(motorPin2, LOW);
  analogWrite(motorEN, 0);
  initializeSound();
  showWaitingScreen();
  systemReady = true;
}
void loop() {
  if (!systemReady) {
    handleSystemError();
    return;
  }
  handleESP32Communication();
  if (!setupComplete) {
    showWaitingForSetup();
    updatePreGameRainbow();
    return;
  }
  if (gameWon) {
    showWinScreen();
    checkPlayAgainButton();
    return;
  }
  checkResetButton();
  updateDecoLEDs();
  updatePlayerRingIndicator(currentPlayer);
  FastLED.show();
  if (singlePlayerMode) {
    handleSinglePlayerInput();
  } else if (dualPlayerMode) {
    handleDualPlayerInput();
  }
  delay(10);
}
void showStartupLEDSequence() {
  Serial.println("Starting integrated LED sequence");
  fill_solid(gridLEDs, NUM_GRID_LEDS, CRGB::Black);
  fill_solid(decoLEDs, NUM_DECO_LEDS, CRGB::Black);
  fill_solid(ringLEDs, NUM_RING_LEDS, CRGB::Black);
  ladderLEDs.clear();
  snakeLEDs.clear();
  FastLED.show();
  ladderLEDs.show();
  snakeLEDs.show();
  delay(500);
  setDecoAnimationMode(0);
  for (int cycle = 0; cycle < 100; cycle++) {
    updatePreGameRainbow();
    delay(50);
  }
}
void showAllLEDsGameEffect() {
  Serial.println("Showing all LEDs game effect");
  for (int flash = 0; flash < 2; flash++) {
    drawGameBoard();
    for (int i = 0; i < NUM_LADDER_LEDS; i++) {
      ladderLEDs.setPixelColor(i, ladderLEDs.Color(0, 255, 0));
    }
    for (int i = 0; i < NUM_SNAKE_LEDS; i++) {
      snakeLEDs.setPixelColor(i, snakeLEDs.Color(255, 0, 0));
    }
    fill_solid(decoLEDs, NUM_DECO_LEDS, CRGB::Blue);
    FastLED.show();
    ladderLEDs.show();
    snakeLEDs.show();
    delay(400);
    FastLED.setBrightness(120);
    ladderLEDs.setBrightness(120);
    snakeLEDs.setBrightness(120);
    FastLED.show();
    ladderLEDs.show();
    snakeLEDs.show();
    delay(200);
    FastLED.setBrightness(GRID_BRIGHTNESS);
    ladderLEDs.setBrightness(150);
    snakeLEDs.setBrightness(150);
  }
  drawGameBoard();
}
void handleSinglePlayerInput() {
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;
  if (waitingForNext) {
    if (digitalRead(button1Pin) == HIGH && digitalRead(button2Pin) == HIGH) {
      if (millis() - lastDebounceTime > debounceDelay) {
        waitingForNext = false;
        if (!gameWon) {
          showGameStatus();
          drawGameBoard();
        }
        lastDebounceTime = millis();
      }
    }
    return;
  }
  if (currentPlayer == 0) {
    if (digitalRead(button1Pin) == LOW) {
      if (millis() - lastDebounceTime > debounceDelay) {
        handleEnhancedTurn(0);
        if (!gameWon) {
          currentPlayer = 1;
          lastAutoTurnTime = millis();
        }
        lastDebounceTime = millis();
      }
    }
  } else {
    if (!autoTurnInProgress && millis() - lastAutoTurnTime > AUTO_TURN_DELAY) {
      handleAutomaticTurn(1);
      if (!gameWon) {
        currentPlayer = 0;
        waitingForNext = true;
      }
    }
  }
}
void handleDualPlayerInput() {
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;
  if (waitingForNext) {
    if (digitalRead(button1Pin) == HIGH && digitalRead(button2Pin) == HIGH) {
      if (millis() - lastDebounceTime > debounceDelay) {
        waitingForNext = false;
        if (!gameWon) {
          showGameStatus();
          drawGameBoard();
        }
        lastDebounceTime = millis();
      }
    }
    return;
  }
  if (currentPlayer == 0 && digitalRead(button1Pin) == LOW) {
    if (millis() - lastDebounceTime > debounceDelay) {
      handleEnhancedTurn(0);
      if (!gameWon) {
        currentPlayer = 1;
        waitingForNext = true;
      }
      lastDebounceTime = millis();
    }
  }
  else if (currentPlayer == 1 && digitalRead(button2Pin) == LOW) {
    if (millis() - lastDebounceTime > debounceDelay) {
      handleEnhancedTurn(1);
      if (!gameWon) {
        currentPlayer = 0;
        waitingForNext = true;
      }
      lastDebounceTime = millis();
    }
  }
}
void showWaitingScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SNAKE & LADDER GAME");
  lcd.setCursor(0, 1);
  lcd.print("Integrated Effects");
  lcd.setCursor(0, 2);
  lcd.print("Waiting for ESP32...");
  lcd.setCursor(0, 3);
  lcd.print("Rainbow mode active");
}
void showWaitingForSetup() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 3000) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Waiting for game");
    lcd.setCursor(0, 1);
    lcd.print("setup from ESP32...");
    lcd.setCursor(0, 2);
    lcd.print("Check web interface");
    lcd.setCursor(0, 3);
    lcd.print("Rainbow + S&L active");
    lastUpdate = millis();
  }
}
void showPlayerIntroduction() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Players Ready!");
  lcd.setCursor(0, 1);
  lcd.print("P1: " + players[0].name);
  lcd.setCursor(0, 2);
  lcd.print("P2: " + players[1].name);
  lcd.setCursor(0, 3);
  lcd.print("Mode: " + String(singlePlayerMode ? "Single" : "Dual"));
}
void showGameStatus() {
  Serial.println("DEBUG: showGameStatus() called");
  lcd.clear();
  lcd.setCursor(0, 0);
  if (singlePlayerMode) {
    if (currentPlayer == 0) {
      lcd.print(players[0].name + "'s Turn");
      lcd.setCursor(0, 1);
      lcd.print("Press button to spin!");
    } else {
      lcd.print(players[1].name + "'s Turn");
      lcd.setCursor(0, 1);
      lcd.print("Auto rolling...");
    }
  } else {
    lcd.print(players[currentPlayer].name + "'s Turn");
    lcd.setCursor(0, 1);
    lcd.print("Hold button to spin!");
  }
  lcd.setCursor(0, 2);
  lcd.print(players[0].name + ":" + String(players[0].position) +
            " " + players[1].name + ":" + String(players[1].position));
  int neededScore = 100 - players[currentPlayer].position;
  if (players[currentPlayer].position > 94) {
    lcd.setCursor(0, 3);
    lcd.print("Need exactly: " + String(neededScore));
  } else {
    lcd.setCursor(0, 3);
    lcd.print("Integrated Effects");
  }
  Serial.println("DEBUG: showGameStatus() completed");
}
void showWinScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("GAME COMPLETE!");
  lcd.setCursor(0, 1);
  lcd.print(players[winner].name + " WINS!");
  lcd.setCursor(0, 2);
  lcd.print("Final Positions:");
  lcd.setCursor(0, 3);
  lcd.print(players[0].name + ":" + String(players[0].position) +
            " " + players[1].name + ":" + String(players[1].position));
  static unsigned long lastFlash = 0;
  static bool showMessage = true;
  if (millis() - lastFlash > 1000) {
    if (showMessage) {
      lcd.setCursor(0, 1);
      lcd.print(players[winner].name + " WINS!");
    } else {
      lcd.setCursor(0, 1);
      lcd.print("Press Play Again!");
    }
    showMessage = !showMessage;
    lastFlash = millis();
  }
  showWinnerCelebration();
}
void showWinnerCelebration() {
  static unsigned long lastCelebration = 0;
  static int celebrationStep = 0;
  if (millis() - lastCelebration > 150) {
    CRGB winnerColor = players[winner].colorCode;
    CRGB goldColor = CRGB::Gold;
    if (celebrationStep % 2 == 0) {
      fill_solid(gridLEDs, NUM_GRID_LEDS, winnerColor);
      fill_solid(decoLEDs, NUM_DECO_LEDS, winnerColor);
      for (int i = 0; i < NUM_LADDER_LEDS; i++) {
        ladderLEDs.setPixelColor(i, ladderLEDs.Color(winnerColor.r, winnerColor.g, winnerColor.b));
      }
      for (int i = 0; i < NUM_SNAKE_LEDS; i++) {
        snakeLEDs.setPixelColor(i, snakeLEDs.Color(winnerColor.r, winnerColor.g, winnerColor.b));
      }
    } else {
      fill_solid(gridLEDs, NUM_GRID_LEDS, goldColor);
      fill_solid(decoLEDs, NUM_DECO_LEDS, goldColor);
      for (int i = 0; i < NUM_LADDER_LEDS; i++) {
        ladderLEDs.setPixelColor(i, ladderLEDs.Color(255, 215, 0));
      }
      for (int i = 0; i < NUM_SNAKE_LEDS; i++) {
        snakeLEDs.setPixelColor(i, snakeLEDs.Color(255, 215, 0));
      }
    }
    FastLED.show();
    ladderLEDs.show();
    snakeLEDs.show();
    celebrationStep++;
    lastCelebration = millis();
  }
}
void handleSystemError() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM ERROR");
  lcd.setCursor(0, 1);
  lcd.print("Please restart");
  static unsigned long lastFlash = 0;
  if (millis() - lastFlash > 500) {
    fill_solid(gridLEDs, NUM_GRID_LEDS, CRGB::Red);
    FastLED.show();
    delay(100);
    fill_solid(gridLEDs, NUM_GRID_LEDS, CRGB::Black);
    FastLED.show();
    lastFlash = millis();
  }
}
void checkResetButton() {
  if (millis() - lastResetCheck > 200) {
    if (digitalRead(resetButtonPin) == LOW) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("RESET GAME?");
      lcd.setCursor(0, 1);
      lcd.print("Hold button for 1s");
      int holdTime = 0;
      while (digitalRead(resetButtonPin) == LOW && holdTime < 1000) {
        delay(100);
        holdTime += 100;
        lcd.setCursor(0, 3);
        lcd.print("Holding: " + String(holdTime / 1000.0, 1) + "s   ");
      }
      if (holdTime >= 1000) {
        performFullReset();
      } else {
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("  Reset canceled  ");
        delay(1000);
        if (gameWon) {
          showWinScreen();
        } else {
          showGameStatus();
        }
      }
      lastResetCheck = millis();
    }
  }
}
void checkPlayAgainButton() {
  if (millis() - lastPlayAgainCheck > 200) {
    if (digitalRead(playAgainPin) == LOW) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PLAY AGAIN?");
      lcd.setCursor(0, 1);
      lcd.print("Hold button for 1s");
      int holdTime = 0;
      while (digitalRead(playAgainPin) == LOW && holdTime < 1000) {
        delay(100);
        holdTime += 100;
        lcd.setCursor(0, 3);
        lcd.print("Holding: " + String(holdTime / 1000.0, 1) + "s   ");
      }
      if (holdTime >= 1000) {
        performPlayAgain();
      } else {
        showWinScreen();
      }
      lastPlayAgainCheck = millis();
    }
  }
}
void performFullReset() {
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("   FULL RESET!   ");
  stopBackgroundMusic();
  playEffectThenResumeMusic(SOUND_RESET);
  Serial1.println("RESET_GAME");
  Serial.println("Reset signal sent to ESP32");
  setupComplete = false;
  gameStarted = false;
  gameWon = false;
  winner = -1;
  currentPlayer = 0;
  waitingForNext = false;
  errorCount = 0;
  autoTurnInProgress = false;
  lastAutoTurnTime = 0;
  for (int i = 0; i < 2; i++) {
    players[i].name = "";
    players[i].colorStr = "";
    players[i].colorCode = CRGB::Black;
    players[i].position = 1;
    players[i].score = 0;
  }
  for (int i = 0; i < 5; i++) {
    fill_solid(gridLEDs, NUM_GRID_LEDS, CRGB::Blue);
    fill_solid(decoLEDs, NUM_DECO_LEDS, CRGB::Blue);
    fill_solid(ringLEDs, NUM_RING_LEDS, CRGB::Blue);
    FastLED.show();
    for (int j = 0; j < NUM_LADDER_LEDS; j++) {
      ladderLEDs.setPixelColor(j, ladderLEDs.Color(0, 0, 255));
    }
    for (int j = 0; j < NUM_SNAKE_LEDS; j++) {
      snakeLEDs.setPixelColor(j, snakeLEDs.Color(0, 0, 255));
    }
    ladderLEDs.show();
    snakeLEDs.show();
    delay(200);
    fill_solid(gridLEDs, NUM_GRID_LEDS, CRGB::Black);
    fill_solid(decoLEDs, NUM_DECO_LEDS, CRGB::Black);
    fill_solid(ringLEDs, NUM_RING_LEDS, CRGB::Black);
    FastLED.show();
    ladderLEDs.clear();
    snakeLEDs.clear();
    ladderLEDs.show();
    snakeLEDs.show();
    delay(200);
  }
  showStartupLEDSequence();
  showWaitingScreen();
  Serial.println("Full reset completed and signal sent to ESP32");
}
void performPlayAgain() {
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("  NEW GAME!  ");
  gameWon = false;
  winner = -1;
  currentPlayer = 0;
  waitingForNext = false;
  errorCount = 0;
  autoTurnInProgress = false;
  lastAutoTurnTime = 0;
  for (int i = 0; i < 2; i++) {
    players[i].position = 1;
    players[i].score = 0;
  }
  startBackgroundMusic();
  setDecoAnimationMode(1);
  showAllLEDsGameEffect();
  drawGameBoard();
  delay(2000);
  showGameStatus();
  Serial.println("Play Again - New game started");
}
void handleAutomaticTurn(int player) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(players[player].name + " (Auto)");
  lcd.setCursor(0, 1);
  lcd.print("Rolling dice...");
  autoTurnInProgress = true;
  for (int i = 0; i < 3; i++) {
    lcd.setCursor(0, 2);
    lcd.print("Rolling" + String(".").substring(0, (i % 3) + 1) + "   ");
    delay(500);
  }
  int diceScore = performAutomaticDiceRollWithTiming();
  if (diceScore == 0) {
    handleDiceError(player);
    autoTurnInProgress = false;
    return;
  }
  lcd.setCursor(0, 2);
  lcd.print("Rolled: " + String(diceScore) + "       ");
  delay(1000);
  sendDiceResultToESP32(player + 1, diceScore);
  showAllLEDsGameEffect();
  processMoveWithValidation(player, diceScore);
  autoTurnInProgress = false;
  lastAutoTurnTime = millis();
  delay(2000);
}
int performAutomaticDiceRollWithTiming() {
  int motorSpeed = ROBOT_MIN_SPEED;
  unsigned long accelStart = millis();
  int maxSpeed = ROBOT_MIN_SPEED;
  while (motorSpeed < ROBOT_MAX_SPEED) {
    motorSpeed += 1;
    if (motorSpeed > ROBOT_MAX_SPEED) motorSpeed = ROBOT_MAX_SPEED;
    analogWrite(motorEN, motorSpeed);
    digitalWrite(motorPin1, HIGH);
    digitalWrite(motorPin2, LOW);
    if (motorSpeed > maxSpeed) maxSpeed = motorSpeed;
    lcd.setCursor(0, 3);
    lcd.print("Robot Speed: " + String(motorSpeed) + "   ");
    delay(20);
  }
  motorAccelerationTime = millis() - accelStart;
  int runDuration = random(2000, 4500);
  unsigned long startTime = millis();
  while (millis() - startTime < runDuration) {
    int position = getPrecisePosition();
    lcd.setCursor(0, 3);
    lcd.print("Robot Pos: " + String(position) + "   ");
    delay(70);
  }
  motorRunTime = millis() - startTime + motorAccelerationTime;
  lastMotorSpeed = maxSpeed;
  while (motorSpeed > 0) {
    motorSpeed -= 1;
    if (motorSpeed < 0) motorSpeed = 0;
    analogWrite(motorEN, motorSpeed);
    lcd.setCursor(0, 3);
    lcd.print("Robot Speed: " + String(motorSpeed) + "   ");
    delay(20);
  }
  analogWrite(motorEN, 0);
  digitalWrite(motorPin1, LOW);
  digitalWrite(motorPin2, LOW);
  return waitForPreciseStopWithTiming();
}
void handleEnhancedTurn(int player) {
  Serial.println("DEBUG: handleEnhancedTurn() called for player " + String(player));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(players[player].name + "'s Turn");
  lcd.setCursor(0, 1);
  lcd.print("Hold button to spin!");
  lcd.setCursor(0, 2);
  lcd.print("Release to stop");
  int diceScore = controlPrecisionMotorWithTiming(player);
  if (diceScore == 0) {
    handleDiceError(player);
    return;
  }
  sendDiceResultToESP32(player + 1, diceScore);
  showAllLEDsGameEffect();
  processMoveWithValidation(player, diceScore);
  delay(2000);
  Serial.println("DEBUG: handleEnhancedTurn() completed");
}
int controlPrecisionMotorWithTiming(int player) {
  int buttonPin = (player == 0) ? button1Pin : button2Pin;
  unsigned long buttonStartTime = 0;
  unsigned long accelerationStartTime = 0;
  unsigned long buttonDuration = 0;
  int currentSpeed = MOTOR_MIN_SPEED;
  int maxSpeedReached = MOTOR_MIN_SPEED;
  unsigned long waitStart = millis();
  while (digitalRead(buttonPin) == HIGH) {
    if (millis() - waitStart > 30000) return 0;
    delay(10);
  }
  buttonStartTime = millis();
  accelerationStartTime = millis();
  while (digitalRead(buttonPin) == LOW && currentSpeed < MOTOR_MAX_SPEED) {
    currentSpeed += 2;
    if (currentSpeed > MOTOR_MAX_SPEED) currentSpeed = MOTOR_MAX_SPEED;
    analogWrite(motorEN, currentSpeed);
    digitalWrite(motorPin1, HIGH);
    digitalWrite(motorPin2, LOW);
    if (currentSpeed > maxSpeedReached) {
      maxSpeedReached = currentSpeed;
    }
    lcd.setCursor(0, 3);
    lcd.print("Speed: " + String(currentSpeed) + "   ");
    delay(15);
  }
  motorAccelerationTime = millis() - accelerationStartTime;
  while (digitalRead(buttonPin) == LOW) {
    int position = getPrecisePosition();
    lcd.setCursor(0, 2);
    lcd.print("Pos: " + String(position) + "   ");
    delay(50);
  }
  buttonDuration = millis() - buttonStartTime;
  motorRunTime = buttonDuration;
  lastMotorSpeed = maxSpeedReached;
  while (currentSpeed > 0) {
    currentSpeed -= 2;
    if (currentSpeed < 0) currentSpeed = 0;
    analogWrite(motorEN, currentSpeed);
    lcd.setCursor(0, 3);
    lcd.print("Speed: " + String(currentSpeed) + "   ");
    delay(15);
  }
  analogWrite(motorEN, 0);
  digitalWrite(motorPin1, LOW);
  digitalWrite(motorPin2, LOW);
  return waitForPreciseStopWithTiming();
}
int waitForPreciseStopWithTiming() {
  int waitTime = calculateSpeedBasedWaitTime(lastMotorSpeed, motorAccelerationTime, motorRunTime);
  int lastPosition = 0;
  int stableCount = 0;
  const int requiredStableReadings = 3;
  lcd.setCursor(0, 1);
  lcd.print("Wheel spinning...   ");
  for (int countdown = waitTime; countdown > 0; countdown--) {
    int currentPosition = getPrecisePosition();
    if (currentPosition == lastPosition && currentPosition != 0) {
      stableCount++;
    } else {
      stableCount = 0;
      lastPosition = currentPosition;
    }
    if (stableCount >= requiredStableReadings && countdown > 2) countdown = 2;
    lcd.setCursor(0, 2);
    lcd.print("Pos: " + String(currentPosition) + "   ");
    lcd.setCursor(0, 3);
    lcd.print("Wait: " + String(countdown) + "s   ");
    delay(1000);
  }
  int finalResult = getFinalPreciseReading();
  lcd.setCursor(0, 1);
  lcd.print("Wheel stopped!      ");
  lcd.setCursor(0, 2);
  lcd.print("Final: " + String(finalResult) + "     ");
  lcd.setCursor(0, 3);
  lcd.print("                    ");
  return finalResult;
}
int getPrecisePosition() {
  static int readings[PRECISION_READING_SAMPLES];
  static int readIndex = 0;
  int currentReading = 0;
  for (int i = 0; i < 6; i++) {
    if (digitalRead(hallPins[i]) == LOW) {
      currentReading = i + 1;
      break;
    }
  }
  readings[readIndex] = currentReading;
  readIndex = (readIndex + 1) % PRECISION_READING_SAMPLES;
  return findMostCommonReading(readings);
}
int findMostCommonReading(int readings[]) {
  int counts[7] = {0};
  for (int i = 0; i < PRECISION_READING_SAMPLES; i++) {
    if (readings[i] >= 0 && readings[i] <= 6) counts[readings[i]]++;
  }
  int mostCommon = 1;
  int maxCount = counts[1];
  for (int i = 2; i <= 6; i++) {
    if (counts[i] > maxCount) {
      maxCount = counts[i];
      mostCommon = i;
    }
  }
  if (maxCount < 2) {
    for (int i = 1; i <= 6; i++) {
      if (counts[i] > 0) return i;
    }
    return 0;
  }
  return mostCommon;
}
int getFinalPreciseReading() {
  int samples[20];
  int validSamples = 0;
  for (int i = 0; i < 20; i++) {
    int reading = 0;
    for (int j = 0; j < 6; j++) {
            if (digitalRead(hallPins[j]) == LOW) {
        reading = j + 1;
        break;
      }
    }
    if (reading > 0) samples[validSamples++] = reading;
    delay(100);
  }
  if (validSamples == 0) return 0;
  int counts[7] = {0};
  for (int i = 0; i < validSamples; i++) counts[samples[i]]++;
  int result = 1;
  int maxCount = counts[1];
  for (int i = 2; i <= 6; i++) {
    if (counts[i] > maxCount) {
      maxCount = counts[i];
      result = i;
    }
  }
  return result;
}
void processMoveWithValidation(int player, int diceScore) {
  Serial.println("DEBUG: processMoveWithValidation() called");
  int oldPosition = players[player].position;
  if (diceScore < 1 || diceScore > 6) {
    handleDiceError(player);
    return;
  }
  if (!isValidMove(oldPosition, diceScore)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(players[player].name + " rolled: " + String(diceScore));
    lcd.setCursor(0, 1);
    lcd.print("INVALID MOVE!");
    lcd.setCursor(0, 2);
    int needed = 100 - oldPosition;
    lcd.print("Need exactly " + String(needed) + " to win");
    lcd.setCursor(0, 3);
    lcd.print("Stay at " + String(oldPosition));
    showInvalidMoveEffect();
    decoInvalidMoveEffect();
    playerRingInvalidMoveEffect(player);
    playEffectThenResumeMusic(SOUND_INVALID);
    delay(3000);
    drawGameBoard();
    return;
  }
  int newPosition = calculateFinalPosition(oldPosition, diceScore);
  players[player].position = newPosition;
  players[player].score += diceScore;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(players[player].name + " rolled: " + String(diceScore));
  lcd.setCursor(0, 1);
  lcd.print("From " + String(oldPosition) + " to " + String(newPosition));
  delay(1000);
  if (players[player].position == 100) {
    gameWon = true;
    winner = player;
    lcd.setCursor(0, 2);
    lcd.print("EXACT LANDING!");
    lcd.setCursor(0, 3);
    lcd.print("WINNER!");
    showWinnerLEDs(player);
    playerRingWinEffect(player);
    decoWinEffect();
    playEffectThenResumeMusic(SOUND_WIN);
    delay(2000);
    return;
  }
  bool foundSpecial = checkSpecialSquares(player, newPosition);
  if (!foundSpecial) {
    lcd.setCursor(0, 2);
    lcd.print("Current: " + String(players[player].position));
    if (players[player].position > 94) {
      int needed = 100 - players[player].position;
      lcd.setCursor(0, 3);
      lcd.print("Need " + String(needed) + " to win exactly");
    }
  }
  drawGameBoard();
  delay(1500);
  Serial.println("DEBUG: processMoveWithValidation() completed");
}
void showInvalidMoveEffect() {
  int playerLedIndex = players[currentPlayer].position - 1;
  for (int flash = 0; flash < 6; flash++) {
    if (playerLedIndex >= 0 && playerLedIndex < NUM_GRID_LEDS) {
      gridLEDs[playerLedIndex] = CRGB::Magenta;
      FastLED.show();
      delay(150);
      gridLEDs[playerLedIndex] = CRGB::Purple;
      FastLED.show();
      delay(150);
    }
  }
  drawGameBoard();
}
bool checkSpecialSquares(int player, int position) {
  for (int i = 0; i < numLadders; i++) {
    if (ladders[i].isValid && position == ladders[i].start) {
      players[player].position = ladders[i].end;
      lcd.setCursor(0, 2);
      lcd.print("LADDER! Up to " + String(ladders[i].end));
      showSpecialEffect(true, ladders[i].start, ladders[i].end);
      playEffectThenResumeMusic(SOUND_LADDER);
      return true;
    }
  }
  for (int i = 0; i < numSnakes; i++) {
    if (snakes[i].isValid && position == snakes[i].start) {
      players[player].position = snakes[i].end;
      lcd.setCursor(0, 2);
      lcd.print("SNAKE! Down to " + String(snakes[i].end));
      showSpecialEffect(false, snakes[i].start, snakes[i].end);
      playEffectThenResumeMusic(SOUND_SNAKE);
      return true;
    }
  }
  return false;
}
void handleDiceError(int player) {
  errorCount++;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dice Reading Error!");
  lcd.setCursor(0, 1);
  lcd.print("Error #" + String(errorCount));
  if (errorCount >= MAX_ERRORS) {
    lcd.setCursor(0, 2);
    lcd.print("Too many errors!");
    lcd.setCursor(0, 3);
    lcd.print("Check connections");
    systemReady = false;
  } else {
    lcd.setCursor(0, 2);
    lcd.print("Try again...");
    lcd.setCursor(0, 3);
    lcd.print("Press button");
    delay(2000);
  }
}
void drawStaticElements() {
  for (int i = 0; i < NUM_GRID_LEDS; i++) {
    if ((i / 10) % 2 == 0) {
      gridLEDs[i] = CRGB(15, 15, 20);
    } else {
      gridLEDs[i] = CRGB(20, 15, 15);
    }
  }
  for (int i = 0; i < NUM_LADDER_LEDS; i++) {
    ladderLEDs.setPixelColor(i, ladderLEDs.Color(0, 180, 0));
  }
  for (int i = 0; i < NUM_SNAKE_LEDS; i++) {
    snakeLEDs.setPixelColor(i, snakeLEDs.Color(180, 0, 120));
  }
  FastLED.show();
  ladderLEDs.show();
  snakeLEDs.show();
}
void drawGameBoard() {
  Serial.println("DEBUG: drawGameBoard() called");
  drawStaticElements();
  int p1LedIndex = players[0].position - 1;
  int p2LedIndex = players[1].position - 1;
  static unsigned long lastBlinkUpdate = 0;
  static bool blinkState = false;
  const unsigned long PLAYER_BLINK_SPEED = 400;
  if (millis() - lastBlinkUpdate > PLAYER_BLINK_SPEED) {
    blinkState = !blinkState;
    lastBlinkUpdate = millis();
  }
  if (p1LedIndex == p2LedIndex && p1LedIndex >= 0 && p1LedIndex < NUM_GRID_LEDS) {
    if (blinkState) {
      gridLEDs[p1LedIndex] = players[0].colorCode;
      gridLEDs[p1LedIndex] %= PLAYER_BRIGHTNESS;
    } else {
      gridLEDs[p1LedIndex] = players[1].colorCode;
      gridLEDs[p1LedIndex] %= PLAYER_BRIGHTNESS;
    }
  } else {
    if (p1LedIndex >= 0 && p1LedIndex < NUM_GRID_LEDS) {
      gridLEDs[p1LedIndex] = players[0].colorCode;
      gridLEDs[p1LedIndex] %= PLAYER_BRIGHTNESS;
    }
    if (p2LedIndex >= 0 && p2LedIndex < NUM_GRID_LEDS) {
      gridLEDs[p2LedIndex] = players[1].colorCode;
      gridLEDs[p2LedIndex] %= PLAYER_BRIGHTNESS;
    }
  }
  FastLED.show();
  Serial.println("DEBUG: drawGameBoard() completed");
}
void showWinnerLEDs(int winner) {
  CRGB winnerColor = players[winner].colorCode;
  for (int flash = 0; flash < 5; flash++) {
    fill_solid(gridLEDs, NUM_GRID_LEDS, winnerColor);
    FastLED.show();
    delay(300);
    fill_solid(gridLEDs, NUM_GRID_LEDS, CRGB::Black);
    FastLED.show();
    delay(200);
  }
  drawGameBoard();
}
void showSpecialEffect(bool isLadder, int start, int end) {
  if (isLadder) {
    int ladderIndex = -1;
    for (int i = 0; i < numLadders; i++) {
      if (ladders[i].start == start && ladders[i].end == end) {
        ladderIndex = i;
        break;
      }
    }
    if (ladderIndex >= 0) {
      int ledIndex = 0;
      for (int i = 0; i < ladderIndex; i++) {
        if (ladders[i].isValid) {
          ledIndex += ladders[i].ledCount;
        }
      }
      for (int step = 0; step < ladders[ladderIndex].ledCount; step++) {
        for (int i = 0; i < NUM_LADDER_LEDS; i++) {
          ladderLEDs.setPixelColor(i, ladderLEDs.Color(0, 180, 0));
        }
        if ((ledIndex + step) < NUM_LADDER_LEDS) {
          ladderLEDs.setPixelColor(ledIndex + step, ladderLEDs.Color(255, 255, 255));
        }
        ladderLEDs.show();
        delay(150);
      }
    }
    for (int step = start; step <= end; step++) {
      int gridIndex = step - 1;
      if (gridIndex >= 0 && gridIndex < NUM_GRID_LEDS) {
        gridLEDs[gridIndex] = CRGB::Yellow;
        FastLED.show();
        delay(100);
      }
    }
    decoLadderEffect();
  } else {
    int snakeIndex = -1;
    for (int i = 0; i < numSnakes; i++) {
      if (snakes[i].start == start && snakes[i].end == end) {
        snakeIndex = i;
        break;
      }
    }
    if (snakeIndex >= 0) {
      int ledIndex = 0;
      for (int i = 0; i < snakeIndex; i++) {
        if (snakes[i].isValid) {
          ledIndex += snakes[i].ledCount;
        }
      }
      for (int step = 0; step < snakes[snakeIndex].ledCount; step++) {
        for (int i = 0; i < NUM_SNAKE_LEDS; i++) {
          snakeLEDs.setPixelColor(i, snakeLEDs.Color(180, 0, 120));
        }
        if ((ledIndex + step) < NUM_SNAKE_LEDS) {
          snakeLEDs.setPixelColor(ledIndex + step, snakeLEDs.Color(255, 0, 0));
        }
        snakeLEDs.show();
        delay(150);
      }
    }
    for (int step = start; step >= end; step--) {
      int gridIndex = step - 1;
      if (gridIndex >= 0 && gridIndex < NUM_GRID_LEDS) {
        gridLEDs[gridIndex] = CRGB::Red;
        FastLED.show();
        delay(100);
      }
    }
    decoSnakeEffect();
  }
  drawGameBoard();
}
