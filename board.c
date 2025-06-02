// main.c
#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "led-matrix-c.h"

const RGBColor COLOR_RED     = {255,   0,   0};
const RGBColor COLOR_BLUE    = {  0,   0, 255};
const RGBColor COLOR_EMPTY   = { 20,  20,  20};
const RGBColor COLOR_BLOCKED = { 50,  50,  50};
const RGBColor COLOR_GRID    = {100, 100, 100};
const RGBColor COLOR_BG      = {  0,   0,   0};

#define MATRIX_SIZE    64
#define CELL_SIZE      (MATRIX_SIZE / BOARD_COLS)  // 8
#define GRID_LINE_WIDTH 1

static void draw_filled_rect(struct LedCanvas *canvas,
                             int x_start, int y_start,
                             int width, int height,
                             RGBColor color)
{
    if (!canvas) return;
    for (int y = y_start; y < y_start + height; ++y) {
        for (int x = x_start; x < x_start + width; ++x) {
            if (x >= 0 && x < MATRIX_SIZE && y >= 0 && y < MATRIX_SIZE) {
                led_canvas_set_pixel(canvas, x, y,
                                     color.r, color.g, color.b);
            }
        }
    }
}

void cleanup_matrix(struct RGBLedMatrix *matrix)
{
    if (!matrix)
        return;
    clear_matrix_display(matrix); // Optional: clear before deleting
    led_matrix_delete(matrix);
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


void render_octaflip_board_with_text(
    struct RGBLedMatrix *matrix,
    struct LedFont      *font,
    const char           octaflip_board[BOARD_ROWS][BOARD_COLS + 1])
{
    if (!matrix || !font) return;

    struct LedCanvas *offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);
    if (!offscreen_canvas) {
        fprintf(stderr, "Failed to create offscreen canvas.\n");
        return;
    }

    // 1. 배경 초기화
    led_canvas_fill(offscreen_canvas, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b);

    // 2. 그리드(회색 선) 그리기
    for (int i = 0; i <= BOARD_ROWS; ++i) {
        int line_pos = i * CELL_SIZE;
        if (line_pos >= MATRIX_SIZE) line_pos = MATRIX_SIZE - GRID_LINE_WIDTH;
        // 가로 선
        draw_filled_rect(offscreen_canvas,
                         0, line_pos, MATRIX_SIZE, GRID_LINE_WIDTH,
                         COLOR_GRID);
        // 세로 선
        draw_filled_rect(offscreen_canvas,
                         line_pos, 0, GRID_LINE_WIDTH, MATRIX_SIZE,
                         COLOR_GRID);
    }

    // 3. 폰트 높이/베이스라인 정보
    int font_height   = height_font(font);
    int font_baseline = baseline_font(font);

    // 4. 각 셀마다 문자 출력
    for (int r = 0; r < BOARD_ROWS; ++r) {
        for (int c = 0; c < BOARD_COLS; ++c) {
            char piece_char = octaflip_board[r][c];
            if (piece_char != 'R' &&
                piece_char != 'B' &&
                piece_char != '.' &&
                piece_char != '#')
            {
                continue; // 알 수 없는 기호면 건너뜀
            }

            // 셀 내부 좌표
            int cell_x0 = c * CELL_SIZE + GRID_LINE_WIDTH;
            int cell_y0 = r * CELL_SIZE + GRID_LINE_WIDTH;
            int cell_inner = CELL_SIZE - 2 * GRID_LINE_WIDTH; // 6×6

            // 문자별 색상
            RGBColor color;
            switch (piece_char) {
                case 'R':  color = COLOR_RED;     break;
                case 'B':  color = COLOR_BLUE;    break;
                case '.':  color = COLOR_EMPTY;   break;
                case '#':  color = COLOR_BLOCKED; break;
                default:   color = COLOR_BG;      break;
            }
            char txt[2] = {piece_char, '\0'};

            // 가로 폭을 대략 5픽셀이라고 가정 (폰트마다 차이 있으므로,
            // 실제로는 draw_text 호출 후 반환값으로 폭을 알 수 있음)
            int char_w = 5;
            int char_h = font_height; // 예: 6

            // 텍스트 좌표 계산 (셀 중앙)
            int text_x = cell_x0 + (cell_inner - char_w) / 2;
            int text_y = cell_y0 + (cell_inner - char_h) / 2 + font_baseline;

            draw_text(offscreen_canvas,
                      font,
                      text_x, text_y,
                      color.r, color.g, color.b,
                      txt,
                      0);
        }
    }

    // 5. 화면으로 스왑
    struct LedCanvas *prev = led_matrix_swap_on_vsync(matrix, offscreen_canvas);
    if (prev) {
        led_canvas_clear(prev);
    }
}

int main(int argc, char *argv[])
{
    // 1) LED 매트릭스 초기화
    struct RGBLedMatrixOptions matrix_options;
    memset(&matrix_options, 0, sizeof(matrix_options));
    matrix_options.rows = 64;
    matrix_options.cols = 64;
    matrix_options.chain_length = 1;
    struct RGBLedMatrix *matrix = led_matrix_create_from_options(&matrix_options, &argc, &argv);
    if (!matrix) {
        fprintf(stderr, "Error: Could not initialize LED matrix.\n");
        return 1;
    }
    // 화면 클리어
    struct LedCanvas *tmp = led_matrix_create_offscreen_canvas(matrix);
    led_canvas_fill(tmp, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b);
    struct LedCanvas *prev = led_matrix_swap_on_vsync(matrix, tmp);
    if (prev) led_canvas_clear(prev);

    // 2) 폰트 로드 (height ≤ 6 픽셀짜리 BDF 파일을 쓴다고 가정)
    struct LedFont *font = load_font("/home/pi/rpi-rgb-led-matrix/fonts/5x8.bdf");
    if (!font) {
        fprintf(stderr, "Error: Could not load font file.\n");
        cleanup_matrix(matrix);
        return 1;
    }

    // 3) 보드 입력 받기 (공백/쉼표 구분 가능)
    char input_board[BOARD_ROWS][BOARD_COLS + 1];
    for (int i = 0; i < BOARD_ROWS; ++i) {
        printf("Line %d: Enter 8 tokens (R, B, ., #) separated by space or comma:\n", i + 1);
        int count = 0;
        while (count < BOARD_COLS) {
            int ret = scanf(" %c%*[, ]", &input_board[i][count]);
            if (ret != 1) {
                fprintf(stderr, "Error: Could not read token %d on line %d.\n", count+1, i+1);
                delete_font(font);
                cleanup_matrix(matrix);
                return 1;
            }
            ++count;
        }
        input_board[i][BOARD_COLS] = '\0';
        // 남은 개행문자(또는 기타 문자)를 버퍼에서 제거
        int ch = getchar();
        while (ch != '\n' && ch != EOF) ch = getchar();
    }

    // 4) 보드 렌더링
    render_octaflip_board_with_text(matrix, font, input_board);

    // 5) 10초 동안 유지
    printf("Displaying board for 10 seconds...\n");
    sleep(10);

    // 6) 정리
    delete_font(font);
    // 화면 지우기
    tmp = led_matrix_create_offscreen_canvas(matrix);
    led_canvas_fill(tmp, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b);
    prev = led_matrix_swap_on_vsync(matrix, tmp);
    if (prev) led_canvas_clear(prev);
    // 매트릭스 해제
    led_matrix_delete(matrix);

    return 0;
}
