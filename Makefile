# Makefile Unified

BUILD_TYPE ?= client

# 컴파일러 및 공통 링커 설정
CC = gcc
LDFLAGS_COMMON = -lrt -lm -lpthread -lstdc++

# rpi-rgb-led-matrix 라이브러리 경로 설정
RGB_MATRIX_BASE_DIR = ./rpi-rgb-led-matrix
RGB_MATRIX_INC_DIR = $(RGB_MATRIX_BASE_DIR)/include
RGB_MATRIX_LIB_DIR = $(RGB_MATRIX_BASE_DIR)/lib
RGB_LIBRARY_NAME = rgbmatrix
# 정적 라이브러리 파일 경로
RGB_LIBRARY = $(RGB_MATRIX_LIB_DIR)/lib$(RGB_LIBRARY_NAME).a

# BUILD_TYPE에 따른 조건부 설정
ifeq ($(BUILD_TYPE), client)
    TARGET_EXECUTABLE := client
    SOURCE_FILES      := client.c cJSON.c board.c
    # 이 빌드 타입을 위한 CFLAGS
    CFLAGS            := -Wall -O3 -g -Wextra -Wno-unused-parameter
else ifeq ($(BUILD_TYPE), standalone_test)
    TARGET_EXECUTABLE := standalone_board_test
    SOURCE_FILES      := board.c
    CFLAGS            := -Wall -O3 -g -Wextra -Wno-unused-parameter -DSTANDALONE_BOARD_TEST
else
    $(error "Invalid BUILD_TYPE: '$(BUILD_TYPE)'. Use 'client' or 'standalone_test'")
endif

# 'all' 타겟은 'make' 명령어 실행 시 기본 목표입니다.
# BUILD_TYPE에 따라 설정된 TARGET_EXECUTABLE을 빌드합니다.
all: $(TARGET_EXECUTABLE)

# rpi-rgb-led-matrix 라이브러리 빌드 규칙
# $(TARGET_EXECUTABLE)의 전제 조건입니다.
$(RGB_LIBRARY):
	@echo "rpi-rgb-led-matrix 라이브러리('$(RGB_LIBRARY_NAME)') 빌드를 확인/진행합니다..."
	@# 라이브러리 빌드 명령어. $(RGB_MATRIX_LIB_DIR) 내의 Makefile이
	@# lib$(RGB_LIBRARY_NAME).a 파일을 올바르게 빌드한다고 가정합니다.
	$(MAKE) -C $(RGB_MATRIX_LIB_DIR)

# 최종 실행 파일 빌드 규칙
# 이 규칙은 BUILD_TYPE에 의해 결정되는 TARGET_EXECUTABLE, SOURCE_FILES, CFLAGS 변수를 사용합니다.
$(TARGET_EXECUTABLE): $(SOURCE_FILES) $(RGB_LIBRARY)
	@echo "'BUILD_TYPE=$(BUILD_TYPE)'에 대해 $(TARGET_EXECUTABLE)을(를) 컴파일하고 링크합니다..."
	$(CC) $(CFLAGS) $(SOURCE_FILES) -o $@ \
		-I$(RGB_MATRIX_INC_DIR) \
		-L$(RGB_MATRIX_LIB_DIR) \
		-l$(RGB_LIBRARY_NAME) \
		$(LDFLAGS_COMMON)
	@echo "$@ 빌드 성공."

# 빌드 결과물 정리 규칙
# 모든 알려진 설정의 실행 파일을 정리합니다.
clean:
	@echo "빌드 결과물을 정리합니다..."
	rm -f client standalone_board_test
	@# 선택 사항: 'make clean' 시 rpi-rgb-led-matrix 라이브러리도 정리하려면 다음 주석을 해제하십시오.
	@# echo "rpi-rgb-led-matrix 라이브러리를 정리합니다..."
	@# $(MAKE) -C $(RGB_MATRIX_LIB_DIR) clean
	@echo "정리 완료."

.PHONY: all clean $(RGB_LIBRARY)
