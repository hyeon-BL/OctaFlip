# OctaFlip Online: Networked C Game with LED Matrix Display & Automated Client

<img width="80%" src="https://github.com/user-attachments/assets/5af1778f-d556-49bf-8c17-af9e9ef5a5c6"/>


**OctaFlip Online** is a sophisticated C-based implementation of the strategic 8x8 board game, OctaFlip. This project showcases a full-fledged client-server architecture enabling two players to compete in real-time. Key features include an automated move-generation client, TCP/IP networking with a JSON-based protocol, and a vibrant visual display of the game board on a 64x64 RGB LED matrix using the `rpi-rgb-led-matrix` library.

The server application acts as the central arbiter, managing all game logic, validating player moves against the established OctaFlip rules, and synchronizing game state across connected clients. Clients, featuring an automated `move_generate` function, connect to the server, register with a unique username, and autonomously participate in matches, with all board states rendered dynamically on the LED matrix.

## ✨ Key Features

* **Core OctaFlip Gameplay**: Implements all fundamental game mechanics including piece cloning, jumping, and the strategic flipping of opponent pieces.
* **Networked Client-Server Architecture**: Robust two-player gameplay facilitated over TCP/IP, with the server managing game flow.
* **JSON Messaging Protocol**: All client-server communication utilizes structured JSON payloads, delimited by newline characters (\n) for reliable message framing over TCP streams.
* **Automated Client Move Generation**: The client employs a `move_generate` function to autonomously decide and execute moves within a specified timeout (e.g., 3 seconds for Assignment 3 server play).
* **RGB LED Matrix Display**: Dynamic visualization of the 8x8 game board on a 64x64 LED panel, managed by a dedicated `board.c`/`board.h` module utilizing the `rpi-rgb-led-matrix` library.
* **Server-Side Authority**: Centralized validation of all game rules, player turns, and move legality, including a 5-second turn timeout enforced by the server.
* **Specialized Pass Move**: Clients can signal a "pass" turn by sending move coordinates `(0,0,0,0)` to the server.
* **Comprehensive Server Logging**: The server maintains a visible log detailing each board state and the corresponding move executed.
* **Flexible Client Configuration**: Client accepts server IP, port, and username via command-line arguments for easy connectivity.
* **Graceful Disconnection Handling**: The server is designed to manage client disconnections, typically by passing the turn of a disconnected player.

## 💻 Tech Stack
* **Language**: C (conforming to C99/C11 standards)
* **Networking**: POSIX Sockets API (TCP/IP) for client-server communication.
   * **Standard headers**: `sys/socket.h`, `netdb.h`, `arpa/inet.h`.
* **JSON Processing**: `cJSON` library (sources compiled directly with the project).
* **LED Matrix Display**: `rpi-rgb-led-matrix` library (linked statically).
* **Server-Side Concurrency**: I/O multiplexing using `select()`.
* **Build System**: GNU Make, with a flexible `Makefile` supporting different build targets.
* **Core Libraries**: `librt`, `libm`, `libpthread`, `libstdc++` (as per Makefile linking).

## 📂 Project Structure

The project is organized into modular components for clarity and maintainability:


octaflip-project/ <br>
├── server.c                # Main server application logic, game orchestration <br>
├── client.c                # Main client application logic, automated move generation <br>
├── board.c                 # LED matrix rendering implementation <br>
├── board.h                 # Public interface for the LED matrix display module <br>
├── protocol.h              # Shared data structures for JSON message payloads <br>
├── cJSON.c                 # cJSON library source file <br>
├── cJSON.h                 # cJSON library header file <br>
├── rpi-rgb-led-matrix/     # Directory containing the rpi-rgb-led-matrix library source <br>
│   ├── include/            # Headers for rpi-rgb-led-matrix <br>
│   └── lib/                # Contains Makefile to build librgbmatrix.a <br>
│       └── librgbmatrix.a  # Static library (built by nested make) <br>
├── Makefile                # Main Makefile for building the project <br>
└── README.md               # This documentation file <br>


## 🛠️ Modules Overview
* **`server.c`**: The heart of the game. Manages client connections, enforces game rules, processes moves, handles turns and timeouts, and broadcasts game state updates.
* **`client.c`**: Connects to the server, handles registration, implements the `move_generate` function for autonomous play, and interfaces with the board.c module to display the game on the LED matrix.
* **`board.c` / `board.h`**: Encapsulates all interactions with the `rpi-rgb-led-matrix` library. Provides functions to initialize the matrix, render the OctaFlip board state, and clean up resources. Includes a standalone test mode.
* **`protocol.h`**: Defines C structures corresponding to the JSON message payloads exchanged between client and server, ensuring type safety and consistency.
* **`cJSON.c` / `cJSON.h`**: Provides robust JSON parsing and generation capabilities.

## 💬 Communication Protocol
All client-server communication adheres to a defined JSON-based protocol over TCP. Messages are newline-terminated (`\n`). Key message types include:

* **Client → Server**:
   * `register`: `{"type": "register", "username": "<name>"}`
   * `move`: `{"type": "move", "username": "<name>", "sx":X, "sy":Y, "tx":X', "ty":Y'}` (coordinates (0,0,0,0) indicate a pass)

* **Server → Client**:
   * `register_ack`: `{"type": "register_ack"}`
   * `register_nack`: `{"type": "register_nack", "reason": "invalid"}`
   * `game_start`: `{"type": "game_start", "players": ["u1","u2"], "first_player": "u1"}` (Board state sent with first `your_turn`)
   * `your_turn`: `{"type": "your_turn", "board": [[...8x8 board strings...]], "timeout": 5.0}`
   * `move_ok`: `{"type": "move_ok", "board": [[...]], "next_player": "<name>"}`
   * `invalid_move`: `{"type": "invalid_move", "board": [[...]], "next_player": "<name>", "reason": "<optional_reason>"}`
   * `pass`: `{"type": "pass", "next_player": "<name>"}`
   * `game_over`: `{"type": "game_over", "scores": {"<user1_name>": S1, "<user2_name>": S2}}`

## 🚀 Compilation and Execution
This project uses a `Makefile` for streamlined building. Ensure you have `gcc`, `make`, and the `rpi-rgb-led-matrix` library source code available.

1. Prerequisites
   * A C compiler (e.g., `gcc`).
   * GNU `make`.
   * The `rpi-rgb-led-matrix` library source code, expected to be located in a subdirectory named `rpi-rgb-led-matrix/` relative to the `Makefile`.

2. Building the `rpi-rgb-led-matrix` Library
The main `Makefile` attempts to build this library if its static archive `librgbmatrix.a` is not found. This typically involves running `make` within the library's own `lib` directory:
```bash
# This step is usually handled automatically by the main Makefile's dependency rule.
# If manual build is needed:
cd rpi-rgb-led-matrix/
make -C lib
cd ..
```
*Note: Ensure the `rpi-rgb-led-matrix` library is configured and built correctly for your specific Raspberry Pi hardware and panel setup.*

3. Building OctaFlip Executables
The `Makefile` supports different build types via the `BUILD_TYPE` variable:

   * To build the networked OctaFlip client (default):
   ```bash
   make
   ```

   or explicitly:
   ```bash
   make BUILD_TYPE=client
   ```
   This will generate the `client` executable, linking `client.c`, `board.c`, and `cJSON.c`.

   * To build the standalone LED board test program:
   ```bash
   make BUILD_TYPE=standalone_test
   ```
   This will generate the standalone_board_test executable from board.c with the STANDALONE_BOARD_TEST macro defined.

   * To build the server (manual command):
   *(The provided Makefile focuses on client-side builds. A server build can be done as follows):*
   ```bash
   gcc server.c cJSON.c -o server -lrt -lm -lpthread -lstdc++
   ```

5. Running the Application
   * Start the OctaFlip Server:
   ```bash
   ./server
   ```
   The server will listen on a configured port (e.g., 5000 for local testing).

   * Run the OctaFlip Client:
   *(Requires `sudo` for direct hardware access by the rpi-rgb-led-matrix library)*
   ```bash
   # Example for a local server on port 5000:
   sudo ./client -ip 127.0.0.1 -port 5000 -username YOUR_CHOSEN_USERNAME
   ```
   * Run the Standalone LED Board Test:
   *(Requires `sudo`)*
   ```bash
   sudo ./standalone_board_test
   ```
   After running, paste an 8x8 board configuration into the terminal, followed by EOF (Ctrl+D). The board will be displayed on the LED matrix.

6. Cleaning Build Artifacts
```bash
make clean
```
This will remove the `client` and `standalone_board_test` executables.

## 💡 LED Matrix Display (`board.c` / `board.h`)
The `board.c` module is responsible for all direct interactions with the 64x64 RGB LED matrix.
* **Initialization**: `client.c` calls `initialize_matrix(&argc, &argv)` at startup. This function also handles command-line options specific to the `rpi-rgb-led-matrix` library.
* **Rendering**: Upon receiving board updates from the server, `client.c` calls `render_octaflip_board(matrix_ptr, board_state)` to draw the current game state.
* **Standalone Test**: The `standalone_board_test` executable (built via `make BUILD_TYPE=standalone_test`) directly uses `board.c` to read an 8x8 board from standard input and display it, facilitating easier testing and grading of the LED display component.
* **Customization**: Piece representation (colors, patterns) and grid lines can be customized within `board.c`.

## 🧠 Automated Move Generation (`client.c`)
The client application (`client.c`) features an automated move selection mechanism:
* A `move_generate(char current_board[8][9], char my_player_symbol)` function is implemented.
* When the server indicates it's the client's turn (via a `your_turn` message), this function is invoked with the current board state.
* It must analyze the board and return a valid move (source `sx`, `sy` and target `tx`, `ty` coordinates, or `(0,0,0,0)` for a pass) within the server-specified timeout.
* The sophistication of the move generation algorithm (heuristics, game theory, AI) is up to the implementer.
