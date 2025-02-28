# Makefile for romlauncher

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O0 -g -DROMLAUNCHER_BUILD_LINUX -I/usr/include/SDL2
LDFLAGS = -lSDL2 -lSDL2_ttf -lSDL2_image

# Source and object files
SRC_DIR = source
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:.c=.o)

# Target binary
TARGET = romlauncher

# Default target
all:
	$(MAKE) -j16 build

build: $(TARGET)

# Linking the final binary
$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# Compiling source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(OBJ) $(TARGET)

test: all
	./romlauncher

# Phony targets
.PHONY: all build clean test
