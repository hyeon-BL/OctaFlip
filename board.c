#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For usleep, if needed for any demos

// Define colors (implementation of externs from board.h)
const RGBColor COLOR_RED = {255, 0, 0};
const RGBColor COLOR_BLUE = {0, 0, 255};
const RGBColor COLOR_EMPTY = {20, 20, 20};   // Dim dot or off
const RGBColor COLOR_BLOCKED = {50, 50, 50}; // Different shade for blocked
const RGBColor COLOR_GRID = {100, 100, 100};
const RGBColor COLOR_BACKGROUND = {0, 0, 0};

struct RGBLedMatrix *initialize_matrix(int *argc, char ***argv)
{
    struct RGBLedMatrixOptions matrix_options;
    struct RGBLedMatrix *matrix;

    // Initialize options with defaults
    memset(&matrix_options, 0, sizeof(matrix_options));
    matrix_options.rows = 64; // Targetting a 64x64 panel [cite: 1]
    matrix_options.cols = 64;
    matrix_options.chain_length = 1;
    // matrix_options.hardware_mapping = "adafruit-hat"; // Or your specific hardware
    // matrix_options.gpio_slowdown = 2; // Adjust as needed
    // matrix_options.brightness = 50; // 0-100

    // Parse command-line options for the matrix library.
    // The library has its own command-line option parsing.
    // This allows overriding defaults via arguments if client.c passes them through.
    matrix = led_matrix_create_from_options(&matrix_options, argc, argv);

    if (matrix == NULL)
    {
        fprintf(stderr, "Error: Could not initialize LED matrix. "
                        "Did you run with sudo? Is the hardware configured?\n");
        return NULL;
    }

    // Clear matrix on startup
    clear_matrix_display(matrix);

    return matrix;
}

void clear_matrix_display(struct RGBLedMatrix *matrix)
{
    if (!matrix)
        return;
    led_matrix_fill(matrix, COLOR_BACKGROUND.r, COLOR_BACKGROUND.g, COLOR_BACKGROUND.b);
    // If using a canvas/swap chain model with the library:
    // struct LedCanvas *canvas = led_matrix_get_canvas(matrix);
    // led_canvas_fill(canvas, COLOR_BACKGROUND.r, COLOR_BACKGROUND.g, COLOR_BACKGROUND.b);
    // led_matrix_swap(matrix, canvas); // or similar function to refresh display
}

void cleanup_matrix(struct RGBLedMatrix *matrix)
{
    if (!matrix)
        return;
    clear_matrix_display(matrix); // Optional: clear before deleting
    led_matrix_delete(matrix);
}

// --- Helper function to draw a single pixel (if needed directly) ---
// static void set_pixel(struct RGBLedMatrix* matrix, int x, int y, RGBColor color) {
//     if (!matrix || x < 0 || x >= 64 || y < 0 || y >= 64) return;
//     led_matrix_set_pixel(matrix, x, y, color.r, color.g, color.b);
// }

// Define cell and piece dimensions based on a 64x64 matrix for an 8x8 board
#define MATRIX_SIZE 64
#define CELL_SIZE (MATRIX_SIZE / BOARD_COLS)          // Should be 8x8 pixels per cell [cite: 10]
#define GRID_LINE_WIDTH 1                             // 1-pixel wide lines [cite: 8]
#define PIECE_AREA_SIZE (CELL_SIZE - GRID_LINE_WIDTH) // Area for piece within cell borders
                                                      // If lines are on the boundary, piece area is (CELL_SIZE - 2*GRID_LINE_WIDTH)
                                                      // PDF: "outermost pixel ... for grid lines, leaving a 6x6-pixel area" [cite: 11]
                                                      // This implies if CELL_SIZE is 8, and 1 pixel border on each side, piece is 6x6.

// Helper function to draw a filled rectangle (you might need to implement this or use library features)
static void draw_filled_rect(struct RGBLedMatrix *matrix, int x_start, int y_start, int width, int height, RGBColor color)
{
    if (!matrix)
        return;
    for (int y = y_start; y < y_start + height; ++y)
    {
        for (int x = x_start; x < x_start + width; ++x)
        {
            if (x >= 0 && x < MATRIX_SIZE && y >= 0 && y < MATRIX_SIZE)
            {
                led_matrix_set_pixel(matrix, x, y, color.r, color.g, color.b);
            }
        }
    }
}

void render_octaflip_board(struct RGBLedMatrix *matrix, const char octaflip_board[BOARD_ROWS][BOARD_COLS + 1])
{
    if (!matrix)
        return;

    // Get a canvas for drawing (more efficient if doing many pixel operations)
    // struct LedCanvas *offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);
    // if (!offscreen_canvas) {
    //     fprintf(stderr, "Failed to create offscreen canvas.\n");
    //     return;
    // }
    // For simplicity with the provided library functions, we'll draw directly to the matrix.
    // If performance becomes an issue, using an offscreen canvas and swapping is preferred.

    // 1. Clear the matrix to background color
    led_matrix_fill(matrix, COLOR_BACKGROUND.r, COLOR_BACKGROUND.g, COLOR_BACKGROUND.b);

    // 2. Draw Grid Lines [cite: 8, 9, 10, 11]
    // Grid lines are at the boundaries of the cells.
    // For an 8x8 board on a 64x64 matrix, CELL_SIZE is 8.
    // Lines will be at 0, 8, 16, 24, 32, 40, 48, 56, and the final boundary at 63 (or effectively MATRIX_SIZE-1).
    for (int i = 0; i <= BOARD_ROWS; ++i)
    { // 9 lines for 8 cells (0 to 8)
        int y_coord = i * CELL_SIZE;
        if (y_coord >= MATRIX_SIZE)
            y_coord = MATRIX_SIZE - GRID_LINE_WIDTH;                                    // Clamp to ensure line is visible
        draw_filled_rect(matrix, 0, y_coord, MATRIX_SIZE, GRID_LINE_WIDTH, COLOR_GRID); // Horizontal line

        int x_coord = i * CELL_SIZE;
        if (x_coord >= MATRIX_SIZE)
            x_coord = MATRIX_SIZE - GRID_LINE_WIDTH;                                    // Clamp to ensure line is visible
        draw_filled_rect(matrix, x_coord, 0, GRID_LINE_WIDTH, MATRIX_SIZE, COLOR_GRID); // Vertical line
    }

    // 3. Draw Pieces in Cells
    for (int r = 0; r < BOARD_ROWS; ++r)
    {
        for (int c = 0; c < BOARD_COLS; ++c)
        {
            char piece_char = octaflip_board[r][c];
            RGBColor piece_color;

            switch (piece_char)
            {
            case 'R':
                piece_color = COLOR_RED;
                break;
            case 'B':
                piece_color = COLOR_BLUE;
                break;
            case '.':
                piece_color = COLOR_EMPTY;
                break;
            case '#':
                piece_color = COLOR_BLOCKED;
                break; // Assuming '#' is for blocked cells
            default:
                piece_color = COLOR_BACKGROUND;
                break; // Should not happen
            }

            // Calculate top-left corner of the piece area within the cell [cite: 11, 12]
            // Cell top-left: (c * CELL_SIZE, r * CELL_SIZE)
            // Piece area top-left (inside grid lines):
            int piece_x_start = c * CELL_SIZE + GRID_LINE_WIDTH;
            int piece_y_start = r * CELL_SIZE + GRID_LINE_WIDTH;

            // Size of the piece rendering area (6x6 for an 8x8 cell with 1px border on each side)
            int piece_render_size = CELL_SIZE - (2 * GRID_LINE_WIDTH);
            if (piece_render_size < 1)
                piece_render_size = 1; // Ensure at least 1x1 pixel

            if (piece_char == 'R' || piece_char == 'B')
            {
                // Draw a solid block for R and B [cite: 12, 13]
                draw_filled_rect(matrix, piece_x_start, piece_y_start, piece_render_size, piece_render_size, piece_color);
            }
            else if (piece_char == '.')
            {
                // For empty cells, draw a smaller dot or a different pattern.
                // Example: a 2x2 dot in the center of the 6x6 area.
                int dot_size = 2; // Or 1 for a single pixel
                int dot_x_start = piece_x_start + (piece_render_size / 2) - (dot_size / 2);
                int dot_y_start = piece_y_start + (piece_render_size / 2) - (dot_size / 2);
                draw_filled_rect(matrix, dot_x_start, dot_y_start, dot_size, dot_size, piece_color);
            }
            else if (piece_char == '#')
            {
                // For blocked cells, draw a solid block or a distinct pattern.
                draw_filled_rect(matrix, piece_x_start, piece_y_start, piece_render_size, piece_render_size, piece_color);
            }
            // If piece_char is something else or COLOR_BACKGROUND, it will effectively be the background
            // color of the piece area, as the main background is already drawn.
        }
    }

    // If using an offscreen canvas that needs to be swapped to the physical matrix:
    // led_matrix_swap_on_vsync(matrix, offscreen_canvas); // or led_matrix_swap()
    // led_canvas_delete(offscreen_canvas);
    // If drawing directly, the changes should be visible.
    // Some libraries might require an explicit "update" or "refresh" call if they buffer internally
    // even without an explicit offscreen canvas object. The rpi-rgb-led-matrix library
    // often updates directly when led_matrix_set_pixel is called on the matrix pointer.
}

// (Standalone main function will be added in Stage L4)
