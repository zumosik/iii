# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -Werror -Wno-unused-parameter

# Target executable
TARGET = iii

# Source and build directories
SRC_DIR = src
BUILD_DIR = build

# Find all .c files in the src directory
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Create a list of .o files in the build directory
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Default target
all: $(TARGET)

# Rule to link object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Rule to compile .c files to .o files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create the build directory if it does not exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean target to remove compiled files
clean:
	rm -f $(BUILD_DIR)/*.o $(TARGET)
	rm -rf $(BUILD_DIR)

# Phony targets
.PHONY: all clean

