#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Board dimensions
#define BOARD_ROWS 8
#define BOARD_COLS 8

// Player symbols and cell states
#define PLAYER_R 'R'
#define PLAYER_B 'B'
#define EMPTY_CELL '.'
#define BLOCKED_CELL '#'

// Function to initialize the game board
void initializeBoard(char board[BOARD_ROWS][BOARD_COLS], char initial_board_input[BOARD_ROWS][BOARD_COLS + 1])
{
    for (int i = 0; i < BOARD_ROWS; i++)
    {
        if (strlen(initial_board_input[i]) != BOARD_COLS)
        {
            printf("Board input error: Row %d length is incorrect.\n", i + 1);
            exit(1); // Exit if row length is incorrect
        }
        for (int j = 0; j < BOARD_COLS; j++)
        {
            char cell = initial_board_input[i][j];
            if (cell == PLAYER_R || cell == PLAYER_B || cell == EMPTY_CELL || cell == BLOCKED_CELL)
            {
                board[i][j] = cell;
            }
            else
            {
                printf("Board input error: Invalid character '%c' at (%d, %d).\n", cell, i + 1, j + 1);
                exit(1); // Exit if invalid character is found
            }
        }
    }
}

// Function to print the game board
void printBoard(char board[BOARD_ROWS][BOARD_COLS])
{
    for (int i = 0; i < BOARD_ROWS; i++)
    {
        for (int j = 0; j < BOARD_COLS; j++)
        {
            printf("%c", board[i][j]);
        }
        printf("\n");
    }
}

// Function to check if coordinates are within board limits
int isWithinBounds(int r, int c)
{
    return (r >= 0 && r < BOARD_ROWS && c >= 0 && c < BOARD_COLS);
}

// Function to validate if the source cell contains the current player's piece
int isValidSource(char board[BOARD_ROWS][BOARD_COLS], int r, int c, char currentPlayer)
{
    if (!isWithinBounds(r, c))
    {
        return 0; // Not within bounds
    }
    return (board[r][c] == currentPlayer); // CurrentPlayer's piece or other
}

// Function to validate if the destination cell is empty
int isDestinationEmpty(char board[BOARD_ROWS][BOARD_COLS], int r, int c)
{
    if (!isWithinBounds(r, c))
    {
        return 0; // Not within bounds
    }
    return (board[r][c] == EMPTY_CELL); // Empty cell or not
}

// Function to determine move type based on distance
int getMoveDistance(int r1, int c1, int r2, int c2)
{
    int dr_abs = abs(r1 - r2);
    int dc_abs = abs(c1 - c2);

    // dr_abs == 0 (horizontal), or dc_abs == 0 (vertical), or dr_abs == dc_abs (diagonal).
    if (!(dr_abs == 0 || dc_abs == 0 || dr_abs == dc_abs))
    {
        return 0; // Not a straight or diagonal line (knight's move or other invalid move)
    }

    int distance = dr_abs + dc_abs;

    if (distance == 1 || (dr_abs == 1 && dc_abs == 1))
    {
        return 1; // Clone move
    }
    else if (distance == 2 || (dr_abs == 2 && dc_abs == 2))
    {
        return 2; // Jump move
    }
    else
    {
        return 0; // Invalid distance (0, or >2)
    }
}

// Function to perform a clone move
void performClone(char board[BOARD_ROWS][BOARD_COLS], int r1, int c1, int r2, int c2, char currentPlayer)
{
    // Source: (r1, c1), Destination: (r2, c2)
    // Assumes move is a valid (1 cell away, destination empty, source has player's piece).
    board[r2][c2] = currentPlayer;
    // No change to board[r1][c1]
}

// Function to perform a jump move
void performJump(char board[BOARD_ROWS][BOARD_COLS], int r1, int c1, int r2, int c2, char currentPlayer)
{
    // Source: (r1, c1), Destination: (r2, c2)
    // Assumes move is a valid (2 cells away, destination empty, source has player's piece).
    board[r2][c2] = currentPlayer;
    board[r1][c1] = EMPTY_CELL;
}

// Function to flip opponent pieces surrounding a newly placed piece
void flipOpponentPieces(char board[BOARD_ROWS][BOARD_COLS], int r_dest, int c_dest, char currentPlayer)
{
    char opponentPlayer = (currentPlayer == PLAYER_R) ? PLAYER_B : PLAYER_R;

    // Iterate through all 8 adjacent cells
    for (int dr = -1; dr <= 1; dr++)
    {
        for (int dc = -1; dc <= 1; dc++)
        {
            // Skip (r_dest, c_dest)
            if (dr == 0 && dc == 0)
            {
                continue;
            }

            int adj_r = r_dest + dr;
            int adj_c = c_dest + dc;

            // Check if the adjacent cell is within board bounds
            if (isWithinBounds(adj_r, adj_c))
            {
                // If it contains an opponent's piece, change it to currentPlayer's piece
                if (board[adj_r][adj_c] == opponentPlayer)
                {
                    board[adj_r][adj_c] = currentPlayer;
                }
            }
        }
    }
}

// Function to switch the current player
char switchPlayer(char currentPlayer)
{
    if (currentPlayer == PLAYER_R)
    {
        return PLAYER_B;
    }
    else
    {
        return PLAYER_R;
    }
}

// Function to process a single move
int processMove(char board[BOARD_ROWS][BOARD_COLS], int r1, int c1, int r2, int c2, char currentPlayer)
{
    // Validate source and destination coordinates are within bounds
    if (!isWithinBounds(r1, c1) || !isWithinBounds(r2, c2))
    {
        return 0; // Invalid move: out of bounds
    }

    // Validate source piece
    if (!isValidSource(board, r1, c1, currentPlayer))
    {
        return 0;
    }

    // Validate destination cell is empty
    if (!isDestinationEmpty(board, r2, c2))
    {
        return 0;
    }

    // Check for blocked cell at destination
    if (board[r2][c2] == BLOCKED_CELL)
    {
        return 0;
    }

    // Determine move type
    int moveType = getMoveDistance(r1, c1, r2, c2);

    if (moveType == 1) // Clone move
    {                  // Verified: 1 cell away, destination empty, source has player's piece
        performClone(board, r1, c1, r2, c2, currentPlayer);
    }
    else if (moveType == 2) // Jump move
    {                       // Verified: 2 cells away, destination empty, source has player's piece
        performJump(board, r1, c1, r2, c2, currentPlayer);
    }
    else
    { // Invalid move type
        return 0;
    }

    // Flip opponent pieces
    flipOpponentPieces(board, r2, c2, currentPlayer);

    return 1; // Move successfully processed
}

// Function to check if the current player has any valid moves
int hasValidMoves(char board[BOARD_ROWS][BOARD_COLS], char currentPlayer)
{
    int dr_options[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int dc_options[] = {-1, 0, 1, -1, 1, -1, 0, 1};

    for (int r_src = 0; r_src < BOARD_ROWS; r_src++)
    {
        for (int c_src = 0; c_src < BOARD_COLS; c_src++)
        {
            if (board[r_src][c_src] == currentPlayer)
            {
                // Check for possible clone moves (1 step away)
                for (int i = 0; i < 8; i++)
                {
                    int r_dest = r_src + dr_options[i];
                    int c_dest = c_src + dc_options[i];

                    if (isWithinBounds(r_dest, c_dest) && isDestinationEmpty(board, r_dest, c_dest))
                    {
                        return 1; // Found a valid clone move
                    }
                }

                // Check for possible jump moves (2 steps away)
                for (int i = 0; i < 8; i++)
                {
                    int r_dest = r_src + 2 * dr_options[i];
                    int c_dest = c_src + 2 * dc_options[i];

                    if (isWithinBounds(r_dest, c_dest) && isDestinationEmpty(board, r_dest, c_dest))
                    {
                        return 1; // Found a valid jump move
                    }
                }
            }
        }
    }
    return 0; // No valid moves found
}

// Function to check if there are no empty cells left on the board
int noEmptyCellsLeft(char board[BOARD_ROWS][BOARD_COLS])
{
    for (int i = 0; i < BOARD_ROWS; i++)
    {
        for (int j = 0; j < BOARD_COLS; j++)
        {
            if (board[i][j] == EMPTY_CELL)
            {
                return 0; // Found an empty cell
            }
        }
    }
    return 1; // No empty cells found
}

// Function to count the number of pieces for a given player
int countPlayerPieces(char board[BOARD_ROWS][BOARD_COLS], char playerSymbol)
{
    int count = 0;
    for (int i = 0; i < BOARD_ROWS; i++)
    {
        for (int j = 0; j < BOARD_COLS; j++)
        {
            if (board[i][j] == playerSymbol)
            {
                count++;
            }
        }
    }
    return count;
}

// Function to check if a player has no pieces left
int playerHasNoPieces(char board[BOARD_ROWS][BOARD_COLS], char playerSymbol)
{
    return (countPlayerPieces(board, playerSymbol) == 0);
}

// Function to check game termination conditions
int checkGameTermination(char board[BOARD_ROWS][BOARD_COLS], int consecutivePasses)
{
    // Condition 1: No empty cells left
    if (noEmptyCellsLeft(board))
    {
        return 1; // Game over
    }

    // Condition 2: One player has no pieces left
    if (playerHasNoPieces(board, PLAYER_R) || playerHasNoPieces(board, PLAYER_B))
    {
        return 1; // Game over
    }

    // Condition 3: Two consecutive passes
    if (consecutivePasses >= 2)
    {
        return 1; // Game over
    }

    return 0; // Game continues
}

// Function to determine the winner and print the result
void determineAndPrintWinner(char board[BOARD_ROWS][BOARD_COLS])
{
    int redPieces = countPlayerPieces(board, PLAYER_R);
    int bluePieces = countPlayerPieces(board, PLAYER_B);

    if (redPieces > bluePieces)
    {
        printf("Red\n");
    }
    else if (bluePieces > redPieces)
    {
        printf("Blue\n");
    }
    else
    {
        printf("Draw\n");
    }
}

// Function to print an invalid move message
void printInvalidMove(int turnNumber)
{
    printf("Invalid move at turn %d\n", turnNumber);
}

// Function to clear the input buffer
void clearInputBuffer()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

int main()
{
    char gameBoard[BOARD_ROWS][BOARD_COLS];
    char initial_board_input[BOARD_ROWS][BOARD_COLS + 1];
    char buffer[100];

    // 1. Read 8 lines for the initial board state
    for (int i = 0; i < BOARD_ROWS; i++)
    {
        scanf("%s", initial_board_input[i]);
    }
    initializeBoard(gameBoard, initial_board_input);

    // 2. Read the integer N (number of moves)
    int N;
    // Check for valid input
    clearInputBuffer();
    if (fgets(buffer, sizeof(buffer), stdin))
    {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n')
        {
            buffer[len - 1] = '\0'; // Remove newline character
        }

        for (int i = 0; buffer[i] != '\0'; i++)
        {
            if (buffer[i] < '0' || buffer[i] > '9')
            {
                fprintf(stderr, "Invalid input at turn 0\n");
                return 1; // Invalid input
            }
        }
        N = atoi(buffer);
        if (N < 0 || strlen(buffer) > 10 || (strlen(buffer) == 10 && strcmp(buffer, "2147483647") > 0))
        {
            fprintf(stderr, "Invalid input at turn 0\n");
            return 1; // Invalid input
        }
    }
    else
    {
        fprintf(stderr, "Invalid input at turn 0\n");
        return 1;
    }

    char currentPlayer = PLAYER_R;
    int consecutivePasses = 0;
    int invalidMoveOccurred = 0; // Flag to track if an invalid move has occurred

    // Main game loop
    for (int turn = 1; turn <= N; ++turn)
    {
        int r1, c1, r2, c2;

        if (fgets(buffer, sizeof(buffer), stdin))
        {
            size_t len = strlen(buffer);
            int result = sscanf(buffer, "%d %d %d %d", &r1, &c1, &r2, &c2);
            if (result != 4 || r1 < 0 || c1 < 0 || r2 < 0 || c2 < 0)
            {
                fprintf(stderr, "Invalid input at turn %d\n", turn);
                return 1;
            }
        }

        int isPassMove = (r1 == 0 && c1 == 0 && r2 == 0 && c2 == 0);

        // Convert to 0-based indexing (if not a pass move)
        int r1_0based = r1, c1_0based = c1, r2_0based = r2, c2_0based = c2;
        if (!isPassMove)
        {
            r1_0based--;
            c1_0based--;
            r2_0based--;
            c2_0based--;
        }

        if (!hasValidMoves(gameBoard, currentPlayer))
        {
            // Player has no valid moves, Player must pass
            if (!isPassMove)
            {
                // Player had no moves but attempted a non-pass move.
                printInvalidMove(turn);
                invalidMoveOccurred = 1;
                break; // Terminate game
            }
            consecutivePasses++;
        }
        else
        {
            // Player has valid moves.
            if (isPassMove)
            {
                // Player cannot pass if they have valid moves.
                printInvalidMove(turn);
                invalidMoveOccurred = 1;
                break; // Terminate game
            }
            else
            {
                // Player attempts a regular move using 0-based coordinates.
                if (!processMove(gameBoard, r1_0based, c1_0based, r2_0based, c2_0based, currentPlayer))
                {
                    printInvalidMove(turn);
                    invalidMoveOccurred = 1;
                    break; // Terminate game
                }
                // Successful move
                consecutivePasses = 0;
            }
        }

        // Check for game termination after a valid action (move or pass)
        if (checkGameTermination(gameBoard, consecutivePasses))
        {
            break; // Game ends, move to post-loop output
        }

        currentPlayer = switchPlayer(currentPlayer);
    }

    // Post-loop output
    if (!invalidMoveOccurred)
    {
        printBoard(gameBoard);
        determineAndPrintWinner(gameBoard);
    }

    // If invalidMoveOccurred is true, the message has already been printed.
    return 0;
}