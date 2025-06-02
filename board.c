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

    // Create an offscreen canvas to draw the cleared state
    struct LedCanvas *canvas = led_matrix_create_offscreen_canvas(matrix);
    if (!canvas)
    {
        fprintf(stderr, "Error: Could not create offscreen canvas in clear_matrix_display.\n");
        return;
    }

    led_canvas_fill(canvas, COLOR_BACKGROUND.r, COLOR_BACKGROUND.g, COLOR_BACKGROUND.b);

    // Swap the cleared canvas to the display.
    // led_matrix_swap_on_vsync takes ownership of 'canvas' and returns the PREVIOUS front buffer.
    // This previous buffer should be deleted if not reused.
    struct LedCanvas *previous_front_buffer = led_matrix_swap_on_vsync(matrix, canvas);
    if (previous_front_buffer)
    { // It might be NULL if this is the very first swap
        led_canvas_clear(previous_front_buffer);
    }
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
//     led_canvas_set_pixel(matrix, x, y, color.r, color.g, color.b); // This would also need to change if used
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
static void draw_filled_rect(struct LedCanvas *canvas, int x_start, int y_start, int width, int height, RGBColor color)
{
    if (!canvas)
        return;
    for (int y = y_start; y < y_start + height; ++y)
    {
        for (int x = x_start; x < x_start + width; ++x)
        {
            if (x >= 0 && x < MATRIX_SIZE && y >= 0 && y < MATRIX_SIZE) // Assuming MATRIX_SIZE is canvas width/height
            {
                led_canvas_set_pixel(canvas, x, y, color.r, color.g, color.b);
            }
        }
    }
}

void render_octaflip_board(struct RGBLedMatrix *matrix, const char octaflip_board[BOARD_ROWS][BOARD_COLS + 2])
{
    if (!matrix)
        return;

    // Get an offscreen canvas for drawing
    struct LedCanvas *offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);
    if (!offscreen_canvas)
    {
        fprintf(stderr, "Failed to create offscreen canvas.\n");
        return;
    }

    // 1. Clear the offscreen canvas to background color
    led_canvas_fill(offscreen_canvas, COLOR_BACKGROUND.r, COLOR_BACKGROUND.g, COLOR_BACKGROUND.b);

    // 2. Draw Grid Lines [cite: 8, 9, 10, 11]
    // Grid lines are at the boundaries of the cells.
    // For an 8x8 board on a 64x64 matrix, CELL_SIZE is 8.
    // Lines will be at 0, 8, 16, 24, 32, 40, 48, 56, and the final boundary at 63 (or effectively MATRIX_SIZE-1).
    for (int i = 0; i <= BOARD_ROWS; ++i)
    { // 9 lines for 8 cells (0 to 8)
        int y_coord = i * CELL_SIZE;
        if (y_coord >= MATRIX_SIZE)
            y_coord = MATRIX_SIZE - GRID_LINE_WIDTH;                                              // Clamp to ensure line is visible
        draw_filled_rect(offscreen_canvas, 0, y_coord, MATRIX_SIZE, GRID_LINE_WIDTH, COLOR_GRID); // Horizontal line

        int x_coord = i * CELL_SIZE;
        if (x_coord >= MATRIX_SIZE)
            x_coord = MATRIX_SIZE - GRID_LINE_WIDTH;                                              // Clamp to ensure line is visible
        draw_filled_rect(offscreen_canvas, x_coord, 0, GRID_LINE_WIDTH, MATRIX_SIZE, COLOR_GRID); // Vertical line
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
                draw_filled_rect(offscreen_canvas, piece_x_start, piece_y_start, piece_render_size, piece_render_size, piece_color);
            }
            else if (piece_char == '.')
            {
                // For empty cells, draw a smaller dot or a different pattern.
                // Example: a 2x2 dot in the center of the 6x6 area.
                int dot_size = 2; // Or 1 for a single pixel
                int dot_x_start = piece_x_start + (piece_render_size / 2) - (dot_size / 2);
                int dot_y_start = piece_y_start + (piece_render_size / 2) - (dot_size / 2);
                draw_filled_rect(offscreen_canvas, dot_x_start, dot_y_start, dot_size, dot_size, piece_color);
            }
            else if (piece_char == '#')
            {
                // For blocked cells, draw a solid block or a distinct pattern.
                draw_filled_rect(offscreen_canvas, piece_x_start, piece_y_start, piece_render_size, piece_render_size, piece_color);
            }
            // If piece_char is something else or COLOR_BACKGROUND, it will effectively be the background
            // color of the piece area, as the main background is already drawn.
        }
    }

    // Swap the drawn offscreen_canvas to the display.
    // led_matrix_swap_on_vsync returns the canvas that was previously on the screen.
    // For a one-shot render like this, we should delete that previous canvas.
    struct LedCanvas *previous_front_buffer = led_matrix_swap_on_vsync(matrix, offscreen_canvas);
    if (previous_front_buffer)
    {
        led_canvas_clear(previous_front_buffer);
    }
    // Note: 'offscreen_canvas' is now the current front buffer on the matrix.
    // It will be cleaned up when led_matrix_delete() is called on the matrix.
    // If this function were part of a continuous rendering loop, the returned
    // 'previous_front_buffer' would become the new 'offscreen_canvas' for the next frame.
}

// (Standalone main function will be added in Stage L4)

// Conditionally compile the main function for standalone testing
#ifdef STANDALONE_BOARD_TEST
int main(int argc, char *argv[])
{
    char input_board[BOARD_ROWS][BOARD_COLS + 2]; // +1 for null terminator, +1 for newline

    // 1. Initialize the LED Matrix
    //    Pass argc and argv by address for the library to parse its options
    struct RGBLedMatrix *matrix = initialize_matrix(&argc, &argv);
    if (matrix == NULL)
    {
        return 1; // Initialization failed
    }

    // 2. Read 8x8 board from stdin [cite: 24]
    //    Assignment 1 format: 8 lines of 8 characters each [cite: 24]
    printf("Enter 8x8 board configuration (8 lines, 8 chars each, e.g., R B . #):\n");
    for (int i = 0; i < BOARD_ROWS; ++i)
    {
        if (fgets(input_board[i], sizeof(input_board[i]), stdin) == NULL)
        {
            fprintf(stderr, "Error reading board input line %d.\n", i + 1);
            cleanup_matrix(matrix);
            return 1;
        }
        // Remove newline character if present and ensure null termination at BOARD_COLS
        input_board[i][strcspn(input_board[i], "\n")] = '\0';
        if (strlen(input_board[i]) != BOARD_COLS)
        {
            fprintf(stderr, "Error: Line %d does not have %d characters. Found %zu.\n", i + 1, BOARD_COLS, strlen(input_board[i]));
            // Optionally, pad or truncate, or exit. For now, we'll proceed but ensure correct termination.
        }
        input_board[i][BOARD_COLS] = '\0'; // Ensure null termination at the correct spot
    }

    // 3. Render the board
    printf("Rendering board...\n");
    render_octaflip_board(matrix, input_board);

    // 4. Keep display on for a bit (e.g., 10 seconds) or until key press
    printf("Displaying for 10 seconds. Press Ctrl+C to exit earlier.\n");
    sleep(10); // Keep the display active

    // 5. Cleanup
    printf("Cleaning up matrix...\n");
    cleanup_matrix(matrix);

    return 0;
}
#endif // STANDALONE_BOARD_TEST

// To compile for standalone test (example):
// gcc board.c -o standalone_board_test -DSTANDALONE_BOARD_TEST $(pkg-config --cflags --libs rgbmatrix)
// (Adjust linker flags for rpi-rgb-led-matrix library as per its documentation, e.g., -lrgbmatrix)