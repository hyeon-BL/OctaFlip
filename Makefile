# Makefile (board.c와 같은 경로에 위치)

# 컴파일러 및 링커 설정
CC = gcc
CFLAGS = -Wall -O3 -g -Wextra -Wno-unused-parameter -DSTANDALONE_BOARD_TEST # 사용자 정의 CFLAGS 추가
LDFLAGS = -lrt -lm -lpthread # 기본 LDFLAGS

# rpi-rgb-led-matrix 라이브러리 경로 설정
# 이 Makefile이 있는 위치에서 rpi-rgb-led-matrix 폴더를 가리킵니다.
RGB_MATRIX_BASE_DIR = ./rpi-rgb-led-matrix
RGB_MATRIX_INC_DIR = $(RGB_MATRIX_BASE_DIR)/include
RGB_MATRIX_LIB_DIR = $(RGB_MATRIX_BASE_DIR)/lib
RGB_LIBRARY_NAME = rgbmatrix
RGB_LIBRARY = $(RGB_MATRIX_LIB_DIR)/lib$(RGB_LIBRARY_NAME).a

# 최종 실행 파일 이름 및 소스 파일
TARGET = standalone_board_test
SRC = board.c

# C API를 사용하는 경우 C++ 런타임 라이브러리 링크가 필요합니다.
# rpi-rgb-led-matrix 라이브러리가 C++로 작성되었기 때문입니다.
LDFLAGS += -lstdc++

# 기본 빌드 목표
all: $(TARGET)

# 라이브러리 빌드 (필요한 경우)
# rpi-rgb-led-matrix/lib 디렉토리의 Makefile이 라이브러리를 빌드합니다.
$(RGB_LIBRARY):
	@echo "Attempting to build rpi-rgb-led-matrix library..."
	$(MAKE) -C $(RGB_MATRIX_LIB_DIR)

# 실행 파일 빌드 규칙
$(TARGET): $(SRC) $(RGB_LIBRARY)
	@echo "Compiling and linking $(TARGET)..."
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) \
		-I$(RGB_MATRIX_INC_DIR) \
		-L$(RGB_MATRIX_LIB_DIR) \
		-l$(RGB_LIBRARY_NAME) \
		$(LDFLAGS)
	@echo "$(TARGET) built successfully."

# 정리 규칙
clean:
	@echo "Cleaning up..."
	rm -f $(TARGET)
	# 필요하다면 $(RGB_MATRIX_LIB_DIR) 내의 object 파일도 정리할 수 있으나,
	# 보통은 해당 라이브러리의 Makefile에서 처리합니다.
	# $(MAKE) -C $(RGB_MATRIX_LIB_DIR) clean

# .PHONY는 실제 파일 이름이 아닌 타겟을 지정합니다.
.PHONY: all clean