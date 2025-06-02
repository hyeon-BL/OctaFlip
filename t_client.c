#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include "cJSON.h"

#define BUF_SIZE 2048

int sock;
char username[32];
char my_color = '?';

typedef struct {
    int sx, sy, tx, ty;
} Move;

int evaluate_board(char grid[9][9], char color) {
    int my = 0, opp = 0;
    char opp_color = (color == 'R') ? 'B' : 'R';
    for (int i = 1; i <= 8; i++) {
        for (int j = 1; j <= 8; j++) {
            if (grid[i][j] == color) my++;
            else if (grid[i][j] == opp_color) opp++;
        }
    }
    return my - opp;
}

void clone_grid(char src[9][9], char dest[9][9]) {
    for (int i = 0; i < 9; i++)
        for (int j = 0; j < 9; j++)
            dest[i][j] = src[i][j];
}

void apply_move(char grid[9][9], int sx, int sy, int tx, int ty, char color) {
    grid[tx][ty] = color;
    if (abs(sx - tx) > 1 || abs(sy - ty) > 1)
        grid[sx][sy] = '.';
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int ni = tx + dy, nj = ty + dx;
            if (ni >= 1 && ni <= 8 && nj >= 1 && nj <= 8 &&
                grid[ni][nj] != '.' && grid[ni][nj] != color)
                grid[ni][nj] = color;
        }
    }
}
int is_valid_move(int sx, int sy, int tx, int ty) {
    int dx = abs(sx - tx);
    int dy = abs(sy - ty);
    if ((dx == 1 && dy <= 1) || (dx == 2 && dy <= 2)) {
        return dx <= 2 && dy <= 2 && (dx == dy || dx == 0 || dy == 0);
    }
    return 0;
}

int negamax(char grid[9][9], char color, int depth, Move *best_move) {
    if (depth == 0) return evaluate_board(grid, color);
    int best_score = -10000;
    char opp_color = (color == 'R') ? 'B' : 'R';

    for (int i = 1; i <= 8; i++) {
        for (int j = 1; j <= 8; j++) {
            if (grid[i][j] != color) continue;
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int ni = i + dy, nj = j + dx;
                    if (ni < 1 || ni > 8 || nj < 1 || nj > 8 || grid[ni][nj] != '.') continue;
                    if (!is_valid_move(i, j, ni, nj)) continue;

                    char new_grid[9][9];
                    clone_grid(grid, new_grid);
                    apply_move(new_grid, i, j, ni, nj, color);

                    int score = -negamax(new_grid, opp_color, depth - 1, NULL);
                    if (score > best_score) {
                        best_score = score;
                        if (best_move) {
                            best_move->sx = i;
                            best_move->sy = j;
                            best_move->tx = ni;
                            best_move->ty = nj;
                        }
                    }
                }
            }
        }
    }
    return best_score;
}


void send_json(cJSON *json) {
    char *text = cJSON_PrintUnformatted(json);
    send(sock, text, strlen(text), 0);
    send(sock, "\n", 1, 0);
    free(text);
}

void register_to_server() {
    cJSON *reg = cJSON_CreateObject();
    cJSON_AddStringToObject(reg, "type", "register");
    cJSON_AddStringToObject(reg, "username", username);
    send_json(reg);
    cJSON_Delete(reg);
}

void handle_register_ack(cJSON *msg) {
    printf("Registered successfully.\n");

    cJSON *color_item = cJSON_GetObjectItem(msg, "color");
    if (color_item && color_item->valuestring) {
        my_color = color_item->valuestring[0];
        printf("You are playing as %c (from server).\n", my_color);
    }
}

void handle_register_nack(cJSON *msg) {
    const char *reason = cJSON_GetObjectItem(msg, "reason")->valuestring;
    printf("Registration failed: %s\n", reason);
}

void handle_game_start(cJSON *msg) {
    const char *first = cJSON_GetObjectItem(msg, "first_player")->valuestring;
    printf("Game started! First player: %s\n", first);

    cJSON *players = cJSON_GetObjectItem(msg, "players");
    if (players) {
        for (int i = 0; i < cJSON_GetArraySize(players); i++) {
            const char *pname = cJSON_GetArrayItem(players, i)->valuestring;
            if (strcmp(pname, username) == 0) {
                my_color = (i == 0) ? 'R' : 'B';
                printf("You are playing as %c.\n", my_color);
            }
        }
    }
}

void move_generate(cJSON *board, int *sx, int *sy, int *tx, int *ty) {
    char grid[9][9] = {0};
    for (int i = 0; i < 8; i++) {
        const char *row = cJSON_GetArrayItem(board, i)->valuestring;
        for (int j = 0; j < 8; j++) {
            grid[i + 1][j + 1] = row[j];
        }
    }

    Move best_move = {0, 0, 0, 0};
    negamax(grid, my_color, 4, &best_move);

    *sx = best_move.sx;
    *sy = best_move.sy;
    *tx = best_move.tx;
    *ty = best_move.ty;
}

void handle_your_turn(cJSON *msg) {
    printf("Your turn!\n");

    cJSON *board = cJSON_GetObjectItem(msg, "board");
    if (board) {
        for (int i = 0; i < cJSON_GetArraySize(board); i++) {
            printf("%s\n", cJSON_GetArrayItem(board, i)->valuestring);
        }
    }

    int sx, sy, tx, ty;
    move_generate(board, &sx, &sy, &tx, &ty);
    printf("Move: (%d,%d) -> (%d,%d)\n", sx, sy, tx, ty);

    cJSON *mv = cJSON_CreateObject();
    cJSON_AddStringToObject(mv, "type", "move");
    cJSON_AddStringToObject(mv, "username", username);
    cJSON_AddNumberToObject(mv, "sx", sx);
    cJSON_AddNumberToObject(mv, "sy", sy);
    cJSON_AddNumberToObject(mv, "tx", tx);
    cJSON_AddNumberToObject(mv, "ty", ty);
    send_json(mv);
    cJSON_Delete(mv);
}

void handle_move_result(cJSON *msg, const char *type) {
    if (strcmp(type, "invalid_move") == 0) {
        printf("[!] Invalid move.\n");
    } else {
        printf("Move OK.\n");
    }

    const char *next = cJSON_GetObjectItem(msg, "next_player")->valuestring;
    printf("Next player: %s\n", next);

    cJSON *board = cJSON_GetObjectItem(msg, "board");
    if (board) {
        for (int i = 0; i < cJSON_GetArraySize(board); i++) {
            printf("%s\n", cJSON_GetArrayItem(board, i)->valuestring);
        }
    }
}

void handle_pass(cJSON *msg) {
    const char *next = cJSON_GetObjectItem(msg, "next_player")->valuestring;
    printf("[PASS] Opponent passed. Next player: %s\n", next);

    cJSON *board = cJSON_GetObjectItem(msg, "board");
    if (board) {
        for (int i = 0; i < cJSON_GetArraySize(board); i++) {
            printf("%s\n", cJSON_GetArrayItem(board, i)->valuestring);
        }
    }
}

void handle_game_over(cJSON *msg) {
    printf("Game over! Scores:\n");
    cJSON *scores = cJSON_GetObjectItem(msg, "scores");
    cJSON *child = scores->child;
    while (child) {
        printf("%s: %d\n", child->string, child->valueint);
        child = child->next;
    }
}

void process_server_message(char *buf) {
    cJSON *msg = cJSON_Parse(buf);
    if (!msg) return;

    const char *type = cJSON_GetObjectItem(msg, "type")->valuestring;
    if (strcmp(type, "register_ack") == 0) {
        handle_register_ack(msg);
    } else if (strcmp(type, "register_nack") == 0) {
        handle_register_nack(msg);
        cJSON_Delete(msg);
        exit(0);
    } else if (strcmp(type, "game_start") == 0) {
        handle_game_start(msg);
    } else if (strcmp(type, "your_turn") == 0) {
        handle_your_turn(msg);
    } else if (strcmp(type, "move_ok") == 0 || strcmp(type, "invalid_move") == 0) {
        handle_move_result(msg, type);
    } else if (strcmp(type, "pass") == 0) {
        handle_pass(msg);
    } else if (strcmp(type, "game_over") == 0) {
        handle_game_over(msg);
        cJSON_Delete(msg);
        exit(0);
    }

    cJSON_Delete(msg);
}

int connect_to_server(const char *ip, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("Socket");
        exit(1);
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    if (connect(s, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect");
        exit(1);
    }
    return s;
}

void recv_loop() {
    char buf[BUF_SIZE] = {0};
    size_t len = 0;

    while (1) {
        int n = recv(sock, buf + len, sizeof(buf) - len - 1, 0);
        if (n <= 0) break;
        len += n;
        buf[len] = '\0';

        char *p;
        while ((p = strchr(buf, '\n'))) {
            *p = '\0';
            process_server_message(buf);

            size_t processed_len = p - buf + 1;
            memmove(buf, p + 1, len - processed_len);
            len -= processed_len;
            buf[len] = '\0';
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 7 || strcmp(argv[1], "-ip") != 0 || strcmp(argv[3], "-port") != 0 || strcmp(argv[5], "-username") != 0) {
        printf("Usage: %s -ip <server_ip> -port <port> -username <name>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[2];
    int port = atoi(argv[4]);
    strncpy(username, argv[6], sizeof(username) - 1);

    sock = connect_to_server(server_ip, port);
    register_to_server();
    recv_loop();

    close(sock);
    return 0;
}
