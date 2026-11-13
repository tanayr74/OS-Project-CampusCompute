CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS ?=
LDLIBS ?= -pthread -lrt

TARGET := campuscompute
SRC_DIR := src
BUILD_DIR := build
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all clean test
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/common.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) campuscompute.log resource_limit_test.tmp

test: $(TARGET)
	./$(TARGET) help
	./$(TARGET) schedule compare
	./$(TARGET) schedule compare sample_jobs.txt
	./$(TARGET) banker safe
	./$(TARGET) banker unsafe
	./$(TARGET) worker normal
	./$(TARGET) worker fail
