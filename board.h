#ifndef OCTAFLIP_BOARD_DISPLAY_H
#define OCTAFLIP_BOARD_DISPLAY_H

#include "rpi-rgb-led-matrix/include/led-matrix-c.h" // Main header for the rpi-rgb-led-matrix library

// Define standard OctaFlip board dimensions
#define BOARD_ROWS 8
#define BOARD_COLS 8

// Structure to hold RGB color values
typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGBColor;

// --- Public Function Prototypes ---

/**
 * @brief Initializes the LED matrix for displaying the OctaFlip board.
 *
 * This function should be called once at the beginning of the client application
 * or when board.c is run in standalone mode.
 * It sets up the matrix options (rows, cols, hardware mapping, brightness, etc.)
 * and returns a pointer to the RGBLedMatrix object.
 *
 * @param argc Pointer to main's argc (for matrix library options).
 * @param argv Pointer to main's argv (for matrix library options).
 * @return struct RGBLedMatrix* Pointer to the initialized matrix object, or NULL on failure.
 */
struct RGBLedMatrix *initialize_matrix(int *argc, char ***argv);

/**
 * @brief Renders the given 8x8 OctaFlip board state onto the LED matrix.
 *
 * @param matrix The initialized RGBLedMatrix object.
 * @param octaflip_board A 2D char array representing the 8x8 OctaFlip board
 * (e.g., char board[BOARD_ROWS][BOARD_COLS+1], using 'R', 'B', '.', '#').
 */
void render_octaflip_board(struct RGBLedMatrix *matrix, const char octaflip_board[BOARD_ROWS][BOARD_COLS + 2]);

/**
 * @brief Clears the LED matrix display.
 *
 * @param matrix The initialized RGBLedMatrix object.
 */
void clear_matrix_display(struct RGBLedMatrix *matrix);

/**
 * @brief Cleans up and releases resources used by the LED matrix.
 *
 * This function should be called before the program exits.
 *
 * @param matrix The RGBLedMatrix object to be deleted.
 */
void cleanup_matrix(struct RGBLedMatrix *matrix);

// --- Color Definitions (Examples - customize as you like) ---
// You have creative freedom for patterns and colors [cite: 7, 13]
extern const RGBColor COLOR_RED;
extern const RGBColor COLOR_BLUE;
extern const RGBColor COLOR_EMPTY;
extern const RGBColor COLOR_BLOCKED;
extern const RGBColor COLOR_GRID;
extern const RGBColor COLOR_BACKGROUND;

#endif // OCTAFLIP_BOARD_DISPLAY_H
