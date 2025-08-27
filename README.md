# Smart Snake and Ladder Game 🎲

A modern web-based implementation of the classic Snake and Ladder board game with smart features and interactive gameplay.

## 📋 Prerequisites

Before running this project, make sure you have the following installed on your system:

- Node.js (version 14.0 or higher)
- MongoDB(Community Edition)
- Git(for cloning the repository)

## 🚀 Installation & Setup

## Step 1: Clone the Repository
```bash
git clone https://github.com/yourusername/smart-snake-ladder-game.git
cd smart-snake-ladder-game
```

## Step 2: Install Dependencies
Navigate to the project directory and install required packages:
```bash
npm install
```

## Step 3: Start MongoDB
Make sure MongoDB is running on your system:
- **Windows**: Start MongoDB service from Services or run `mongod` in command prompt
- **macOS/Linux**: Run `sudo systemctl start mongod` or `brew services start mongodb-community`

🎯 Running the Application

## Step 1: Start the Backend Server
1. Navigate to the backend directory:
   ```bash
   cd ludogame/backend
   ```

2. Start the backend server:
   ```bash
   node index.js
   ```
   
   You should see a message indicating the server is running (typically on port 3000 or 5000).

## Step 2: Start the Frontend Development Server
1. Open a new terminal/command prompt
2. Navigate to the frontend path:
   ```bash
   cd ludogame/frontend
   ```

3. Start the development server:
   ```bash
   npm run dev
   ```

## Step 3: Access the Game
- Open your web browser
- Navigate to `http://localhost:3000` (or the port shown in your terminal)
- Start playing the Smart Snake and Ladder game!

## 🎮 How to Play

1. Open the game in your web browser
2. Select mode and enter names start game
3. For players ,push the push button to play
4. Land on a ladder to climb up, hit a snake to slide down
5. First player to reach the end wins!
