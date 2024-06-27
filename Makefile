# MODE         "debug" or "release".
# NAME         Name of the output executable (and object file directory).
# SOURCE_DIR   Directory where source files and headers are found.

MODE=debug
NAME=iii
SOURCE_DIR=src

ifeq ($(CPP),true)
	CFLAGS := -std=c++11
	C_LANG := -x c++
else
	CFLAGS := -std=c99
endif

CFLAGS += -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sequence-point

ifeq ($(SNIPPET),true)
	CFLAGS += -Wno-unused-function
endif

ifeq ($(MODE),debug)
	CFLAGS += -O0 -DDEBUG -g
	BUILD_DIR := build/debug
else
	CFLAGS += -O3 -flto
	BUILD_DIR := build/release
endif

HEADERS := $(wildcard $(SOURCE_DIR)/*.h)
SOURCES := $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS := $(addprefix $(BUILD_DIR)/$(NAME)/, $(notdir $(SOURCES:.c=.o)))

# Targets ---------------------------------------------------------------------

# Link the interpreter.
build/$(NAME): $(OBJECTS)
	@ printf "%8s %-40s %s\n" $(CC) $@ "$(CFLAGS)"
	@ mkdir -p build
	@ $(CC) $(CFLAGS) $^ -o $@

# Compile object files.
$(BUILD_DIR)/$(NAME)/%.o: $(SOURCE_DIR)/%.c $(HEADERS)
	@ printf "%8s %-40s %s\n" $(CC) $< "$(CFLAGS)"
	@ mkdir -p $(BUILD_DIR)/$(NAME)
	@ $(CC) -c $(C_LANG) $(CFLAGS) -o $@ $<

.PHONY: default


