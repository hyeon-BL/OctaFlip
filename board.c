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

// (Standalone main function will be added in Stage L4)
// (render_octaflip_board function will be added in Stage L3)
