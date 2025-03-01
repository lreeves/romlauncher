#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/emulator_selection.h"

// Test function prototypes
int test_derive_system_from_path();
int test_derive_emulator_from_path();

// Helper function to check test results
// Returns 0 for success, 1 for failure
int assert_emulator_config(const char* test_name,
                           Emulator expected_emulator,
                           const char* expected_core,
                           const EmulatorConfig* actual) {
    if (actual == NULL) {
        if (expected_emulator == EMULATOR_RETROARCH && expected_core == NULL) {
            printf("✓ %s: Both configs are NULL as expected\n", test_name);
            return 0;
        } else {
            printf("✗ %s: Expected emulator %d with core '%s' but got NULL\n",
                   test_name, expected_emulator,
                   expected_core ? expected_core : "NULL");
            return 1;
        }
    }

    int emulator_match = (actual->emulator == expected_emulator);
    int core_match = 0;

    if (expected_core == NULL && actual->core_name == NULL) {
        core_match = 1;
    } else if (expected_core != NULL && actual->core_name != NULL) {
        core_match = (strcmp(expected_core, actual->core_name) == 0);
    }

    if (emulator_match && core_match) {
        printf("✓ %s: Emulator=%d, Core='%s'\n",
               test_name, actual->emulator,
               actual->core_name ? actual->core_name : "NULL");
        return 0;
    } else {
        printf("✗ %s: Expected Emulator=%d, Core='%s' but got Emulator=%d, Core='%s'\n",
               test_name, expected_emulator, expected_core ? expected_core : "NULL",
               actual->emulator, actual->core_name ? actual->core_name : "NULL");
        return 1;
    }
}

// Test derive_emulator_from_path function
int test_derive_emulator_from_path() {
    printf("\nTesting derive_emulator_from_path:\n");
    int failures = 0;

    // Test case 1: Super Famicom ROM
    const char* input1 = "games/mario.sfc";
    const EmulatorConfig* result1 = derive_emulator_from_path(input1);
    failures += assert_emulator_config("Super Famicom ROM", EMULATOR_RETROARCH, "snes9x", result1);

    // Test case 1.1: NES ROM
    const char* input1_1 = "games/mario.nes";
    const EmulatorConfig* result1_1 = derive_emulator_from_path(input1_1);
    failures += assert_emulator_config("NES ROM", EMULATOR_RETROARCH, "fceumm", result1_1);

    // Test case 1.2: Sega Genesis/Mega Drive ROM
    const char* input1_2 = "games/sonic.md";
    const EmulatorConfig* result1_2 = derive_emulator_from_path(input1_2);
    failures += assert_emulator_config("Genesis ROM", EMULATOR_RETROARCH, "genesis_plus_gx", result1_2);

    // Test case 2: NULL input
    const EmulatorConfig* result2 = derive_emulator_from_path(NULL);
    failures += assert_emulator_config("NULL input", EMULATOR_RETROARCH, NULL, result2);

    // Test case 3: No extension
    const char* input3 = "games/mario";
    const EmulatorConfig* result3 = derive_emulator_from_path(input3);
    failures += assert_emulator_config("No extension", EMULATOR_RETROARCH, NULL, result3);

    // Test case 4: Unsupported extension
    const char* input4 = "games/document.txt";
    const EmulatorConfig* result4 = derive_emulator_from_path(input4);
    failures += assert_emulator_config("Unsupported extension", EMULATOR_RETROARCH, NULL, result4);

    // Test case 5: PlayStation path with CHD file
    const char* input5 = "/roms/playstation/test.chd";
    const EmulatorConfig* result5 = derive_emulator_from_path(input5);
    if (result5 != NULL) {
        failures += assert_emulator_config("PlayStation path", EMULATOR_RETROARCH, "pcsx_rearmed", result5);
    }

    // Test case 6: PlayStation uppercase path with CHD file
    const char* input6 = "/roms/PlayStation/test.chd";
    const EmulatorConfig* result6 = derive_emulator_from_path(input6);
    if (result6 != NULL) {
        failures += assert_emulator_config("PlayStation cased-path", EMULATOR_RETROARCH, "pcsx_rearmed", result6);
    }
    return failures;
}

// Helper function to check system type results
// Returns 0 for success, 1 for failure
int assert_system_type(const char* test_name, SystemType expected, SystemType actual) {
    if (expected == actual) {
        printf("✓ %s: System type matches (enum value: %d)\n", test_name, actual);
        return 0;
    } else {
        printf("✗ %s: Expected System type %d but got %d\n",
               test_name, expected, actual);
        return 1;
    }
}

// Test derive_system_from_path function
int test_derive_system_from_path() {
    printf("\nTesting derive_system_from_path:\n");
    int failures = 0;

    // Test case 1: NES ROM
    const char* input1 = "games/mario.nes";
    SystemType result1 = derive_system_from_path(input1);
    failures += assert_system_type("NES ROM", SYSTEM_NES, result1);

    // Test case 2: Super Famicom ROM
    const char* input2 = "games/mario.sfc";
    SystemType result2 = derive_system_from_path(input2);
    failures += assert_system_type("Super Famicom ROM", SYSTEM_SNES, result2);

    // Test case 3: Genesis ROM
    const char* input3 = "games/sonic.md";
    SystemType result3 = derive_system_from_path(input3);
    failures += assert_system_type("Genesis ROM", SYSTEM_GENESIS, result3);

    // Test case 4: NULL input
    SystemType result4 = derive_system_from_path(NULL);
    failures += assert_system_type("NULL input", SYSTEM_UNKNOWN, result4);

    // Test case 5: No extension
    const char* input5 = "games/mario";
    SystemType result5 = derive_system_from_path(input5);
    failures += assert_system_type("No extension", SYSTEM_UNKNOWN, result5);

    // Test case 6: Unsupported extension
    const char* input6 = "games/document.txt";
    SystemType result6 = derive_system_from_path(input6);
    failures += assert_system_type("Unsupported extension", SYSTEM_UNKNOWN, result6);

    return failures;
}

// Run all emulator_selection tests
int run_emulator_selection_tests() {
    printf("=== Running Emulator Selection Tests ===\n");
    int failures = 0;

    failures += test_derive_system_from_path();
    failures += test_derive_emulator_from_path();

    printf("=== Emulator Selection Tests Complete ===\n\n");
    return failures; // Return number of failures
}
