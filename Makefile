CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -std=c11 -g

BUILD_DIR := build
BUILD_STAMP := $(BUILD_DIR)/.dir
TARGET := $(BUILD_DIR)/tinykv-server
SRCS := $(wildcard src/*.c)
HDRS := $(wildcard src/*.h)

.PHONY: all build exec format clean

all: build

build: $(TARGET)

$(TARGET): $(SRCS) $(HDRS) | $(BUILD_STAMP)
	$(CC) $(CFLAGS) $(SRCS) -o $@

$(BUILD_STAMP):
	mkdir -p $(BUILD_DIR)
	touch $@

exec: build
	./$(TARGET)

format:
	clang-format --style=file -i $(SRCS) $(HDRS)

clean:
	rm -rf $(BUILD_DIR)
