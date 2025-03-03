#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/path_utils.h"
#include "../source/config.h"

// Test function prototypes
int test_absolute_to_relative();
int test_relative_to_absolute();

// Helper function to check test results
// Returns 0 for success, 1 for failure
int assert_str_equals(const char* test_name, const char* expected, const char* actual) {
    if (expected == NULL && actual == NULL) {
        printf("✓ %s: Both strings are NULL as expected\n", test_name);
        return 0;
    }

    if (expected == NULL) {
        printf("✗ %s: Expected NULL but got '%s'\n", test_name, actual);
        return 1;
    }

    if (actual == NULL) {
        printf("✗ %s: Expected '%s' but got NULL\n", test_name, expected);
        return 1;
    }

    if (strcmp(expected, actual) == 0) {
        printf("✓ %s: '%s'\n", test_name, actual);
        return 0;
    } else {
        printf("✗ %s: Expected '%s' but got '%s'\n", test_name, expected, actual);
        return 1;
    }
}

// Test absolute_rom_path_to_relative function
int test_absolute_to_relative() {
    printf("\nTesting absolute_rom_path_to_relative:\n");
    int failures = 0;

    // Test case 1: Normal absolute path
    char* input1 = ROM_DIRECTORY "game.nes";
    char* expected1 = "game.nes";
    char* result1 = absolute_rom_path_to_relative(input1);
    failures += assert_str_equals("Normal absolute path", expected1, result1);
    free(result1);

    // Test case 2: Already relative path
    char* input2 = "nes/game.nes";
    char* expected2 = "nes/game.nes";
    char* result2 = absolute_rom_path_to_relative(input2);
    failures += assert_str_equals("Already relative path", expected2, result2);
    free(result2);

    // Test case 3: NULL input
    char* result3 = absolute_rom_path_to_relative(NULL);
    failures += assert_str_equals("NULL input", NULL, result3);

    // Test case 4: Different prefix
    char* input4 = "sdmc:/different/path/game.nes";
    char* expected4 = "sdmc:/different/path/game.nes";
    char* result4 = absolute_rom_path_to_relative(input4);
    failures += assert_str_equals("Different prefix", expected4, result4);
    free(result4);

    return failures;
}

// Test relative_rom_path_to_absolute function
int test_relative_to_absolute() {
    printf("\nTesting relative_rom_path_to_absolute:\n");
    int failures = 0;

    // Test case 1: Normal relative path
    char* input1 = "nes/game.nes";
    char* expected1 = ROM_DIRECTORY "/nes/game.nes";
    char* result1 = relative_rom_path_to_absolute(input1);
    failures += assert_str_equals("Normal relative path", expected1, result1);
    free(result1);

    // Test case 2: Already absolute path
    char* input2 = ROM_DIRECTORY "/nes/game.nes";
    char* expected2 = ROM_DIRECTORY "/nes/game.nes";
    char* result2 = relative_rom_path_to_absolute(input2);
    failures += assert_str_equals("Already absolute path", expected2, result2);
    free(result2);

    // Test case 3: NULL input
    char* result3 = relative_rom_path_to_absolute(NULL);
    failures += assert_str_equals("NULL input", NULL, result3);

    return failures;
}

// Run all path_utils tests
int run_path_utils_tests() {
    printf("=== Running Path Utils Tests ===\n");
    int failures = 0;

    failures += test_absolute_to_relative();
    failures += test_relative_to_absolute();

    printf("=== Path Utils Tests Complete ===\n\n");
    return failures; // Return number of failures
}
