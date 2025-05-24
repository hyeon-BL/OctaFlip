#ifndef PROTOCOL_H
#define PROTOCOL_H

// Maximum string lengths (adjust as needed)
#define MAX_USERNAME_LEN 32
#define MAX_REASON_LEN 128
#define MAX_BOARD_STR_LEN (8 * 9) // 8 rows * (8 chars + 1 newline/null)

// 1. Define C structs for each JSON message type.
//    These structs will be used by both client and server for parsing and constructing messages.
//    Reference "Assignment_2_05.21(1).pdf" for payload fields. [cite: 4, 5, 6, 7, 8]

// Example for a generic message type identifier (adapt as needed)
typedef struct
{
    char type[32]; // "register", "move", "game_start", etc.
} BaseMessage;

// Client to Server Payloads
typedef struct
{
    char type[32]; // "register"
    char username[MAX_USERNAME_LEN];
} ClientRegisterPayload;

typedef struct
{
    char type[32]; // "move"
    char username[MAX_USERNAME_LEN];
    int sx;
    int sy;
    int tx;
    int ty;
} ClientMovePayload;

// Server to Client Payloads
typedef struct
{
    char type[32]; // "register_ack"
} ServerRegisterAckPayload;

typedef struct
{
    char type[32]; // "register_nack"
    char reason[MAX_REASON_LEN];
} ServerRegisterNackPayload;

typedef struct
{
    char type[32]; // "game_start"
    char players[2][MAX_USERNAME_LEN];
    char first_player[MAX_USERNAME_LEN];
    char initial_board[8][9]; // Array of 8 strings, each for a row
} ServerGameStartPayload;

typedef struct
{
    char type[32];    // "your_turn"
    char board[8][9]; // Array of 8 strings
    double timeout;
} ServerYourTurnPayload;

typedef struct
{
    char type[32];    // "move_ok"
    char board[8][9]; // Array of 8 strings
    char next_player[MAX_USERNAME_LEN];
} ServerMoveOkPayload;

typedef struct
{
    char type[32];    // "invalid_move"
    char board[8][9]; // Array of 8 strings
    char next_player[MAX_USERNAME_LEN];
} ServerInvalidMovePayload;

typedef struct
{
    char type[32]; // "pass"
    char next_player[MAX_USERNAME_LEN];
} ServerPassPayload;

typedef struct
{
    char username[MAX_USERNAME_LEN];
    int score;
} PlayerScore;

typedef struct
{
    char type[32]; // "game_over"
    PlayerScore scores[2];
} ServerGameOverPayload;

// 2. Function Prototypes for JSON Utilities (to be implemented in server.c/client.c as needed)
//    These are examples; you might need more or different signatures.
//    Consider using a JSON library like cJSON. [cite: 3]

//    Helper to get message type
//    const char* get_message_type_from_json(const char* json_string);

//    Serialization examples (implement in the file that sends the message)
//    char* serialize_client_register(const ClientRegisterPayload* payload);
//    char* serialize_client_move(const ClientMovePayload* payload);
//    char* serialize_server_game_start(const ServerGameStartPayload* payload);
//    ... and so on for all messages that need to be sent.

//    Deserialization examples (implement in the file that receives the message)
//    int deserialize_client_register(const char* json_string, ClientRegisterPayload* out_payload);
//    int deserialize_server_game_start(const char* json_string, ServerGameStartPayload* out_payload);
//    ... and so on for all messages that need to be received.
//    Return 0 for success, -1 for failure.

#endif // PROTOCOL_H
