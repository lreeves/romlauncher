CC = gcc
CFLAGS = -Wall -Wextra -g -DROMLAUNCHER_BUILD_LINUX

# Source files
TEST_SOURCES = test_runner.c test_path_utils.c test_emulator_selection.c mock_logging.c
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

# Source files from main project needed for testing
PROJECT_SOURCES = ../source/path_utils.c ../source/emulator_selection.c
PROJECT_OBJECTS = $(PROJECT_SOURCES:.c=.o)

# Include directories
CFLAGS += -I../source

# Target executable
TEST_EXECUTABLE = test_runner

all: $(TEST_EXECUTABLE)

$(TEST_EXECUTABLE): $(TEST_OBJECTS) $(PROJECT_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TEST_OBJECTS) $(PROJECT_OBJECTS) $(TEST_EXECUTABLE)

run: $(TEST_EXECUTABLE)
	./$(TEST_EXECUTABLE)

.PHONY: all clean run
