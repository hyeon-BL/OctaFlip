#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h> // For struct addrinfo and getaddrinfo
#include <unistd.h>
#include <arpa/inet.h>  // For inet_ntop
#include <sys/select.h> // For select()
#include <time.h>       // For time_t in PlayerState
#include "protocol.h"   // Include the protocol header
#include <errno.h>      // For errno
#include "cJSON.h"      // Assuming cJSON library is used for JSON parsing/generation.
// You would need to link against cJSON, e.g., gcc server.c cJSON.c -o server -lm

// Server configuration
#define SERVER_PORT "5050"
#define MAX_CLIENTS 2
#define BUFFER_SIZE 2048
#define LISTEN_BACKLOG 10
#define TURN_TIMEOUT_SECONDS 5
#define PLAYER_RECV_BUFFER_MAX_LEN (BUFFER_SIZE * 2) // Max size for a player's receive buffer

// Player state enumeration
typedef enum
{
    P_EMPTY,
    P_CONNECTED,
    P_REGISTERED,
    P_PLAYING,
    P_DISCONNECTED
} ClientConnectionState;

// Player Information Structure
typedef struct
{
    int socket_fd;
    char username[MAX_USERNAME_LEN];
    ClientConnectionState state;
    struct sockaddr_storage address;
    socklen_t addr_len;
    char player_role; // 'R' or 'B'
    time_t last_message_time;
    char recv_buffer[PLAYER_RECV_BUFFER_MAX_LEN]; // Buffer for incoming messages
    int recv_buffer_len;                          // Current length of data in recv_buffer
} PlayerState;

// Global variables
PlayerState players[MAX_CLIENTS];
int num_clients = 0;
int num_registered_players = 0;
char octaflip_board[8][9];          // Game board
int current_turn_player_index = -1; // Index in the 'players' array
time_t turn_start_time;             // To track the 5s timeout
int total_moves_made_in_game = 0;   // For game over condition

fd_set master_fds; // Master file descriptor list
fd_set read_fds;   // Temp file descriptor list for select()
int listener_fd;   // Listening socket descriptor
int fd_max;        // Maximum file descriptor number

// Forward declarations
void initialize_player_states(PlayerState all_players[]);
int initialize_server_socket(const char *port);
void add_player(int client_socket, struct sockaddr_storage *client_addr, socklen_t addr_len, PlayerState all_players[], int *current_num_clients);
void remove_player(PlayerState *player_to_remove, PlayerState all_players[], int *current_num_clients, int *current_num_registered_players);
void accept_new_connection(int current_listener_fd, PlayerState all_players[], int *current_num_clients);
void handle_client_message(PlayerState *player, PlayerState all_players[], int *current_num_clients, int *current_num_registered_players, char game_board[8][9]);
void process_move_request(PlayerState *player, const char *received_json_string, PlayerState all_players[], char game_board[8][9]);
void handle_turn_timeout(PlayerState all_players[], char game_board[8][9]);
void start_player_turn(PlayerState all_players[], int player_idx, char game_board[8][9]);
void switch_to_next_turn(PlayerState all_players[], char game_board[8][9]);
int check_and_process_game_over(PlayerState all_players[], char game_board[8][9], int total_moves_completed);
void handle_client_disconnection(PlayerState *disconnected_player, PlayerState all_players[], char game_board[8][9]);
int count_player_pieces_on_board(char board[8][9], char player_symbol);
void attempt_game_start(PlayerState players[], int current_registered_count, char game_board[8][9]);
void log_board_and_move(char current_board[8][9], const char *player_username, int sx, int sy, int tx, int ty, const char *move_type_or_status); // New

// --- JSON Utility Stubs ---
const char *get_message_type_from_json(const char *json_string)
{
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error parsing JSON before: %s\n", error_ptr);
        }
        return NULL;
    }

    cJSON *type_json = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type_json) || (type_json->valuestring == NULL))
    {
        fprintf(stderr, "Error: JSON message does not have a valid 'type' field.\n");
        cJSON_Delete(root);
        return NULL;
    }

    char *type_str = strdup(type_json->valuestring); // Caller must free this
    cJSON_Delete(root);
    return type_str;
}

// Deserialize ClientMovePayload
int deserialize_client_move(const char *json_string, ClientMovePayload *out_payload)
{
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL)
    {
        fprintf(stderr, "Error: Failed to parse ClientMovePayload JSON.\n");
        return -1;
    }

    cJSON *type_json = cJSON_GetObjectItemCaseSensitive(root, "type");
    cJSON *username_json = cJSON_GetObjectItemCaseSensitive(root, "username");
    cJSON *sx_json = cJSON_GetObjectItemCaseSensitive(root, "sx");
    cJSON *sy_json = cJSON_GetObjectItemCaseSensitive(root, "sy");
    cJSON *tx_json = cJSON_GetObjectItemCaseSensitive(root, "tx");
    cJSON *ty_json = cJSON_GetObjectItemCaseSensitive(root, "ty");

    if (!cJSON_IsString(type_json) || (type_json->valuestring == NULL) || strcmp(type_json->valuestring, "move") != 0 ||
        !cJSON_IsString(username_json) || (username_json->valuestring == NULL) ||
        !cJSON_IsNumber(sx_json) || !cJSON_IsNumber(sy_json) ||
        !cJSON_IsNumber(tx_json) || !cJSON_IsNumber(ty_json))
    {
        fprintf(stderr, "Error: Malformed ClientMovePayload JSON or incorrect type.\n");
        cJSON_Delete(root);
        return -1;
    }

    strcpy(out_payload->type, type_json->valuestring);
    strncpy(out_payload->username, username_json->valuestring, MAX_USERNAME_LEN - 1);
    out_payload->username[MAX_USERNAME_LEN - 1] = '\0';
    out_payload->sx = sx_json->valueint;
    out_payload->sy = sy_json->valueint;
    out_payload->tx = tx_json->valueint;
    out_payload->ty = ty_json->valueint;

    cJSON_Delete(root);
    return 0; // Success
}

// Serialize ServerYourTurnPayload
char *serialize_server_your_turn(const ServerYourTurnPayload *payload)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return NULL;

    if (cJSON_AddStringToObject(root, "type", payload->type) == NULL)
        goto error;

    cJSON *board_array = cJSON_CreateArray();
    if (board_array == NULL)
        goto error;
    for (int i = 0; i < 8; ++i)
    {
        cJSON *row_string = cJSON_CreateString(payload->board[i]);
        if (row_string == NULL)
        {
            cJSON_Delete(board_array);
            goto error;
        }
        cJSON_AddItemToArray(board_array, row_string);
    }
    cJSON_AddItemToObject(root, "board", board_array);

    if (cJSON_AddNumberToObject(root, "timeout", payload->timeout) == NULL)
        goto error; // board_array will be deleted with root

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;

error:
    cJSON_Delete(root);
    return NULL;
}

// Serialize ServerMoveOkPayload
char *serialize_server_move_ok(const ServerMoveOkPayload *payload)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return NULL;

    if (cJSON_AddStringToObject(root, "type", payload->type) == NULL)
        goto error;

    cJSON *board_array = cJSON_CreateArray();
    if (board_array == NULL)
        goto error;
    for (int i = 0; i < 8; ++i)
    {
        cJSON *row_string = cJSON_CreateString(payload->board[i]);
        if (row_string == NULL)
        {
            cJSON_Delete(board_array);
            goto error;
        }
        cJSON_AddItemToArray(board_array, row_string);
    }
    cJSON_AddItemToObject(root, "board", board_array);

    if (cJSON_AddStringToObject(root, "next_player", payload->next_player) == NULL)
        goto error; // board_array will be deleted with root

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;

error:
    cJSON_Delete(root);
    return NULL;
}

// Serialize ServerInvalidMovePayload
char *serialize_server_invalid_move(const ServerInvalidMovePayload *payload)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return NULL;

    if (cJSON_AddStringToObject(root, "type", payload->type) == NULL)
        goto error;

    cJSON *board_array = cJSON_CreateArray();
    if (board_array == NULL)
        goto error;
    for (int i = 0; i < 8; ++i)
    {
        cJSON *row_string = cJSON_CreateString(payload->board[i]);
        if (row_string == NULL)
        {
            cJSON_Delete(board_array);
            goto error;
        }
        cJSON_AddItemToArray(board_array, row_string);
    }
    cJSON_AddItemToObject(root, "board", board_array);

    if (cJSON_AddStringToObject(root, "next_player", payload->next_player) == NULL)
        goto error; // board_array will be deleted with root

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;

error:
    cJSON_Delete(root);
    return NULL;
}

// Serialize ServerPassPayload
char *serialize_server_pass(const ServerPassPayload *payload)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return NULL;

    if (cJSON_AddStringToObject(root, "type", payload->type) == NULL)
        goto error;
    if (cJSON_AddStringToObject(root, "next_player", payload->next_player) == NULL)
        goto error;

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;

error:
    cJSON_Delete(root);
    return NULL;
}

// Serialize ServerGameOverPayload
char *serialize_server_game_over(const ServerGameOverPayload *payload)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return NULL;

    if (cJSON_AddStringToObject(root, "type", payload->type) == NULL)
        goto error;

    cJSON *scores_array = cJSON_CreateArray();
    if (scores_array == NULL)
        goto error;

    for (int i = 0; i < 2; ++i)
    { // Assuming MAX_CLIENTS is 2 for scores
        cJSON *score_obj = cJSON_CreateObject();
        if (score_obj == NULL)
        {
            cJSON_Delete(scores_array);
            goto error;
        }
        if (cJSON_AddStringToObject(score_obj, "username", payload->scores[i].username) == NULL)
        {
            cJSON_Delete(score_obj);
            cJSON_Delete(scores_array);
            goto error;
        }
        if (cJSON_AddNumberToObject(score_obj, "score", payload->scores[i].score) == NULL)
        {
            cJSON_Delete(score_obj);
            cJSON_Delete(scores_array);
            goto error;
        }
        cJSON_AddItemToArray(scores_array, score_obj);
    }
    cJSON_AddItemToObject(root, "scores", scores_array);

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;

error:
    cJSON_Delete(root);
    return NULL;
}

int deserialize_client_register(const char *json_string, ClientRegisterPayload *out_payload)
{
    cJSON *root = cJSON_Parse(json_string);
    if (!root)
    {
        fprintf(stderr, "Error: Failed to parse ClientRegisterPayload JSON.\n");
        return -1;
    }
    cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    cJSON *username = cJSON_GetObjectItemCaseSensitive(root, "username");

    if (!cJSON_IsString(type) || !type->valuestring || strcmp(type->valuestring, "register") != 0 ||
        !cJSON_IsString(username) || !username->valuestring)
    {
        fprintf(stderr, "Error: Malformed ClientRegisterPayload JSON or incorrect type.\n");
        cJSON_Delete(root);
        return -1;
    }
    strcpy(out_payload->type, type->valuestring);
    strncpy(out_payload->username, username->valuestring, MAX_USERNAME_LEN - 1);
    out_payload->username[MAX_USERNAME_LEN - 1] = '\0';
    cJSON_Delete(root);
    return 0;
}

// Serialize ServerRegisterAckPayload
char *serialize_server_register_ack(const ServerRegisterAckPayload *payload)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;
    if (!cJSON_AddStringToObject(root, "type", payload->type))
    { // type should be "register_ack"
        cJSON_Delete(root);
        return NULL;
    }
    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

// Serialize ServerRegisterNackPayload
char *serialize_server_register_nack(const ServerRegisterNackPayload *payload)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;
    if (!cJSON_AddStringToObject(root, "type", payload->type) || // type should be "register_nack"
        !cJSON_AddStringToObject(root, "reason", payload->reason))
    {
        cJSON_Delete(root);
        return NULL;
    }
    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

// Serialize ServerGameStartPayload
char *serialize_server_game_start(const ServerGameStartPayload *payload)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    if (!cJSON_AddStringToObject(root, "type", payload->type))
        goto error;

    cJSON *players_array = cJSON_CreateArray();
    if (!players_array)
        goto error;
    for (int i = 0; i < 2; ++i)
    { // Assuming MAX_CLIENTS is 2
        cJSON *player_name = cJSON_CreateString(payload->players[i]);
        if (!player_name)
        {
            cJSON_Delete(players_array);
            goto error;
        }
        cJSON_AddItemToArray(players_array, player_name);
    }
    cJSON_AddItemToObject(root, "players", players_array);

    if (!cJSON_AddStringToObject(root, "first_player", payload->first_player))
        goto error;

    // The initial_board field is no longer sent in the game_start message as per [cite: 8]
    // cJSON *board_array = cJSON_CreateArray();
    // if (!board_array)
    //     goto error;
    // for (int i = 0; i < 8; ++i)
    // {
    //     cJSON *row_string = cJSON_CreateString(payload->initial_board[i]);
    //     if (!row_string)
    //     {
    //         cJSON_Delete(board_array);
    //         goto error;
    //     }
    //     cJSON_AddItemToArray(board_array, row_string);
    // }
    // cJSON_AddItemToObject(root, "initial_board", board_array);

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;

error:
    cJSON_Delete(root);
    return NULL;
}

// --- End JSON Utility Stubs ---

// --- Logging Function ---
void log_board_and_move(char current_board[8][9], const char *player_username, int sx, int sy, int tx, int ty, const char *move_type_or_status)
{
    printf("Server Log:\n");
    printf("  Player: %s\n", player_username ? player_username : "N/A");
    if (sx == -1 && sy == -1 && tx == -1 && ty == -1)
    { // Convention for timeout or non-move events
        printf("  Action: %s\n", move_type_or_status);
    }
    else if (sx == 0 && sy == 0 && tx == 0 && ty == 0 && (strcmp(move_type_or_status, "Attempted Pass") == 0 || strcmp(move_type_or_status, "Valid Pass") == 0))
    {
        printf("  Move: Pass\n");
        printf("  Status: %s\n", move_type_or_status);
    }
    else
    {
        printf("  Move: (%d,%d) -> (%d,%d)\n", sx, sy, tx, ty);
        printf("  Status: %s\n", move_type_or_status);
    }
    printf("  Board State:\n");
    for (int i = 0; i < 8; ++i)
    {
        printf("    %s\n", current_board[i]);
    }
    printf("----------------------------------------\n");
}

// --- OctaFlip Game Logic Interface (Placeholder) ---
// This function would wrap or call your OctaFlip game logic.
// It should return 1 if the move is valid and processed, 0 otherwise.
// It updates game_board directly if the move is valid.

void attempt_game_start(PlayerState all_players[], int current_num_registered_players_val, char game_board[8][9])
{
    if (current_num_registered_players_val == MAX_CLIENTS && current_turn_player_index == -1)
    {
        printf("Server: Two players registered. Attempting to start game.\n");

        for (int r = 0; r < 8; r++)
        {
            for (int c = 0; c < 8; c++)
                game_board[r][c] = '.';
            game_board[r][8] = '\0';
        }
        // initial game board setup
        game_board[0][6] = 'R';
        game_board[7][0] = 'B';
        game_board[0][7] = 'B';
        game_board[7][7] = 'R';

        int players_assigned_role = 0;
        int first_player_idx = -1;

        for (int k = 0; k < MAX_CLIENTS; ++k)
        {
            if (all_players[k].state == P_REGISTERED)
            {
                all_players[k].state = P_PLAYING;
                all_players[k].player_role = (players_assigned_role == 0) ? 'R' : 'B'; // first player 'R', second player 'B'
                if (players_assigned_role == 0)
                {
                    first_player_idx = k;
                }
                printf("Server: Player %s is ready to play as %c.\n", all_players[k].username, all_players[k].player_role);
                players_assigned_role++;
            }
        }

        if (players_assigned_role == MAX_CLIENTS && first_player_idx != -1)
        {
            total_moves_made_in_game = 0;

            ServerGameStartPayload gs_payload;
            strcpy(gs_payload.type, "game_start");
            // Board is initialized internally on the server [cite: 8]
            // The client does not receive initial_board in game_start message.
            // However, we might still populate it locally if the struct in protocol.h has it.
            // This line can be removed if ServerGameStartPayload in protocol.h no longer has initial_board.
            // memcpy(gs_payload.initial_board, game_board, sizeof(gs_payload.initial_board));

            int player_info_count = 0;
            for (int k = 0; k < MAX_CLIENTS; ++k)
            {
                if (all_players[k].state == P_PLAYING)
                {
                    strncpy(gs_payload.players[player_info_count], all_players[k].username, MAX_USERNAME_LEN - 1);
                    gs_payload.players[player_info_count][MAX_USERNAME_LEN - 1] = '\0';
                    if (all_players[k].player_role == 'R')
                    { // first player
                        strncpy(gs_payload.first_player, all_players[k].username, MAX_USERNAME_LEN - 1);
                        gs_payload.first_player[MAX_USERNAME_LEN - 1] = '\0';
                    }
                    player_info_count++;
                    if (player_info_count == MAX_CLIENTS)
                        break;
                }
            }
            if (strlen(gs_payload.first_player) == 0 && all_players[first_player_idx].state == P_PLAYING)
            {
                strncpy(gs_payload.first_player, all_players[first_player_idx].username, MAX_USERNAME_LEN - 1);
                gs_payload.first_player[MAX_USERNAME_LEN - 1] = '\0';
            }

            char *json_gs_message = serialize_server_game_start(&gs_payload);
            if (json_gs_message)
            {
                for (int k = 0; k < MAX_CLIENTS; ++k)
                {
                    if (all_players[k].state == P_PLAYING)
                    {
                        if (send(all_players[k].socket_fd, json_gs_message, strlen(json_gs_message), 0) == -1 ||
                            send(all_players[k].socket_fd, "\n", 1, 0) == -1) // Added newline
                        {
                            perror("send game_start or newline");
                            handle_client_disconnection(&all_players[k], all_players, game_board);
                        }
                        else
                        {
                            printf("Server: Sent 'game_start' to %s.\n", all_players[k].username);
                        }
                    }
                }
                free(json_gs_message);
            }
            else
            {
                fprintf(stderr, "Error serializing ServerGameStartPayload.\n");
            }

            start_player_turn(all_players, first_player_idx, game_board);
        }
        else
        {
            fprintf(stderr, "Error: Could not assign roles or find first player to start the game. Registered: %d\n", players_assigned_role);
            for (int k = 0; k < MAX_CLIENTS; ++k)
            {
                if (all_players[k].state == P_PLAYING)
                {
                    all_players[k].state = P_REGISTERED;
                    all_players[k].player_role = ' ';
                }
            }
        }
    }
}

void process_registration_request(PlayerState *player, const char *received_json_string, PlayerState all_players[], int *current_num_registered_players, char game_board[8][9])
{
    ClientRegisterPayload reg_payload;
    if (deserialize_client_register(received_json_string, &reg_payload) != 0)
    {
        fprintf(stderr, "Server: Failed to deserialize register request from socket %d.\n", player->socket_fd);
        return;
    }

    if (player->state != P_CONNECTED)
    {
        fprintf(stderr, "Server: Player (socket %d) attempted to register but not in P_CONNECTED state (current state: %d).\n", player->socket_fd, player->state);
        ServerRegisterNackPayload nack;
        strcpy(nack.type, "register_nack");
        strcpy(nack.reason, "Invalid state for registration.");
        char *nack_json = serialize_server_register_nack(&nack);
        if (nack_json)
        {
            if (send(player->socket_fd, nack_json, strlen(nack_json), 0) == -1 ||
                send(player->socket_fd, "\n", 1, 0) == -1) // Added newline
            {
                perror("send register_nack (invalid state) or newline");
                // Consider if disconnection is appropriate here or just log
            }
            free(nack_json);
        }
        return;
    }

    if (strlen(reg_payload.username) == 0)
    {
        fprintf(stderr, "Server: Empty username in registration from socket %d.\n", player->socket_fd);
        ServerRegisterNackPayload nack;
        strcpy(nack.type, "register_nack");
        strcpy(nack.reason, "Username cannot be empty.");
        char *nack_json = serialize_server_register_nack(&nack);
        if (nack_json)
        {
            if (send(player->socket_fd, nack_json, strlen(nack_json), 0) == -1 ||
                send(player->socket_fd, "\n", 1, 0) == -1) // Added newline
            {
                perror("send register_nack (empty username) or newline");
            }
            free(nack_json);
        }
        return;
    }

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (all_players[i].socket_fd != -1 && (&all_players[i] != player) &&
            (all_players[i].state == P_REGISTERED || all_players[i].state == P_PLAYING) &&
            strcmp(all_players[i].username, reg_payload.username) == 0)
        {
            fprintf(stderr, "Server: Username '%s' already taken. Registration failed for socket %d.\n", reg_payload.username, player->socket_fd);
            ServerRegisterNackPayload nack;
            strcpy(nack.type, "register_nack");
            strcpy(nack.reason, "invalid");
            char *nack_json = serialize_server_register_nack(&nack);
            if (nack_json)
            {
                if (send(player->socket_fd, nack_json, strlen(nack_json), 0) == -1 ||
                    send(player->socket_fd, "\n", 1, 0) == -1) // Added newline
                {
                    perror("send register_nack (username taken) or newline");
                }
                free(nack_json);
            }
            return;
        }
    }

    if (*current_num_registered_players >= MAX_CLIENTS && player->state != P_REGISTERED)
    {
        fprintf(stderr, "Server: Maximum registered players reached. Cannot register '%s'.\n", reg_payload.username);
        ServerRegisterNackPayload nack;
        strcpy(nack.type, "register_nack");
        strcpy(nack.reason, "invalid");
        char *nack_json = serialize_server_register_nack(&nack);
        if (nack_json)
        {
            if (send(player->socket_fd, nack_json, strlen(nack_json), 0) == -1 ||
                send(player->socket_fd, "\n", 1, 0) == -1) // Added newline
            {
                perror("send register_nack (server full) or newline");
            }
            free(nack_json);
        }
        return;
    }

    strncpy(player->username, reg_payload.username, MAX_USERNAME_LEN - 1);
    player->username[MAX_USERNAME_LEN - 1] = '\0';
    player->state = P_REGISTERED;
    (*current_num_registered_players)++;

    printf("Server: Player %s (socket %d) registered successfully. Total registered: %d\n", player->username, player->socket_fd, *current_num_registered_players);

    ServerRegisterAckPayload ack;
    strcpy(ack.type, "register_ack");
    char *ack_json = serialize_server_register_ack(&ack);
    if (ack_json)
    {
        if (send(player->socket_fd, ack_json, strlen(ack_json), 0) == -1 ||
            send(player->socket_fd, "\n", 1, 0) == -1) // Added newline
        {
            perror("send register_ack or newline");
            handle_client_disconnection(player, all_players, game_board);
        }
        free(ack_json);
    }
    else
    {
        fprintf(stderr, "Error serializing ServerRegisterAckPayload for %s\n", player->username);
    }

    if (player->state == P_REGISTERED)
    {
        attempt_game_start(all_players, *current_num_registered_players, game_board); // Renamed function call
    }
}

int validate_and_process_move(char game_board[8][9], int r1, int c1, int r2, int c2, char player_role)
{
    // This is where you'd call your existing OctaFlip logic, e.g., from octaflip.c
    // For example, if you have a function like:
    // int processMove(char board[8][9], int r1_0based, int c1_0based, int r2_0based, int c2_0based, char currentPlayer);
    // return processMove(game_board, r1, c1, r2, c2, player_role);

    // Placeholder logic:
    printf("Server: Validating move for %c from (%d,%d) to (%d,%d)\n", player_role, r1, c1, r2, c2);
    if (r1 < 0 || r1 >= 8 || c1 < 0 || c1 >= 8 || r2 < 0 || r2 >= 8 || c2 < 0 || c2 >= 8)
    {
        printf("Server: Move out of bounds.\n");
        return 0; // Invalid: out of bounds
    }
    if (game_board[r1][c1] != player_role)
    {
        printf("Server: Source cell does not contain player's piece.\n");
        return 0; // Invalid: not player's piece at source
    }
    if (game_board[r2][c2] != '.')
    { // Assuming '.' is empty
        printf("Server: Destination cell not empty.\n");
        return 0; // Invalid: destination not empty
    }
    // Simulate a valid move (e.g., a simple jump or clone)
    // This is highly simplified and needs your actual game rules.
    int dr = abs(r1 - r2);
    int dc = abs(c1 - c2);

    if ((dr <= 2 && dc <= 2) && (dr > 0 || dc > 0))
    { // Basic check for move distance
        if (dr == 2 || dc == 2)
        { // Jump
            game_board[r1][c1] = '.';
        }
        game_board[r2][c2] = player_role; // Clone or jump

        // Flip opponent pieces in the surrounding cells
        char opponent_role = (player_role == 'R') ? 'B' : 'R';
        for (int row_offset = -1; row_offset <= 1; ++row_offset)
        {
            for (int col_offset = -1; col_offset <= 1; ++col_offset)
            {
                if (row_offset == 0 && col_offset == 0)
                {
                    continue; // Skip the piece itself
                }

                int adjacent_r = r2 + row_offset;
                int adjacent_c = c2 + col_offset;

                // Check bounds for 8x8 board
                if (adjacent_r >= 0 && adjacent_r < 8 && adjacent_c >= 0 && adjacent_c < 8)
                {
                    if (game_board[adjacent_r][adjacent_c] == opponent_role)
                    {
                        game_board[adjacent_r][adjacent_c] = player_role;
                        printf("Server: Flipped opponent piece at (%d,%d) to %c\n", adjacent_r, adjacent_c, player_role);
                    }
                }
            }
        }
        printf("Server: Move validated and processed (placeholder).\n");
        return 1; // Valid
    }
    printf("Server: Move failed validation (placeholder).\n");
    return 0; // Invalid
}
// --- End OctaFlip Game Logic Interface ---

// Helper function to count pieces for a player
int count_player_pieces_on_board(char board[8][9], char player_symbol)
{
    int count = 0;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        { // Iterate 8x8 game board part
            if (board[i][j] == player_symbol)
            {
                count++;
            }
        }
    }
    return count;
}

// Function to notify the current player of their turn
// Already handles timeout and sends the game board state
void start_player_turn(PlayerState all_players[], int player_idx, char game_board[8][9])
{
    if (player_idx < 0 || player_idx >= MAX_CLIENTS || all_players[player_idx].state != P_PLAYING)
    {
        fprintf(stderr, "Error: Cannot start turn for invalid player index %d or player not playing.\n", player_idx);
        // Potentially try to find next valid player or end game
        return;
    }
    current_turn_player_index = player_idx;
    turn_start_time = time(NULL); // Record start of turn for timeout

    ServerYourTurnPayload payload;
    strcpy(payload.type, "your_turn");
    memcpy(payload.board, game_board, sizeof(payload.board)); // Copy current board state
    payload.timeout = (double)TURN_TIMEOUT_SECONDS;

    char *json_message = serialize_server_your_turn(&payload);
    if (json_message)
    {
        if (send(all_players[player_idx].socket_fd, json_message, strlen(json_message), 0) == -1 ||
            send(all_players[player_idx].socket_fd, "\n", 1, 0) == -1) // Added newline
        {
            perror("send your_turn or newline");
            handle_client_disconnection(&all_players[player_idx], all_players, game_board);
        }
        printf("Server: Sent 'your_turn' to %s (socket %d).\n", all_players[player_idx].username, all_players[player_idx].socket_fd);
        free(json_message); // Free the serialized string
    }
    else
    {
        fprintf(stderr, "Error serializing ServerYourTurnPayload\n");
    }
    // printf("Server: Notifying player %s (socket %d) it's their turn.\n", all_players[player_idx].username, all_players[player_idx].socket_fd);
}

// Function to check and process game over condition
int check_and_process_game_over(PlayerState all_players[], char game_board[8][9], int total_moves_completed)
{
    if (total_moves_completed == 2) // Game over condition: 2 moves made (placeholder)
    {
        printf("Server: Game over! Total moves reached: %d.\n", total_moves_completed);

        ServerGameOverPayload gop;
        strcpy(gop.type, "game_over");

        // Calculate scores for players[0] and players[1]
        // Ensure players[0] and players[1] are the actual game participants
        if (all_players[0].state == P_PLAYING || all_players[0].state == P_DISCONNECTED)
        { // Check if player 0 was part of the game
            strcpy(gop.scores[0].username, all_players[0].username);
            gop.scores[0].score = count_player_pieces_on_board(game_board, all_players[0].player_role);
        }
        else
        { // Fallback if player 0 wasn't playing (should not happen in 2-player game)
            strcpy(gop.scores[0].username, "Player1_ERR");
            gop.scores[0].score = 0;
        }

        if (all_players[1].state == P_PLAYING || all_players[1].state == P_DISCONNECTED)
        { // Check if player 1 was part of the game
            strcpy(gop.scores[1].username, all_players[1].username);
            gop.scores[1].score = count_player_pieces_on_board(game_board, all_players[1].player_role);
        }
        else
        { // Fallback
            strcpy(gop.scores[1].username, "Player2_ERR");
            gop.scores[1].score = 0;
        }

        char *json_game_over = serialize_server_game_over(&gop);
        if (json_game_over)
        {
            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                if (all_players[i].socket_fd != -1 && (all_players[i].state == P_PLAYING || all_players[i].state == P_DISCONNECTED))
                {
                    if (send(all_players[i].socket_fd, json_game_over, strlen(json_game_over), 0) == -1 ||
                        send(all_players[i].socket_fd, "\n", 1, 0) == -1) // Added newline
                    {
                        perror("send game_over or newline");
                    }
                    else
                    {
                        printf("Server: Sent 'game_over' to %s (socket %d).\n", all_players[i].username, all_players[i].socket_fd);
                    }
                }
            }
            free(json_game_over);
        }
        else
        {
            fprintf(stderr, "Error serializing ServerGameOverPayload.\n");
        }

        // Clean up players involved in the game
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (all_players[i].socket_fd != -1)
            { // If they haven't been removed by a disconnect already
                // Check if they were part of this game session (e.g. by role or if their state was P_PLAYING)
                // For simplicity, if game over, remove both player slots if they were active.
                // remove_player will set state to P_DISCONNECTED or P_EMPTY.
                remove_player(&all_players[i], all_players, &num_clients, &num_registered_players);
            }
        }

        current_turn_player_index = -1;
        total_moves_made_in_game = 0;
        // num_registered_players should be 0 after remove_player calls.
        // num_clients might also be 0 if no new clients connected in the meantime.

        printf("Server: Game session concluded and reset.\n");
        return 1; // Game is over
    }
    return 0; // Game continues
}

// Function to switch to the next player's turn
void switch_to_next_turn(PlayerState all_players[], char game_board[8][9])
{
    total_moves_made_in_game++;
    printf("Server: Total moves made: %d\n", total_moves_made_in_game);

    if (check_and_process_game_over(all_players, game_board, total_moves_made_in_game))
    {
        return; // Game ended, no next turn
    }

    int next_player_candidate = (current_turn_player_index + 1) % MAX_CLIENTS;
    int initial_candidate = next_player_candidate;

    // Find the next player who is actually in P_PLAYING state
    // This loop prevents infinite loop if no players are P_PLAYING, but game should ideally end before that.
    do
    {
        if (all_players[next_player_candidate].state == P_PLAYING)
        {
            start_player_turn(all_players, next_player_candidate, game_board);
            return;
        }
        next_player_candidate = (next_player_candidate + 1) % MAX_CLIENTS;
    } while (next_player_candidate != initial_candidate);

    // If no playing player is found (e.g., all disconnected or game ended unexpectedly)
    fprintf(stderr, "Error: Could not find a valid next player in P_PLAYING state. Game might be stalled or over.\n");
    current_turn_player_index = -1; // Indicate no current turn
    // Consider ending the game here if this state is reached.
}

// Function to process a move request from a client
void process_move_request(PlayerState *player, const char *received_json_string, PlayerState all_players[], char game_board[8][9])
{
    if (current_turn_player_index == -1 || player->socket_fd != all_players[current_turn_player_index].socket_fd)
    {
        fprintf(stderr, "Server: Received move from %s (socket %d) but it's not their turn. Current turn: %s (socket %d).\n",
                player->username, player->socket_fd,
                (current_turn_player_index != -1 ? all_players[current_turn_player_index].username : "N/A"),
                (current_turn_player_index != -1 ? all_players[current_turn_player_index].socket_fd : -1));

        ServerInvalidMovePayload nack_payload;
        strcpy(nack_payload.type, "invalid_move");
        memcpy(nack_payload.board, game_board, sizeof(nack_payload.board)); // Current board

        // The "next_player" is the player whose turn it actually is.
        if (current_turn_player_index != -1 && all_players[current_turn_player_index].state == P_PLAYING)
        {
            strncpy(nack_payload.next_player, all_players[current_turn_player_index].username, MAX_USERNAME_LEN - 1);
            nack_payload.next_player[MAX_USERNAME_LEN - 1] = '\0';
        }
        else
        {
            // Should not happen if game is ongoing, but as a fallback:
            strcpy(nack_payload.next_player, "N/A");
        }
        // Log this attempt
        log_board_and_move(game_board, player->username, 0, 0, 0, 0, "Attempted Move - Not Your Turn");

        char *json_response_nack = serialize_server_invalid_move(&nack_payload);
        if (json_response_nack)
        {
            if (send(player->socket_fd, json_response_nack, strlen(json_response_nack), 0) == -1 ||
                send(player->socket_fd, "\n", 1, 0) == -1) // Added newline
            {
                perror("send invalid_move (not your turn) or newline");
                // No disconnection here, just an error message to the offending client
            }
            free(json_response_nack);
        }
        else
        {
            fprintf(stderr, "Error serializing ServerInvalidMovePayload for 'not your turn' for %s\n", player->username);
        }
        return; // Do not switch turns
    }

    turn_start_time = time(NULL); // Reset turn timer

    ClientMovePayload move_payload;
    if (deserialize_client_move(received_json_string, &move_payload) != 0)
    {
        fprintf(stderr, "Server: Failed to deserialize move request from %s (socket %d).\n", player->username, player->socket_fd);
        // This is a malformed message, could be grounds for an invalid_move or even disconnect.
        // For now, let's treat as an invalid move that passes the turn.
        log_board_and_move(game_board, player->username, -1, -1, -1, -1, "Deserialization Failed Move");

        ServerInvalidMovePayload nack_payload;
        strcpy(nack_payload.type, "invalid_move");
        memcpy(nack_payload.board, game_board, sizeof(nack_payload.board));
        // Determine next player for the message
        int actual_next_player_idx = (current_turn_player_index + 1) % MAX_CLIENTS;
        int i = 0;
        do
        {
            if (all_players[actual_next_player_idx].state == P_PLAYING)
            {
                strncpy(nack_payload.next_player, all_players[actual_next_player_idx].username, MAX_USERNAME_LEN - 1);
                nack_payload.next_player[MAX_USERNAME_LEN - 1] = '\0';
                break;
            }
            actual_next_player_idx = (actual_next_player_idx + 1) % MAX_CLIENTS;
            i++;
        } while (i < MAX_CLIENTS);
        if (i == MAX_CLIENTS)
            strcpy(nack_payload.next_player, "N/A");

        char *json_response_nack = serialize_server_invalid_move(&nack_payload);
        if (json_response_nack)
        {
            if (send(player->socket_fd, json_response_nack, strlen(json_response_nack), 0) == -1 ||
                send(player->socket_fd, "\n", 1, 0) == -1)
            { // Added newline
                perror("send invalid_move (deserialize failed) or newline");
                handle_client_disconnection(player, all_players, game_board);
            }
            free(json_response_nack);
        }
        switch_to_next_turn(all_players, game_board);
        return;
    }

    int r1 = move_payload.sx;
    int c1 = move_payload.sy;
    int r2 = move_payload.tx;
    int c2 = move_payload.ty;

    // Check for pass attempt [cite: 5]
    if (r1 == 0 && c1 == 0 && r2 == 0 && c2 == 0)
    {
        log_board_and_move(game_board, player->username, r1, c1, r2, c2, "Attempted Pass");
        // For now, assume any 0,0,0,0 is a valid pass request [cite: 5]
        // "If the pass is valid, respond with move_ok"
        // If pass validation (e.g. checking if other moves are impossible) is needed, it would go here.
        // Current placeholder game logic does not have a way to check if a pass is forced.
        // So, we treat this as a voluntary, valid pass.

        ServerMoveOkPayload ok_payload; // Pass is like a move that doesn't change the board
        strcpy(ok_payload.type, "move_ok");
        memcpy(ok_payload.board, game_board, sizeof(ok_payload.board)); // Board doesn't change on pass

        // Determine actual next player's username for the message
        int actual_next_player_idx = current_turn_player_index; // Start with current to find next
        int i = 0;
        do
        {
            actual_next_player_idx = (actual_next_player_idx + 1) % MAX_CLIENTS;
            if (all_players[actual_next_player_idx].state == P_PLAYING)
            {
                strncpy(ok_payload.next_player, all_players[actual_next_player_idx].username, MAX_USERNAME_LEN - 1);
                ok_payload.next_player[MAX_USERNAME_LEN - 1] = '\0';
                break;
            }
            i++;
        } while (i < MAX_CLIENTS);
        if (i == MAX_CLIENTS)
            strcpy(ok_payload.next_player, "N/A"); // Should not happen

        char *json_response = serialize_server_move_ok(&ok_payload);
        if (json_response)
        {
            if (send(player->socket_fd, json_response, strlen(json_response), 0) == -1 ||
                send(player->socket_fd, "\n", 1, 0) == -1)
            { // Added newline
                perror("send move_ok (for pass) or newline");
                handle_client_disconnection(player, all_players, game_board);
            }
            else
            {
                printf("Server: Sent 'move_ok' (for pass) to %s.\n", player->username);
                log_board_and_move(game_board, player->username, r1, c1, r2, c2, "Valid Pass");
            }
            free(json_response);
        }
        else
        {
            fprintf(stderr, "Error serializing ServerMoveOkPayload for pass for %s\n", player->username);
        }
        switch_to_next_turn(all_players, game_board);
        return;
    }

    // Regular move
    log_board_and_move(game_board, player->username, r1, c1, r2, c2, "Attempted Move");
    char original_board_on_invalid_move[8][9];
    memcpy(original_board_on_invalid_move, game_board, sizeof(original_board_on_invalid_move));

    if (validate_and_process_move(game_board, r1, c1, r2, c2, player->player_role))
    {
        log_board_and_move(game_board, player->username, r1, c1, r2, c2, "Valid Move");
        ServerMoveOkPayload ok_payload;
        strcpy(ok_payload.type, "move_ok");
        memcpy(ok_payload.board, game_board, sizeof(ok_payload.board));

        int actual_next_player_idx = current_turn_player_index;
        int i = 0;
        do
        {
            actual_next_player_idx = (actual_next_player_idx + 1) % MAX_CLIENTS;
            if (all_players[actual_next_player_idx].state == P_PLAYING)
            {
                strncpy(ok_payload.next_player, all_players[actual_next_player_idx].username, MAX_USERNAME_LEN - 1);
                ok_payload.next_player[MAX_USERNAME_LEN - 1] = '\0';
                break;
            }
            i++;
        } while (i < MAX_CLIENTS);
        if (i == MAX_CLIENTS)
            strcpy(ok_payload.next_player, "N/A");

        char *json_response = serialize_server_move_ok(&ok_payload);
        if (json_response)
        {
            if (send(player->socket_fd, json_response, strlen(json_response), 0) == -1 ||
                send(player->socket_fd, "\n", 1, 0) == -1) // Added newline
            {
                perror("send move_ok or newline");
                handle_client_disconnection(player, all_players, game_board);
            }
            else
            {
                printf("Server: Sent 'move_ok' to %s.\n", player->username);
            }
            free(json_response);
        }
        else
        {
            fprintf(stderr, "Error serializing ServerMoveOkPayload for %s\n", player->username);
        }
        switch_to_next_turn(all_players, game_board);
    }
    else
    {
        log_board_and_move(original_board_on_invalid_move, player->username, r1, c1, r2, c2, "Invalid Move");
        ServerInvalidMovePayload nack_payload;
        strcpy(nack_payload.type, "invalid_move");
        memcpy(nack_payload.board, original_board_on_invalid_move, sizeof(nack_payload.board));

        int actual_next_player_idx = current_turn_player_index;
        int i = 0;
        do
        {
            actual_next_player_idx = (actual_next_player_idx + 1) % MAX_CLIENTS;
            if (all_players[actual_next_player_idx].state == P_PLAYING)
            {
                strncpy(nack_payload.next_player, all_players[actual_next_player_idx].username, MAX_USERNAME_LEN - 1);
                nack_payload.next_player[MAX_USERNAME_LEN - 1] = '\0';
                break;
            }
            i++;
        } while (i < MAX_CLIENTS);
        if (i == MAX_CLIENTS)
            strcpy(nack_payload.next_player, "N/A");

        char *json_response_nack = serialize_server_invalid_move(&nack_payload);
        if (json_response_nack)
        {
            if (send(player->socket_fd, json_response_nack, strlen(json_response_nack), 0) == -1 ||
                send(player->socket_fd, "\n", 1, 0) == -1) // Added newline
            {
                perror("send invalid_move or newline");
                handle_client_disconnection(player, all_players, game_board);
            }
            else
            {
                printf("Server: Sent 'invalid_move' to %s.\n", player->username);
            }
            free(json_response_nack);
        }
        else
        {
            fprintf(stderr, "Error serializing ServerInvalidMovePayload for %s\n", player->username);
        }
        switch_to_next_turn(all_players, game_board);
    }
}

// Function to handle turn timeout
void handle_turn_timeout(PlayerState all_players[], char game_board[8][9])
{
    if (current_turn_player_index == -1 || all_players[current_turn_player_index].state != P_PLAYING)
    {
        return;
    }

    PlayerState *timed_out_player = &all_players[current_turn_player_index];
    printf("Server: Player %s (socket %d) timed out.\n", timed_out_player->username, timed_out_player->socket_fd);
    log_board_and_move(game_board, timed_out_player->username, -1, -1, -1, -1, "Timeout Pass");

    ServerPassPayload pass_payload;
    strcpy(pass_payload.type, "pass");

    // Determine next player's username
    int next_player_idx = (current_turn_player_index + 1) % MAX_CLIENTS;
    if (all_players[next_player_idx].state == P_PLAYING)
    {
        strcpy(pass_payload.next_player, all_players[next_player_idx].username);
    }
    else
    {
        // Find the actual next playing player for the message
        int temp_next_idx = current_turn_player_index;
        int i = 0;
        do
        {
            temp_next_idx = (temp_next_idx + 1) % MAX_CLIENTS;
            if (all_players[temp_next_idx].state == P_PLAYING)
            {
                strcpy(pass_payload.next_player, all_players[temp_next_idx].username);
                break;
            }
            i++;
        } while (i < MAX_CLIENTS);
        if (i == MAX_CLIENTS)
            strcpy(pass_payload.next_player, "N/A"); // Should not happen
    }

    char *json_response = serialize_server_pass(&pass_payload);
    if (json_response)
    {
        if (send(timed_out_player->socket_fd, json_response, strlen(json_response), 0) == -1 ||
            send(timed_out_player->socket_fd, "\n", 1, 0) == -1) // Added newline
        {
            perror("send pass on timeout or newline");
            handle_client_disconnection(timed_out_player, all_players, game_board);
        }
        else
        {
            printf("Server: Sent 'pass' to %s due to timeout.\n", timed_out_player->username);
        }
        free(json_response);
    }
    else
    {
        fprintf(stderr, "Error serializing ServerPassPayload for %s\n", timed_out_player->username);
    }

    switch_to_next_turn(all_players, game_board);
}

// Function to initialize player states
void initialize_player_states(PlayerState all_players[])
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        all_players[i].socket_fd = -1; // Indicates slot is empty
        all_players[i].state = P_EMPTY;
        memset(all_players[i].username, 0, MAX_USERNAME_LEN);
        all_players[i].player_role = ' ';
        all_players[i].recv_buffer_len = 0;
        all_players[i].recv_buffer[0] = '\0';
    }
}

// Function to initialize the server socket
int initialize_server_socket(const char *port)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            close(sockfd);          // Close the socket if setsockopt fails
            freeaddrinfo(servinfo); // Free the servinfo list
            return -1;              // Return error
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        return -1;
    }

    if (listen(sockfd, LISTEN_BACKLOG) == -1)
    {
        perror("listen");
        close(sockfd); // Close the socket if listen fails
        return -1;     // Return error
    }
    return sockfd;
}

// Function to add a player to the list
void add_player(int client_socket, struct sockaddr_storage *client_addr, socklen_t addr_len, PlayerState all_players[], int *current_num_clients)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (all_players[i].socket_fd == -1)
        {
            all_players[i].socket_fd = client_socket;
            all_players[i].address = *client_addr;
            all_players[i].addr_len = addr_len;
            all_players[i].state = P_CONNECTED;
            all_players[i].last_message_time = time(NULL);
            (*current_num_clients)++;

            FD_SET(client_socket, &master_fds);
            if (client_socket > fd_max)
            {
                fd_max = client_socket;
            }
            char ip_str[INET6_ADDRSTRLEN];
            inet_ntop(client_addr->ss_family,
                      (client_addr->ss_family == AF_INET) ? (void *)&(((struct sockaddr_in *)client_addr)->sin_addr)
                                                          : (void *)&(((struct sockaddr_in6 *)client_addr)->sin6_addr),
                      ip_str, sizeof(ip_str));
            printf("Server: New connection from %s on socket %d. Client slot %d.\n", ip_str, client_socket, i);
            return;
        }
    }
    // Should not happen if num_clients check is done before calling add_player
    fprintf(stderr, "Error: Tried to add player but no empty slots (this should not happen).\n");
    close(client_socket);
}

// Function to remove a player
void remove_player(PlayerState *player_to_remove, PlayerState all_players[], int *current_num_clients, int *current_num_registered_players)
{
    if (player_to_remove->socket_fd != -1)
    {
        printf("Server: Closing connection for socket %d (username: %s)\n", player_to_remove->socket_fd, player_to_remove->username[0] ? player_to_remove->username : "N/A");

        // Removed logic for advancing turn if current player disconnects.
        // This is now handled by handle_client_disconnection.

        close(player_to_remove->socket_fd);
        FD_CLR(player_to_remove->socket_fd, &master_fds);

        if (player_to_remove->state == P_REGISTERED || player_to_remove->state == P_PLAYING)
        {
            if (*current_num_registered_players > 0)
            {
                (*current_num_registered_players)--;
            }
        }

        player_to_remove->socket_fd = -1;
        player_to_remove->state = P_DISCONNECTED; // Or P_EMPTY
        memset(player_to_remove->username, 0, MAX_USERNAME_LEN);
        player_to_remove->player_role = ' ';

        if (*current_num_clients > 0)
        {
            (*current_num_clients)--;
        }
    }
}

// Function to accept a new connection
void accept_new_connection(int current_listener_fd, PlayerState all_players[], int *current_num_clients)
{
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_fd = accept(current_listener_fd, (struct sockaddr *)&client_addr, &addr_len);

    if (new_fd == -1)
    {
        perror("accept");
        return;
    }

    if (*current_num_clients >= MAX_CLIENTS)
    {
        fprintf(stderr, "Server: Maximum clients reached. Rejecting new connection from socket %d.\n", new_fd);
        // TODO: Optionally send a "server full" message to the client
        const char *msg = "Server is full. Try again later.\n";
        send(new_fd, msg, strlen(msg), 0);
        close(new_fd);
    }
    else
    {
        add_player(new_fd, &client_addr, addr_len, all_players, current_num_clients);
    }
}

// Function to handle client disconnection and associated game logic
void handle_client_disconnection(PlayerState *disconnected_player, PlayerState all_players[], char game_board[8][9])
{
    if (disconnected_player == NULL || disconnected_player->socket_fd == -1)
    {
        // Already processed or invalid player pointer
        return;
    }

    printf("Server: Handling disconnection for player %s (socket %d).\n",
           disconnected_player->username[0] ? disconnected_player->username : "N/A",
           disconnected_player->socket_fd);

    char disconnected_username_copy[MAX_USERNAME_LEN];
    strncpy(disconnected_username_copy, disconnected_player->username, MAX_USERNAME_LEN - 1);
    disconnected_username_copy[MAX_USERNAME_LEN - 1] = '\0';

    int game_was_active_with_two_players = (num_registered_players == MAX_CLIENTS && current_turn_player_index != -1);
    int disconnected_player_slot = -1;
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (&all_players[i] == disconnected_player)
        {
            disconnected_player_slot = i;
            break;
        }
    }

    remove_player(disconnected_player, all_players, &num_clients, &num_registered_players);

    if (game_was_active_with_two_players && num_registered_players == 1)
    {
        // One player left, game ends by forfeit.
        PlayerState *remaining_player = NULL;
        int remaining_player_slot = -1;
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (all_players[i].socket_fd != -1 && (all_players[i].state == P_PLAYING || all_players[i].state == P_REGISTERED))
            {
                remaining_player = &all_players[i];
                remaining_player_slot = i;
                break;
            }
        }

        if (remaining_player)
        {
            printf("Server: Player %s disconnected. Player %s wins by forfeit.\n",
                   disconnected_username_copy, remaining_player->username);

            ServerGameOverPayload gop;
            strcpy(gop.type, "game_over");

            // Determine payload indices based on original player slots if possible,
            // or assign based on who is remaining vs disconnected.
            // Assuming players[0] and players[1] map to gop.scores[0] and gop.scores[1].
            // This requires knowing the original slot of the disconnected player.

            int r_payload_idx = (remaining_player_slot == 0) ? 0 : 1;    // if remaining was players[0] -> scores[0]
            int d_payload_idx = (disconnected_player_slot == 0) ? 0 : 1; // if disconnected was players[0] -> scores[0]

            // This logic might be flawed if slots don't map directly or if one is -1.
            // Simplified: assign scores based on who is who.
            // Let's ensure scores[0] is for players[0] if it was involved, scores[1] for players[1].
            // If disconnected_player_slot is known:
            if (disconnected_player_slot == 0)
            { // disconnected was players[0]
                strcpy(gop.scores[0].username, disconnected_username_copy);
                gop.scores[0].score = 0; // Loss by forfeit
                strcpy(gop.scores[1].username, remaining_player->username);
                gop.scores[1].score = count_player_pieces_on_board(game_board, remaining_player->player_role); // Or 1 for win
            }
            else if (disconnected_player_slot == 1)
            { // disconnected was players[1]
                strcpy(gop.scores[1].username, disconnected_username_copy);
                gop.scores[1].score = 0; // Loss by forfeit
                strcpy(gop.scores[0].username, remaining_player->username);
                gop.scores[0].score = count_player_pieces_on_board(game_board, remaining_player->player_role); // Or 1 for win
            }
            else
            { // Fallback if slot unknown (should not happen if logic is correct)
                // This case needs robust handling if player slots are dynamic or not fixed.
                // For now, assume fixed player slots 0 and 1.
                // If remaining_player is players[0], then disconnected must have been players[1] for gop.
                if (remaining_player == &all_players[0])
                {
                    strcpy(gop.scores[0].username, remaining_player->username);
                    gop.scores[0].score = count_player_pieces_on_board(game_board, remaining_player->player_role);
                    strcpy(gop.scores[1].username, disconnected_username_copy);
                    gop.scores[1].score = 0;
                }
                else
                { // remaining_player is players[1]
                    strcpy(gop.scores[1].username, remaining_player->username);
                    gop.scores[1].score = count_player_pieces_on_board(game_board, remaining_player->player_role);
                    strcpy(gop.scores[0].username, disconnected_username_copy);
                    gop.scores[0].score = 0;
                }
            }

            char *json_game_over = serialize_server_game_over(&gop);
            if (json_game_over)
            {
                if (send(remaining_player->socket_fd, json_game_over, strlen(json_game_over), 0) == -1 ||
                    send(remaining_player->socket_fd, "\n", 1, 0) == -1) // Added newline
                {
                    perror("send game_over on forfeit or newline");
                }
                free(json_game_over);
            }
            remove_player(remaining_player, all_players, &num_clients, &num_registered_players);
        }
        // Reset game state
        current_turn_player_index = -1;
        total_moves_made_in_game = 0;
        printf("Server: Game session ended due to disconnection and reset.\n");
    }
    else if (num_registered_players == 0 && game_was_active_with_two_players)
    {
        // This means the second player of an active game disconnected.
        printf("Server: Both players have disconnected from an active game. Resetting.\n");
        current_turn_player_index = -1;
        total_moves_made_in_game = 0;
    }
    else if (num_registered_players == 0)
    {
        // Game was not active or already ended, and last client(s) left.
        printf("Server: All clients disconnected or game already over. Server idle or reset.\n");
        current_turn_player_index = -1; // Ensure reset
        total_moves_made_in_game = 0;
    }
}

// Function to handle messages from a client
void handle_client_message(PlayerState *player, PlayerState all_players[], int *current_num_clients, int *current_num_registered_players, char game_board[8][9])
{
    char buf[BUFFER_SIZE]; // Temporary buffer for recv
    int nbytes = recv(player->socket_fd, buf, sizeof(buf) - 1, 0);

    if (nbytes <= 0)
    {
        if (nbytes == 0)
        {
            printf("Server: Socket %d (username: %s) hung up.\n", player->socket_fd, player->username[0] ? player->username : "N/A");
        }
        else
        {
            perror("recv");
        }
        handle_client_disconnection(player, all_players, game_board);
        return;
    }

    // Check for buffer overflow before appending
    if (player->recv_buffer_len + nbytes >= PLAYER_RECV_BUFFER_MAX_LEN)
    {
        fprintf(stderr, "Server: Receive buffer overflow for player %s (socket %d). Disconnecting.\n",
                player->username[0] ? player->username : "N/A", player->socket_fd);
        handle_client_disconnection(player, all_players, game_board);
        return;
    }

    // Append new data to player's persistent buffer
    memcpy(player->recv_buffer + player->recv_buffer_len, buf, nbytes);
    player->recv_buffer_len += nbytes;
    player->recv_buffer[player->recv_buffer_len] = '\0'; // Null-terminate for strchr

    // Process all complete newline-terminated messages in the buffer
    char *current_pos = player->recv_buffer;
    char *newline_ptr;
    while ((newline_ptr = strchr(current_pos, '\n')) != NULL)
    {
        int message_len = newline_ptr - current_pos;
        char json_message[message_len + 1];
        strncpy(json_message, current_pos, message_len);
        json_message[message_len] = '\0';

        printf("Server: Processing message from socket %d (username: %s): %s\n",
               player->socket_fd, player->username[0] ? player->username : "N/A", json_message);

        player->last_message_time = time(NULL); // Update activity time

        const char *msg_type_const = get_message_type_from_json(json_message);
        if (msg_type_const == NULL)
        {
            fprintf(stderr, "Server: Could not determine message type from: %s. Player: %s\n", json_message, player->username);
            // Decide if to skip or disconnect. For now, skip to next potential message.
        }
        else
        {
            if (strcmp(msg_type_const, "register") == 0)
            {
                process_registration_request(player, json_message, all_players, current_num_registered_players, game_board);
            }
            else if (strcmp(msg_type_const, "move") == 0)
            {
                if (player->state == P_PLAYING && current_turn_player_index != -1 &&
                    all_players[current_turn_player_index].socket_fd == player->socket_fd)
                {
                    process_move_request(player, json_message, all_players, game_board);
                }
                else
                {
                    fprintf(stderr, "Server: Move received from %s but not their turn or not playing.\n", player->username);
                    // Optionally send an error message back
                }
            }
            else
            {
                fprintf(stderr, "Server: Unknown message type '%s' from %s.\n", msg_type_const, player->username);
                // Optionally send an error to client
            }
            free((void *)msg_type_const); // Free the string returned by get_message_type_from_json
        }

        // Move to the character after the processed newline
        current_pos = newline_ptr + 1;
    }

    // Shift remaining partial message (if any) to the beginning of the buffer
    int remaining_len = player->recv_buffer_len - (current_pos - player->recv_buffer);
    if (remaining_len > 0 && current_pos != player->recv_buffer)
    {
        memmove(player->recv_buffer, current_pos, remaining_len);
    }
    player->recv_buffer_len = remaining_len;
    player->recv_buffer[player->recv_buffer_len] = '\0'; // Null-terminate the potentially shifted buffer
}

int main(int argc, char *argv[])
{
    const char *port = SERVER_PORT;

    initialize_player_states(players);

    listener_fd = initialize_server_socket(port);
    if (listener_fd == -1)
    {
        fprintf(stderr, "Failed to initialize server socket. Exiting.\n");
        exit(1);
    }

    FD_ZERO(&master_fds);
    FD_ZERO(&read_fds);
    FD_SET(listener_fd, &master_fds);
    fd_max = listener_fd;

    printf("Server: Listening on port %s...\n", port);

    // Example: Initialize board (replace with your actual game start board)
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            octaflip_board[i][j] = '.'; // Empty
        }
        octaflip_board[i][8] = '\0'; // Null terminate rows
    }

    // --- Game Start Simulation (for testing turn logic) ---
    // This would normally happen after two players register and are ready.
    // For now, let's assume players[0] and players[1] will be 'R' and 'B'
    // and the game starts immediately if they connect.
    // This is a placeholder for actual game start logic.
    // if (num_registered_players == MAX_CLIENTS) { // Or some other condition
    //    players[0].player_role = 'R'; players[0].state = P_PLAYING;
    //    players[1].player_role = 'B'; players[1].state = P_PLAYING;
    //    current_turn_player_index = 0; // Player R starts
    //    start_player_turn(players, current_turn_player_index, octaflip_board);
    // }
    // --- End Game Start Simulation ---

    while (1)
    {
        read_fds = master_fds; // Copy it
        struct timeval tv;
        tv.tv_sec = 1; // 1-second timeout for select
        tv.tv_usec = 0;

        int activity = select(fd_max + 1, &read_fds, NULL, NULL, &tv);

        if (activity == -1)
        {
            if (errno == EINTR)
            { // Interrupted by signal, continue
                continue;
            }
            perror("select");
            // Consider more robust error handling
            exit(4);
        }

        // Check for turn timeout if select timed out or even if there was activity
        // This check should happen regardless of 'activity > 0' if a game is in progress
        if (current_turn_player_index != -1 && players[current_turn_player_index].state == P_PLAYING)
        {
            if (time(NULL) - turn_start_time >= TURN_TIMEOUT_SECONDS)
            {
                handle_turn_timeout(players, octaflip_board);
            }
        }

        // Run through the existing connections looking for data to read
        for (int i = 0; i <= fd_max; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                if (i == listener_fd)
                {
                    // Handle new connections
                    accept_new_connection(listener_fd, players, &num_clients);
                }
                else
                {
                    // Handle data from a client
                    PlayerState *current_player = NULL;
                    for (int j = 0; j < MAX_CLIENTS; j++)
                    {
                        if (players[j].socket_fd == i)
                        {
                            current_player = &players[j];
                            break;
                        }
                    }
                    if (current_player)
                    {
                        handle_client_message(current_player, players, &num_clients, &num_registered_players, octaflip_board);
                    }
                    else
                    {
                        // This should ideally not happen if fd_sets are managed correctly
                        fprintf(stderr, "Warning: Activity on unmanaged socket %d\n", i);
                        FD_CLR(i, &master_fds); // Remove from master set if it's an anomaly
                    }
                }
            }
        }
        // TODO: Add timeout checks for players if necessary
        // (e.g., iterate players, check time(NULL) - player.last_message_time > TIMEOUT_SECONDS)
        // if timeout, call remove_player()
    }

    // Cleanup (though this loop runs forever in this basic setup)
    close(listener_fd);
    // TODO: Close all client sockets if any remain
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (players[i].socket_fd != -1)
        {
            close(players[i].socket_fd);
        }
    }

    return 0;
}
