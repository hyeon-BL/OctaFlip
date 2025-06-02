#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include "protocol.h"
#include "cJSON.h"
#include "board.h" // Added for LED matrix control

#define BUFFER_SIZE 2048
#define CLIENT_RECV_BUFFER_MAX_LEN (BUFFER_SIZE * 2)

char client_username[MAX_USERNAME_LEN];
char my_player_symbol = ' ';
char client_recv_buffer[CLIENT_RECV_BUFFER_MAX_LEN];
int client_recv_buffer_len = 0;

static struct RGBLedMatrix *matrix_ptr = NULL; // Pointer for the LED matrix

typedef struct
{
    int sx;
    int sy;
    int tx;
    int ty;
} MoveCoords;

// Helper: Check if coordinates are within board limits (client-side)
static int isWithinBounds_client(int r, int c)
{
    return (r >= 0 && r < 8 && c >= 0 && c < 8);
}

// Automated Move Generation Function
MoveCoords move_generate(char current_board[BOARD_ROWS][BOARD_COLS + 2], char player_symbol)
{
    MoveCoords move;

    // Try to find a valid clone move first
    for (int r_src = 0; r_src < 8; ++r_src)
    {
        for (int c_src = 0; c_src < 8; ++c_src)
        {
            if (current_board[r_src][c_src] == player_symbol)
            {
                // Check 1-step neighbors for clone
                for (int dr = -1; dr <= 1; ++dr)
                {
                    for (int dc = -1; dc <= 1; ++dc)
                    {
                        if (dr == 0 && dc == 0)
                            continue;

                        int r_dest = r_src + dr;
                        int c_dest = c_src + dc;

                        if (isWithinBounds_client(r_dest, c_dest) && current_board[r_dest][c_dest] == '.')
                        {
                            move.sx = r_src + 1;
                            move.sy = c_src + 1;
                            move.tx = r_dest + 1;
                            move.ty = c_dest + 1;
                            printf("Client AI: Found clone move (%d,%d) -> (%d,%d)\n", move.sx, move.sy, move.tx, move.ty);
                            return move;
                        }
                    }
                }
            }
        }
    }

    // If no clone move, try to find a valid jump move
    for (int r_src = 0; r_src < 8; ++r_src)
    {
        for (int c_src = 0; c_src < 8; ++c_src)
        {
            if (current_board[r_src][c_src] == player_symbol)
            {
                // Check 2-step neighbors for jump
                for (int dr = -1; dr <= 1; ++dr)
                {
                    for (int dc = -1; dc <= 1; ++dc)
                    {
                        if (dr == 0 && dc == 0)
                            continue;

                        int r_dest = r_src + 2 * dr;
                        int c_dest = c_src + 2 * dc;

                        if (isWithinBounds_client(r_dest, c_dest) && current_board[r_dest][c_dest] == '.')
                        {
                            move.sx = r_src + 1;
                            move.sy = c_src + 1;
                            move.tx = r_dest + 1;
                            move.ty = c_dest + 1;
                            printf("Client AI: Found jump move (%d,%d) -> (%d,%d)\n", move.sx, move.sy, move.tx, move.ty);
                            return move;
                        }
                    }
                }
            }
        }
    }

    // If no valid moves found, pass (0,0,0,0 indicates pass)
    printf("Client AI: No valid moves found. Passing.\n");
    move.sx = 0;
    move.sy = 0;
    move.tx = 0;
    move.ty = 0;
    return move;
}

// JSON Utility Function Implementations (Client-side)

// Helper function to get message type from JSON
char *get_message_type_from_json(const char *json_string)
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

    char *type_str = strdup(type_json->valuestring);
    cJSON_Delete(root);
    return type_str;
}

// Serialization for ClientRegisterPayload
char *serialize_client_register(const ClientRegisterPayload *payload)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return NULL;

    if (cJSON_AddStringToObject(root, "type", payload->type) == NULL)
    {
        cJSON_Delete(root);
        return NULL;
    }
    if (cJSON_AddStringToObject(root, "username", payload->username) == NULL)
    {
        cJSON_Delete(root);
        return NULL;
    }

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

// Deserialization for ServerRegisterAckPayload
int deserialize_server_register_ack(const char *json_string, ServerRegisterAckPayload *out_payload)
{
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL)
        return -1;

    cJSON *type_json = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type_json) || strcmp(type_json->valuestring, "register_ack") != 0)
    {
        cJSON_Delete(root);
        return -1;
    }
    strcpy(out_payload->type, "register_ack");
    cJSON_Delete(root);
    return 0;
}

// Deserialization for ServerRegisterNackPayload
int deserialize_server_register_nack(const char *json_string, ServerRegisterNackPayload *out_payload)
{
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL)
        return -1;

    cJSON *type_json = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type_json) || strcmp(type_json->valuestring, "register_nack") != 0)
    {
        cJSON_Delete(root);
        return -1;
    }
    strcpy(out_payload->type, "register_nack");

    cJSON *reason_json = cJSON_GetObjectItemCaseSensitive(root, "reason");
    if (!cJSON_IsString(reason_json) || (reason_json->valuestring == NULL))
    {
        cJSON_Delete(root);
        return -1;
    }
    strncpy(out_payload->reason, reason_json->valuestring, MAX_REASON_LEN - 1);
    out_payload->reason[MAX_REASON_LEN - 1] = '\0';

    cJSON_Delete(root);
    return 0;
}

// Deserialization for ServerGameStartPayload
int deserialize_server_game_start(const char *json_string, ServerGameStartPayload *out_payload)
{
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL)
        return -1;

    cJSON *type_json = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type_json) || strcmp(type_json->valuestring, "game_start") != 0)
    {
        cJSON_Delete(root);
        return -1;
    }
    strcpy(out_payload->type, "game_start");

    cJSON *players_json = cJSON_GetObjectItemCaseSensitive(root, "players");
    if (!cJSON_IsArray(players_json) || cJSON_GetArraySize(players_json) != 2)
    {
        cJSON_Delete(root);
        return -1;
    }
    for (int i = 0; i < 2; ++i)
    {
        cJSON *player_name_json = cJSON_GetArrayItem(players_json, i);
        if (!cJSON_IsString(player_name_json) || (player_name_json->valuestring == NULL))
        {
            cJSON_Delete(root);
            return -1;
        }
        strncpy(out_payload->players[i], player_name_json->valuestring, MAX_USERNAME_LEN - 1);
        out_payload->players[i][MAX_USERNAME_LEN - 1] = '\0';
    }

    cJSON *first_player_json = cJSON_GetObjectItemCaseSensitive(root, "first_player");
    if (!cJSON_IsString(first_player_json) || (first_player_json->valuestring == NULL))
    {
        cJSON_Delete(root);
        return -1;
    }
    strncpy(out_payload->first_player, first_player_json->valuestring, MAX_USERNAME_LEN - 1);
    out_payload->first_player[MAX_USERNAME_LEN - 1] = '\0';

    cJSON_Delete(root);
    return 0;
}

// Serialization for ClientMovePayload
char *serialize_client_move(const ClientMovePayload *payload)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return NULL;

    if (cJSON_AddStringToObject(root, "type", payload->type) == NULL)
        goto error;
    if (cJSON_AddStringToObject(root, "username", payload->username) == NULL)
        goto error;
    if (cJSON_AddNumberToObject(root, "sx", payload->sx) == NULL)
        goto error;
    if (cJSON_AddNumberToObject(root, "sy", payload->sy) == NULL)
        goto error;
    if (cJSON_AddNumberToObject(root, "tx", payload->tx) == NULL)
        goto error;
    if (cJSON_AddNumberToObject(root, "ty", payload->ty) == NULL)
        goto error;

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;

error:
    cJSON_Delete(root);
    return NULL;
}

// Deserialization for ServerYourTurnPayload
int deserialize_server_your_turn(const char *json_string, ServerYourTurnPayload *out_payload)
{
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL)
        return -1;

    cJSON *type_json = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type_json) || strcmp(type_json->valuestring, "your_turn") != 0)
    {
        cJSON_Delete(root);
        return -1;
    }
    strcpy(out_payload->type, "your_turn");

    cJSON *board_json = cJSON_GetObjectItemCaseSensitive(root, "board");
    if (!cJSON_IsArray(board_json) || cJSON_GetArraySize(board_json) != 8)
    {
        cJSON_Delete(root);
        return -1;
    }
    for (int i = 0; i < 8; ++i)
    {
        cJSON *row_json = cJSON_GetArrayItem(board_json, i);
        if (!cJSON_IsString(row_json) || (row_json->valuestring == NULL))
        {
            cJSON_Delete(root);
            return -1;
        }
        strncpy(out_payload->board[i], row_json->valuestring, 8);
        out_payload->board[i][8] = '\0';
    }

    cJSON *timeout_json = cJSON_GetObjectItemCaseSensitive(root, "timeout");
    if (!cJSON_IsNumber(timeout_json))
    {
        cJSON_Delete(root);
        return -1;
    }
    out_payload->timeout = timeout_json->valuedouble;

    cJSON_Delete(root);
    return 0;
}

// Deserialization for ServerMoveOkPayload
int deserialize_server_move_ok(const char *json_string, ServerMoveOkPayload *out_payload)
{
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL)
        return -1;

    cJSON *type_json = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type_json) || strcmp(type_json->valuestring, "move_ok") != 0)
    {
        cJSON_Delete(root);
        return -1;
    }
    strcpy(out_payload->type, "move_ok");

    cJSON *board_json = cJSON_GetObjectItemCaseSensitive(root, "board");
    if (!cJSON_IsArray(board_json) || cJSON_GetArraySize(board_json) != 8)
    {
        cJSON_Delete(root);
        return -1;
    }
    for (int i = 0; i < 8; ++i)
    {
        cJSON *row_json = cJSON_GetArrayItem(board_json, i);
        if (!cJSON_IsString(row_json) || (row_json->valuestring == NULL))
        {
            cJSON_Delete(root);
            return -1;
        }
        strncpy(out_payload->board[i], row_json->valuestring, 8);
        out_payload->board[i][8] = '\0';
    }

    cJSON *next_player_json = cJSON_GetObjectItemCaseSensitive(root, "next_player");
    if (!cJSON_IsString(next_player_json) || (next_player_json->valuestring == NULL))
    {
        cJSON_Delete(root);
        return -1;
    }
    strncpy(out_payload->next_player, next_player_json->valuestring, MAX_USERNAME_LEN - 1);
    out_payload->next_player[MAX_USERNAME_LEN - 1] = '\0';

    cJSON_Delete(root);
    return 0;
}

// Deserialization for ServerInvalidMovePayload
int deserialize_server_invalid_move(const char *json_string, ServerInvalidMovePayload *out_payload)
{
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL)
        return -1;

    cJSON *type_json = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type_json) || strcmp(type_json->valuestring, "invalid_move") != 0)
    {
        cJSON_Delete(root);
        return -1;
    }
    strcpy(out_payload->type, "invalid_move");

    cJSON *board_json = cJSON_GetObjectItemCaseSensitive(root, "board");
    if (!cJSON_IsArray(board_json) || cJSON_GetArraySize(board_json) != 8)
    {
        cJSON_Delete(root);
        return -1;
    }
    for (int i = 0; i < 8; ++i)
    {
        cJSON *row_json = cJSON_GetArrayItem(board_json, i);
        if (!cJSON_IsString(row_json) || (row_json->valuestring == NULL))
        {
            cJSON_Delete(root);
            return -1;
        }
        strncpy(out_payload->board[i], row_json->valuestring, 8);
        out_payload->board[i][8] = '\0';
    }

    cJSON *next_player_json = cJSON_GetObjectItemCaseSensitive(root, "next_player");
    if (!cJSON_IsString(next_player_json) || (next_player_json->valuestring == NULL))
    {
        cJSON_Delete(root);
        return -1;
    }
    strncpy(out_payload->next_player, next_player_json->valuestring, MAX_USERNAME_LEN - 1);
    out_payload->next_player[MAX_USERNAME_LEN - 1] = '\0';

    // Optionally parse the reason field
    cJSON *reason_json = cJSON_GetObjectItemCaseSensitive(root, "reason");
    if (cJSON_IsString(reason_json) && (reason_json->valuestring != NULL))
    {
        strncpy(out_payload->reason, reason_json->valuestring, MAX_REASON_LEN - 1);
        out_payload->reason[MAX_REASON_LEN - 1] = '\0';
    }
    else
    {
        out_payload->reason[0] = '\0';
    }

    cJSON_Delete(root);
    return 0;
}

// Deserialization for ServerPassPayload
int deserialize_server_pass(const char *json_string, ServerPassPayload *out_payload)
{
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL)
        return -1;

    cJSON *type_json = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type_json) || strcmp(type_json->valuestring, "pass") != 0)
    {
        cJSON_Delete(root);
        return -1;
    }
    strcpy(out_payload->type, "pass");

    cJSON *next_player_json = cJSON_GetObjectItemCaseSensitive(root, "next_player");
    if (!cJSON_IsString(next_player_json) || (next_player_json->valuestring == NULL))
    {
        cJSON_Delete(root);
        return -1;
    }
    strncpy(out_payload->next_player, next_player_json->valuestring, MAX_USERNAME_LEN - 1);
    out_payload->next_player[MAX_USERNAME_LEN - 1] = '\0';

    cJSON_Delete(root);
    return 0;
}

// Deserialization for ServerGameOverPayload
int deserialize_server_game_over(const char *json_string, ServerGameOverPayload *out_payload)
{
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL)
        return -1;

    cJSON *type_json = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type_json) || strcmp(type_json->valuestring, "game_over") != 0)
    {
        cJSON_Delete(root);
        return -1;
    }
    strcpy(out_payload->type, "game_over");

    cJSON *scores_obj_json = cJSON_GetObjectItemCaseSensitive(root, "scores");
    if (!cJSON_IsObject(scores_obj_json))
    {
        cJSON_Delete(root);
        return -1;
    }

    int score_idx = 0;
    cJSON *current_score_item = NULL;
    cJSON_ArrayForEach(current_score_item, scores_obj_json)
    {
        if (score_idx < 2)
        {
            if (current_score_item->string != NULL && cJSON_IsNumber(current_score_item))
            {
                strncpy(out_payload->scores[score_idx].username, current_score_item->string, MAX_USERNAME_LEN - 1);
                out_payload->scores[score_idx].username[MAX_USERNAME_LEN - 1] = '\0';
                out_payload->scores[score_idx].score = current_score_item->valueint;
                score_idx++;
            }
            else
            {
                fprintf(stderr, "Warning: Malformed score item in game_over message.\n");
            }
        }
        else
        {
            fprintf(stderr, "Warning: More than 2 scores received in game_over message. Ignoring extras.\n");
            break;
        }
    }

    for (int i = score_idx; i < 2; ++i)
    {
        strcpy(out_payload->scores[i].username, "N/A");
        out_payload->scores[i].score = 0;
    }

    cJSON_Delete(root);
    return 0;
}

// Client Logic Functions

void send_registration_to_server(int sockfd, const char *username)
{
    ClientRegisterPayload reg_payload;
    strcpy(reg_payload.type, "register");
    strncpy(reg_payload.username, username, MAX_USERNAME_LEN - 1);
    reg_payload.username[MAX_USERNAME_LEN - 1] = '\0';

    char *json_string = serialize_client_register(&reg_payload);
    if (json_string == NULL)
    {
        fprintf(stderr, "Error: Could not serialize registration message.\n");
        return;
    }

    if (send(sockfd, json_string, strlen(json_string), 0) < 0 ||
        send(sockfd, "\n", 1, 0) < 0)
    {
        perror("send registration or newline failed");
    }
    else
    {
        printf("Registration message sent for username: %s\n", username);
    }
    free(json_string);
}

void display_board(char board[8][9])
{
    printf("Current Board:\n");
    printf("   1 2 3 4 5 6 7 8\n");
    printf(" +-----------------+\n");
    for (int i = 0; i < 8; ++i)
    {
        printf("%d| ", i + 1);
        for (int j = 0; j < 8; ++j)
        {
            printf("%c ", board[i][j]);
        }
        printf("|\n");
    }
    printf(" +-----------------+\n");
}

void handle_server_message(const char *json_message, int sockfd)
{
    char *msg_type_str = get_message_type_from_json(json_message);
    if (msg_type_str == NULL)
    {
        fprintf(stderr, "Could not determine message type from: %s\n", json_message);
        return;
    }

    printf("Received message of type: %s\n", msg_type_str);

    if (strcmp(msg_type_str, "register_ack") == 0)
    {
        ServerRegisterAckPayload ack_payload;
        if (deserialize_server_register_ack(json_message, &ack_payload) == 0)
        {
            printf("Registration successful. Waiting for game to start...\n");
        }
        else
        {
            fprintf(stderr, "Error deserializing register_ack.\n");
        }
    }
    else if (strcmp(msg_type_str, "register_nack") == 0)
    {
        ServerRegisterNackPayload nack_payload;
        if (deserialize_server_register_nack(json_message, &nack_payload) == 0)
        {
            fprintf(stderr, "Registration failed: %s\n", nack_payload.reason);
            if (matrix_ptr)
                cleanup_matrix(matrix_ptr); // Cleanup matrix
            close(sockfd);
            exit(1);
        }
        else
        {
            fprintf(stderr, "Error deserializing register_nack.\n");
        }
    }
    else if (strcmp(msg_type_str, "game_start") == 0)
    {
        ServerGameStartPayload gs_payload;
        if (deserialize_server_game_start(json_message, &gs_payload) == 0)
        {
            printf("Game started!\n");
            printf("Players: %s, %s\n", gs_payload.players[0], gs_payload.players[1]);
            printf("First player: %s\n", gs_payload.first_player);

            if (strcmp(client_username, gs_payload.first_player) == 0)
            {
                my_player_symbol = 'R';
            }
            else
            {
                my_player_symbol = 'B';
            }
            printf("Client is player %c.\n", my_player_symbol);

            if (strcmp(gs_payload.first_player, client_username) == 0)
            {
                printf("It's your turn first! (Waiting for YOUR_TURN message)\n");
            }
            else
            {
                printf("Waiting for %s to make a move...\n", gs_payload.first_player);
            }
        }
        else
        {
            fprintf(stderr, "Error deserializing game_start.\n");
        }
    }
    else if (strcmp(msg_type_str, "your_turn") == 0)
    {
        ServerYourTurnPayload yt_payload;
        if (deserialize_server_your_turn(json_message, &yt_payload) == 0)
        {
            printf("\nIt's your turn! (Automating move)\n");
            display_board(yt_payload.board);
            if (matrix_ptr)
            {
                render_octaflip_board(matrix_ptr, yt_payload.board);
            }

            if (my_player_symbol == ' ')
            {
                fprintf(stderr, "Error: Player symbol not set. Cannot generate move.\n");
            }

            MoveCoords decided_move = move_generate(yt_payload.board, my_player_symbol);

            ClientMovePayload move_payload_to_send;
            strcpy(move_payload_to_send.type, "move");
            strcpy(move_payload_to_send.username, client_username);

            move_payload_to_send.sx = decided_move.sx;
            move_payload_to_send.sy = decided_move.sy;
            move_payload_to_send.tx = decided_move.tx;
            move_payload_to_send.ty = decided_move.ty;

            char *json_move_string = serialize_client_move(&move_payload_to_send);
            if (json_move_string)
            {
                printf("Client sending move to server: (%d,%d) -> (%d,%d)\n",
                       move_payload_to_send.sx, move_payload_to_send.sy,
                       move_payload_to_send.tx, move_payload_to_send.ty);
                if (send(sockfd, json_move_string, strlen(json_move_string), 0) < 0 ||
                    send(sockfd, "\n", 1, 0) < 0)
                {
                    perror("send move or newline failed");
                }
                free(json_move_string);
            }
            else
            {
                fprintf(stderr, "Error serializing move message.\n");
            }
        }
        else
        {
            fprintf(stderr, "Error deserializing your_turn.\n");
        }
    }
    else if (strcmp(msg_type_str, "move_ok") == 0)
    {
        ServerMoveOkPayload mo_payload;
        if (deserialize_server_move_ok(json_message, &mo_payload) == 0)
        {
            printf("Move accepted.\n");
            display_board(mo_payload.board);
            if (matrix_ptr)
            {
                render_octaflip_board(matrix_ptr, mo_payload.board);
            }
            printf("Next player: %s\n", mo_payload.next_player);
            if (strcmp(mo_payload.next_player, client_username) != 0)
            {
                printf("Waiting for %s to move...\n", mo_payload.next_player);
            }
        }
        else
        {
            fprintf(stderr, "Error deserializing move_ok.\n");
        }
    }
    else if (strcmp(msg_type_str, "invalid_move") == 0)
    {
        ServerInvalidMovePayload im_payload;
        if (deserialize_server_invalid_move(json_message, &im_payload) == 0)
        {
            printf("Move invalid by server.");
            if (im_payload.reason[0] != '\0')
            {
                printf(" Reason: %s\n", im_payload.reason);
            }
            else
            {
                printf("\n");
            }
            display_board(im_payload.board);
            if (matrix_ptr)
            {
                render_octaflip_board(matrix_ptr, im_payload.board);
            }
            printf("Next player: %s. It might be your turn again if server indicates.\n", im_payload.next_player);
            if (strcmp(im_payload.next_player, client_username) != 0)
            {
                printf("Waiting for %s to move...\n", im_payload.next_player);
            }
        }
        else
        {
            fprintf(stderr, "Error deserializing invalid_move.\n");
        }
    }
    else if (strcmp(msg_type_str, "pass") == 0)
    {
        ServerPassPayload pass_payload;
        if (deserialize_server_pass(json_message, &pass_payload) == 0)
        {
            printf("Turn passed by server (e.g. timeout or no valid moves). Next player: %s\n", pass_payload.next_player);
            if (strcmp(pass_payload.next_player, client_username) != 0)
            {
                printf("Waiting for %s to move...\n", pass_payload.next_player);
            }
        }
        else
        {
            fprintf(stderr, "Error deserializing pass.\n");
        }
    }
    else if (strcmp(msg_type_str, "game_over") == 0)
    {
        ServerGameOverPayload go_payload;
        if (deserialize_server_game_over(json_message, &go_payload) == 0)
        {
            printf("\nGame Over!\n");
            printf("Scores:\n");
            printf("  %s: %d\n", go_payload.scores[0].username, go_payload.scores[0].score);
            printf("  %s: %d\n", go_payload.scores[1].username, go_payload.scores[1].score);

            if (go_payload.scores[0].score > go_payload.scores[1].score)
            {
                printf("Winner: %s\n", go_payload.scores[0].username);
            }
            else if (go_payload.scores[1].score > go_payload.scores[0].score)
            {
                printf("Winner: %s\n", go_payload.scores[1].username);
            }
            else
            {
                printf("The game is a Draw.\n");
            }

            if (matrix_ptr)
                cleanup_matrix(matrix_ptr); // Cleanup matrix
            close(sockfd);
            printf("Exiting.\n");
            exit(0);
        }
        else
        {
            fprintf(stderr, "Error deserializing game_over.\n");
            if (matrix_ptr)
                cleanup_matrix(matrix_ptr); // Cleanup matrix
            close(sockfd);
            exit(1);
        }
    }
    else
    {
        fprintf(stderr, "Received unknown or unhandled message type from server: %s\n", msg_type_str);
    }
    free(msg_type_str);
}

// 1. Command-Line Argument Parsing:
int parse_client_args(int argc, char *argv[], char **server_ip_out, char **server_port_out, char **username_out)
{
    *server_ip_out = NULL;
    *server_port_out = NULL;
    *username_out = NULL;

    if (argc < 7)
    {
        fprintf(stderr, "Usage: %s -ip <server_ip> -port <server_port> -username <username>\n", argv[0]);
        return -1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-ip") == 0)
        {
            if (i + 1 < argc)
            {
                *server_ip_out = argv[++i];
            }
            else
            {
                fprintf(stderr, "Error: -ip flag requires an argument.\n");
                goto usage_error;
            }
        }
        else if (strcmp(argv[i], "-port") == 0)
        {
            if (i + 1 < argc)
            {
                *server_port_out = argv[++i];
            }
            else
            {
                fprintf(stderr, "Error: -port flag requires an argument.\n");
                goto usage_error;
            }
        }
        else if (strcmp(argv[i], "-username") == 0)
        {
            if (i + 1 < argc)
            {
                *username_out = argv[++i];
            }
            else
            {
                fprintf(stderr, "Error: -username flag requires an argument.\n");
                goto usage_error;
            }
        }
        else
        {
            fprintf(stderr, "Error: Unknown argument '%s'.\n", argv[i]);
            goto usage_error;
        }
    }

    if (*server_ip_out == NULL || *server_port_out == NULL || *username_out == NULL)
    {
        fprintf(stderr, "Error: Missing one or more required arguments (-ip, -port, -username).\n");
        goto usage_error;
    }

    if (strlen(*username_out) >= MAX_USERNAME_LEN)
    {
        fprintf(stderr, "Error: Username is too long (max %d characters).\n", MAX_USERNAME_LEN - 1);
        return -1;
    }

    return 0;

usage_error:
    fprintf(stderr, "Usage: %s -ip <server_ip> -port <server_port> -username <username>\n", argv[0]);
    return -1;
}

// 2. Server Connection:
int connect_to_server(const char *server_ip, const char *server_port)
{
    struct addrinfo hints, *res;
    int sockfd, status;

    // Set up socket parameters
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Resolve the IP address of the remote server using getaddrinfo
    if ((status = getaddrinfo(server_ip, server_port, &hints, &res)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    // Create a socket object
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1)
    {
        perror("socket error");
        freeaddrinfo(res);
        return -1;
    }

    // Connect to the remote server
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        perror("connect error");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return sockfd;
}

int main(int argc, char *argv[])
{
    char *server_ip;
    char *server_port;
    char *username_from_arg;
    int sockfd;

    // Argument parsing must happen before matrix initialization,
    // as initialize_matrix might use/modify argc, argv.
    if (parse_client_args(argc, argv, &server_ip, &server_port, &username_from_arg) == -1)
    {
        // No matrix to clean up yet if arg parsing fails early
        exit(1);
    }

    // Initialize LED Matrix (modifies argc and argv)
    // Do this after parsing our own args but before using them if they might be consumed by matrix lib
    matrix_ptr = initialize_matrix(&argc, &argv);
    // We can check if matrix_ptr is NULL here if initialization is critical before connection,
    // but errors are handled within initialize_matrix.

    strncpy(client_username, username_from_arg, MAX_USERNAME_LEN - 1);
    client_username[MAX_USERNAME_LEN - 1] = '\0';

    printf("Attempting to connect to server %s on port %s for user %s...\n", server_ip, server_port, client_username);

    sockfd = connect_to_server(server_ip, server_port);
    if (sockfd == -1)
    {
        fprintf(stderr, "Failed to connect to the server.\n");
        if (matrix_ptr)
            cleanup_matrix(matrix_ptr); // Cleanup matrix if connection fails
        exit(1);
    }

    printf("Connected to server. Socket FD: %d\n", sockfd);

    send_registration_to_server(sockfd, client_username);

    fd_set read_fds;

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        int current_max_fd = sockfd;

        int activity = select(current_max_fd + 1, &read_fds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR))
        {
            perror("select error");
            fprintf(stderr, "Disconnected from server. Exiting.\n");
            if (matrix_ptr)
                cleanup_matrix(matrix_ptr); // Cleanup matrix
            close(sockfd);
            exit(1);
        }

        if (FD_ISSET(sockfd, &read_fds))
        {
            char temp_buf[BUFFER_SIZE];
            int bytes_received = recv(sockfd, temp_buf, sizeof(temp_buf) - 1, 0);
            if (bytes_received > 0)
            {
                temp_buf[bytes_received] = '\0';

                if (client_recv_buffer_len + bytes_received >= CLIENT_RECV_BUFFER_MAX_LEN)
                {
                    fprintf(stderr, "Client receive buffer overflow. Discarding data.\n");
                    client_recv_buffer_len = 0;
                }
                else
                {
                    memcpy(client_recv_buffer + client_recv_buffer_len, temp_buf, bytes_received);
                    client_recv_buffer_len += bytes_received;
                    client_recv_buffer[client_recv_buffer_len] = '\0';
                }

                char *current_pos = client_recv_buffer;
                char *newline_ptr;
                while ((newline_ptr = strchr(current_pos, '\n')) != NULL)
                {
                    int message_len = newline_ptr - current_pos;
                    char single_json_message[message_len + 1];
                    strncpy(single_json_message, current_pos, message_len);
                    single_json_message[message_len] = '\0';

                    handle_server_message(single_json_message, sockfd);

                    current_pos = newline_ptr + 1;
                }

                int remaining_len = client_recv_buffer_len - (current_pos - client_recv_buffer);
                if (remaining_len > 0 && current_pos != client_recv_buffer)
                {
                    memmove(client_recv_buffer, current_pos, remaining_len);
                }
                client_recv_buffer_len = remaining_len;
                client_recv_buffer[client_recv_buffer_len] = '\0';
            }
            else if (bytes_received == 0)
            {
                printf("Disconnected from server. Exiting.\n");
                if (matrix_ptr)
                    cleanup_matrix(matrix_ptr); // Cleanup matrix
                close(sockfd);
                exit(1);
            }
            else
            {
                perror("recv error");
                fprintf(stderr, "Disconnected from server. Exiting.\n");
                if (matrix_ptr)
                    cleanup_matrix(matrix_ptr); // Cleanup matrix
                close(sockfd);
                exit(1);
            }
        }
    }

    printf("Closing connection (unexpected exit from loop).\n");
    if (matrix_ptr)
        cleanup_matrix(matrix_ptr); // Cleanup matrix
    close(sockfd);

    return 0;
}
