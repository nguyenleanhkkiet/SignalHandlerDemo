CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS += -Iinclude -D_POSIX_C_SOURCE=200809L
DEPFLAGS := -MMD -MP
LDFLAGS ?=

BUILD_DIR := build
TARGET   := $(BUILD_DIR)/signal-handler-demo
SOURCES  := src/main.c src/logger.c src/process_manager.c src/menu.c src/tasks.c src/signal_masking.c
OBJECTS  := $(SOURCES:src/%.c=$(BUILD_DIR)/%.o)
DEPFILES := $(OBJECTS:.o=.d)

.PHONY: all clean run

all: $(TARGET)

-include $(DEPFILES)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)
