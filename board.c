#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// RGB color definitions
const RGBColor COLOR_RED = {255, 0, 0};
const RGBColor COLOR_BLUE = {0, 0, 255};
const RGBColor COLOR_EMPTY = {20, 20, 20};
const RGBColor COLOR_BLOCKED = {50, 50, 50};
const RGBColor COLOR_GRID = {100, 100, 100};
const RGBColor COLOR_BACKGROUND = {0, 0, 0};

struct RGBLedMatrix *initialize_matrix(int *argc, char ***argv)
{
    struct RGBLedMatrixOptions matrix_options;
    struct RGBLedMatrix *matrix;

    memset(&matrix_options, 0, sizeof(matrix_options));
    matrix_options.rows = 64;
    matrix_options.cols = 64;
    matrix_options.chain_length = 1;
    matrix_options.disable_hardware_pulsing = true;
    matrix_options.brightness = 50;

    matrix = led_matrix_create_from_options(&matrix_options, argc, argv);

    if (matrix == NULL)
    {
        fprintf(stderr, "Error: Could not initialize LED matrix.\n");
        return NULL;
    }

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

#define MATRIX_SIZE 64
#define CELL_SIZE (MATRIX_SIZE / BOARD_COLS)
#define GRID_LINE_WIDTH 1
#define PIECE_AREA_SIZE (CELL_SIZE - GRID_LINE_WIDTH)

// Helper function to draw a filled rectangle
static void draw_filled_rect(struct LedCanvas *canvas, int x_start, int y_start, int width, int height, RGBColor color)
{
    if (!canvas)
        return;
    for (int y = y_start; y < y_start + height; ++y)
    {
        for (int x = x_start; x < x_start + width; ++x)
        {
            if (x >= 0 && x < MATRIX_SIZE && y >= 0 && y < MATRIX_SIZE)
            {
                led_canvas_set_pixel(canvas, x, y, color.r, color.g, color.b);
            }
        }
    }
}

void render_octaflip_board(struct RGBLedMatrix *matrix, const char octaflip_board[BOARD_ROWS][BOARD_COLS + 1])
{
    if (!matrix)
        return;

    struct LedCanvas *offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);
    if (!offscreen_canvas)
    {
        fprintf(stderr, "Failed to create offscreen canvas.\n");
        return;
    }

    led_canvas_fill(offscreen_canvas, COLOR_BACKGROUND.r, COLOR_BACKGROUND.g, COLOR_BACKGROUND.b);

    // Draw grid lines
    for (int i = 0; i <= BOARD_ROWS; ++i)
    {
        int y_coord = i * CELL_SIZE;
        if (y_coord >= MATRIX_SIZE)
            y_coord = MATRIX_SIZE - GRID_LINE_WIDTH;
        draw_filled_rect(offscreen_canvas, 0, y_coord, MATRIX_SIZE, GRID_LINE_WIDTH, COLOR_GRID);

        int x_coord = i * CELL_SIZE;
        if (x_coord >= MATRIX_SIZE)
            x_coord = MATRIX_SIZE - GRID_LINE_WIDTH;
        draw_filled_rect(offscreen_canvas, x_coord, 0, GRID_LINE_WIDTH, MATRIX_SIZE, COLOR_GRID);
    }

    // Draw pieces
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
                break;
            default:
                piece_color = COLOR_BACKGROUND;
                break;
            }

            int piece_x_start = c * CELL_SIZE + GRID_LINE_WIDTH;
            int piece_y_start = r * CELL_SIZE + GRID_LINE_WIDTH;
            int piece_render_size = CELL_SIZE - (2 * GRID_LINE_WIDTH);
            if (piece_render_size < 1)
                piece_render_size = 1;

            if (piece_char == 'R' || piece_char == 'B' || piece_char == '#')
            {
                draw_filled_rect(offscreen_canvas, piece_x_start, piece_y_start,
                                 piece_render_size, piece_render_size, piece_color);
            }
            else if (piece_char == '.')
            {
                int dot_size = 2;
                int dot_x_start = piece_x_start + (piece_render_size / 2) - (dot_size / 2);
                int dot_y_start = piece_y_start + (piece_render_size / 2) - (dot_size / 2);
                draw_filled_rect(offscreen_canvas, dot_x_start, dot_y_start,
                                 dot_size, dot_size, piece_color);
            }
        }
    }

    struct LedCanvas *previous_front_buffer = led_matrix_swap_on_vsync(matrix, offscreen_canvas);
    if (previous_front_buffer)
    {
        led_canvas_clear(previous_front_buffer);
    }
}

// Conditionally compile the main function for standalone testing
#ifdef STANDALONE_BOARD_TEST
int main(int argc, char *argv[])
{
    char input_board[BOARD_ROWS][BOARD_COLS + 1]; // +1 for null terminator

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
        if (fgets(input_board[i], sizeof(input_board[i]) + 1, stdin) == NULL) // +1 for newline
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