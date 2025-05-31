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
    char reason[MAX_REASON_LEN]; // Reason for invalid move
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

/*
Note on JSON utilities (cJSON.c, cJSON.h, or your own json_utils.c/.h):
The submission allows for these files[cite: 25].
You should implement robust serialization and deserialization functions for these payloads.
Remember to handle the "\n" delimiter for sending and receiving messages[cite: 16, 17, 18, 19, 20, 21].
Example for sending: char *json_str = cJSON_PrintUnformatted(obj); send(fd, json_str, strlen(json_str), 0); send(fd, "\n", 1, 0); free(json_str); [cite: 17, 18]
Example for reading: Implement a buffered read that can parse newline-terminated JSON strings[cite: 19, 20, 21].
*/

#endif // PROTOCOL_H
