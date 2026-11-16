# Smart Snake & Ladder 🎲

An interactive Snake & Ladder board game that connects a real, physical board
to a full-stack web application. Players press a button on the board, and the
web app and the board LEDs stay in sync.

## Features

- **Physical board** with an LED grid, an automated spinning wheel and an LCD display
- **Full game logic**: snakes send you down and ladders take you up, automatically
- **Real-time sync** between player colors in the web UI and the LEDs on the board
- **Two modes**
  - Single Player: play against an automated robot
  - Dual Player: head-to-head on the physical board
- **Accounts**: sign up and log in before starting a game

## Tech stack

| Part | Technology |
|---|---|
| Hardware | Arduino Mega, ESP32, LED grid, LCD display, push buttons |
| Frontend | React, TypeScript, Vite, Tailwind CSS |
| Backend | Node.js, Express |
| Database | MongoDB |

## Project structure

```
├── arduino_mega.ino        # Board logic: LEDs, wheel, LCD, buttons
├── esp32.ino               # Wi-Fi bridge between the board and the web app
└── Ludogame/
    ├── backend/            # Express API (auth and game routes)
    └── frontend/           # React web app
```

## Getting started

You need Node.js 18 or newer and MongoDB.

### 1. Clone

```bash
git clone https://github.com/minidu10/smart-snake-and-ladder-game_titancore.git
cd smart-snake-and-ladder-game_titancore
```

### 2. Backend

```bash
cd Ludogame/backend
npm install
cp .env.example .env      # then edit the values
node index.js
```

The API runs on `http://localhost:5000`.

### 3. Frontend

Open a second terminal:

```bash
cd Ludogame/frontend
npm install
npm run dev
```

Open the address shown in the terminal (usually `http://localhost:5173`).

### 4. Hardware

1. Upload `arduino_mega.ino` to the Arduino Mega.
2. Upload `esp32.ino` to the ESP32.
3. Power the board. The ESP32 links the board to the web app.

## How to play

1. Sign up or log in.
2. Choose Single Player or Dual Player and enter the player names.
3. Press the push button on the board to spin.
4. Land on a ladder to climb up. Land on a snake to slide down.
5. The first player to reach the last square wins.

## License

MIT
