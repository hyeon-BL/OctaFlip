# OctaFlip Online: A Networked C Implementation 🎮

**OctaFlip Online** is a client-server implementation of the strategic 8x8 board game, OctaFlip. This project brings the classic turn-based game to life, allowing two players to register, compete in real-time, and outmaneuver each other through tactical piece cloning, jumping, and flipping. Built entirely in C, it leverages TCP/IP sockets for network communication and JSON for a structured data interchange protocol.

The server acts as the authoritative referee, managing game state, validating moves against the core OctaFlip rules, and synchronizing clients. Clients connect to the server, register with a unique username, and interact with the game through a command-line interface, receiving real-time updates.

## ✨ Key Features

* **Client-Server Architecture**: Robust server managing game logic and client interactions.
* **Real-time Two-Player Gameplay**: Engage in head-to-head OctaFlip matches.
* **TCP/IP Socket Communication**: Reliable, stream-oriented communication between server and clients.
* **JSON Messaging Protocol**: Structured and standardized data exchange for all game actions and updates.
* **Player Registration**: Secure a spot with a unique username; first-come, first-served for play order.
* **Server-Side Move Validation**: All moves are validated by the server against OctaFlip game rules.
* **Turn-Based System with Timeouts**: Strict 5-second turn-timers enforce a brisk game pace; automatic pass on timeout.
* **Dynamic Game State Updates**: Clients receive real-time board updates and game status notifications.
* **Simplified Game Over Condition**: For this version, the game concludes after each player has made exactly one move (two moves total).
* **Basic Disconnection Handling**: Server manages graceful turn passing if one client disconnects.

## 💻 Tech Stack

* **Language**: C (targeting C99/C11 standard for broad compatibility and modern features)
* **Networking**: POSIX Sockets API (TCP/IP)
    * `sys/socket.h`, `netdb.h`, `arpa/inet.h`
* **Data Interchange**: JSON
    * Utilizing a standard C JSON library (e.g., `cJSON` or `Jansson` as recommended)
* **Server-Side Concurrency**: I/O Multiplexing using `select()` (or `poll()`) for handling multiple clients efficiently.
* **Build System**: `gcc` (potentially with Makefiles for larger project organization - *TBD*)

## 📂 Project Structure

The project is primarily composed of three main components:

octaflip-online/
├── server.c          # Core server logic, game state management, client handling
├── client.c          # Client application, user interaction, server communication
├── protocol.h        # Shared definitions: message structs, constants, JSON schemas
└── README.md         # This file

* The core OctaFlip game rules (board mechanics, move validation from Assignment 1) are integrated within `server.c`.

## 🚀 Development Blueprint (Phased Approach)

This project is structured across several key development phases, building from core game logic to full network integration:

1.  **Phase 1: Core OctaFlip Game Logic (Stages 1-9)**
    * Board representation and initialization.
    * Implementing piece movements: Clone & Jump.
    * Flip mechanism for opponent pieces.
    * Comprehensive move validation (bounds, source, destination, distance).
    * Turn management and game flow simulation.
    * Advanced validation including pass moves.
    * Game termination conditions (no empty cells, no pieces, consecutive passes).
    * Winner determination and output formatting.
    * Input parsing and main simulation loop (for local play).

2.  **Phase 2: Network Foundation & Server Implementation (Stages 10-15)**
    * **Stage 10: Common Protocol & JSON Utilities**: Defining message structs in `protocol.h` and establishing interfaces for JSON (de)serialization.
    * **Stage 11 (Server)**: TCP socket setup, binding, listening, and basic non-blocking connection management using `select()`.
    * **Stage 12 (Server)**: Player registration logic (username uniqueness, waiting list), `register_ack`/`nack`, and game initiation (`game_start` broadcast) once two players are ready.
    * **Stage 13 (Server)**: Turn management, tracking current player, and sending `your_turn` notifications with board state and 5s timeout.
    * **Stage 14 (Server)**: Receiving `move` messages, server-side validation against game rules, handling timeouts (auto-pass), and broadcasting results (`move_ok`, `invalid_move`, `pass`).
    * **Stage 15 (Server)**: Implementing the specific game over condition (2 total moves), score calculation, sending `game_over` messages, and handling client disconnections.

3.  **Phase 3: Client Implementation (Stages 16-19)**
    * **Stage 16 (Client)**: Command-line argument parsing for server IP/port, TCP socket creation, and connection to the server.
    * **Stage 17 (Client)**: Sending `register` requests, handling `register_ack`/`nack`, and processing the `game_start` message (displaying initial board, player roles).
    * **Stage 18 (Client)**: Receiving `your_turn`, prompting user for move input (source & target coordinates), and sending `move` messages to the server.
    * **Stage 19 (Client)**: Processing server updates (`move_ok`, `invalid_move`, `pass`), displaying board changes, and handling the `game_over` sequence (displaying scores, exiting).

## ⚙️ How to Compile & Run (Illustrative)

*(Specific commands will depend on the chosen JSON library and final build setup)*

1.  **Prerequisites**:
    * `gcc` compiler (or compatible C compiler).
    * A C JSON library (e.g., `cJSON`) installed and linkable.

2.  **Compilation**:
    ```bash
    # Example for cJSON
    # Compile Server
    gcc server.c cJSON.c -o server -lm # Or your specific JSON lib flags

    # Compile Client
    gcc client.c cJSON.c -o client -lm # Or your specific JSON lib flags
    ```

3.  **Running the Application**:
    * **Start the Server**:
        ```bash
        ./server
        # Server will listen on port 5050 (default, or as configured)
        ```
    * **Connect Clients**: Open two separate terminal windows.
        ```bash
        # Client 1
        ./client <server_ip_address> 5050

        # Client 2
        ./client <server_ip_address> 5050
        ```
        (Replace `<server_ip_address>` with the actual IP of the server, e.g., `127.0.0.1` for local testing).

## 💬 Protocol Overview

Communication is managed via simple JSON payloads over TCP. Key message types include:

* **Client -> Server**:
    * `{"type": "register", "username": "<name>"}`
    * `{"type": "move", "username": "<name>", "sx":X, "sy":Y, "tx":X', "ty":Y'}`
* **Server -> Client**:
    * `{"type": "register_ack"}` / `{"type": "register_nack", "reason": "<...>"}`
    * `{"type": "game_start", "players": ["u1","u2"], "first_player": "u1", "initial_board": [[...]]}`
    * `{"type": "your_turn", "board": [[...]], "timeout": 5.0}`
    * `{"type": "move_ok", "board": [[...]], "next_player": "<name>"}`
    * `{"type": "invalid_move", "board": [[...]], "next_player": "<name>"}`
    * `{"type": "pass", "next_player": "<name>"}`
    * `{"type": "game_over", "scores": {"u1": S1, "u2": S2}}`

---

This README aims to provide a comprehensive yet accessible overview for developers looking to understand, build, or contribute to OctaFlip Online.
