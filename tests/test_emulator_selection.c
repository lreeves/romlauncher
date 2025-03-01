#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/emulator_selection.h"

// Test function prototypes
void test_derive_system_from_path();
void test_derive_emulator_from_path();

// Helper function to check test results
void assert_emulator_config(const char* test_name, 
                           Emulator expected_emulator, 
                           const char* expected_core, 
                           const EmulatorConfig* actual) {
    if (actual == NULL) {
        if (expected_emulator == EMULATOR_RETROARCH && expected_core == NULL) {
            printf("✓ %s: Both configs are NULL as expected\n", test_name);
        } else {
            printf("✗ %s: Expected emulator %d with core '%s' but got NULL\n", 
                   test_name, expected_emulator, 
                   expected_core ? expected_core : "NULL");
        }
        return;
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
    } else {
        printf("✗ %s: Expected Emulator=%d, Core='%s' but got Emulator=%d, Core='%s'\n", 
               test_name, expected_emulator, expected_core ? expected_core : "NULL",
               actual->emulator, actual->core_name ? actual->core_name : "NULL");
    }
}

// Test derive_emulator_from_path function
void test_derive_emulator_from_path() {
    printf("\nTesting derive_emulator_from_path:\n");
    
    // Test case 1: Super Famicom ROM
    const char* input1 = "games/mario.sfc";
    const EmulatorConfig* result1 = derive_emulator_from_path(input1);
    assert_emulator_config("Super Famicom ROM", EMULATOR_RETROARCH, "snes9x", result1);
    
    // Test case 1.1: NES ROM
    const char* input1_1 = "games/mario.nes";
    const EmulatorConfig* result1_1 = derive_emulator_from_path(input1_1);
    assert_emulator_config("NES ROM", EMULATOR_RETROARCH, "fceumm", result1_1);
    
    // Test case 1.2: Sega Genesis/Mega Drive ROM
    const char* input1_2 = "games/sonic.md";
    const EmulatorConfig* result1_2 = derive_emulator_from_path(input1_2);
    assert_emulator_config("Genesis ROM", EMULATOR_RETROARCH, "genesis_plus_gx", result1_2);
    
    // Test case 2: NULL input
    const EmulatorConfig* result2 = derive_emulator_from_path(NULL);
    assert_emulator_config("NULL input", EMULATOR_RETROARCH, NULL, result2);
    
    // Test case 3: No extension
    const char* input3 = "games/mario";
    const EmulatorConfig* result3 = derive_emulator_from_path(input3);
    assert_emulator_config("No extension", EMULATOR_RETROARCH, NULL, result3);
    
    // Test case 4: Unsupported extension
    const char* input4 = "games/document.txt";
    const EmulatorConfig* result4 = derive_emulator_from_path(input4);
    assert_emulator_config("Unsupported extension", EMULATOR_RETROARCH, NULL, result4);
}

// Helper function to check system type results
void assert_system_type(const char* test_name, SystemType expected, SystemType actual) {
    if (expected == actual) {
        printf("✓ %s: System type matches (enum value: %d)\n", test_name, actual);
    } else {
        printf("✗ %s: Expected System type %d but got %d\n", 
               test_name, expected, actual);
    }
}

// Test derive_system_from_path function
void test_derive_system_from_path() {
    printf("\nTesting derive_system_from_path:\n");
    
    // Test case 1: NES ROM
    const char* input1 = "games/mario.nes";
    SystemType result1 = derive_system_from_path(input1);
    assert_system_type("NES ROM", SYSTEM_NES, result1);
    
    // Test case 2: Super Famicom ROM
    const char* input2 = "games/mario.sfc";
    SystemType result2 = derive_system_from_path(input2);
    assert_system_type("Super Famicom ROM", SYSTEM_SNES, result2);
    
    // Test case 3: Genesis ROM
    const char* input3 = "games/sonic.md";
    SystemType result3 = derive_system_from_path(input3);
    assert_system_type("Genesis ROM", SYSTEM_GENESIS, result3);
    
    // Test case 4: NULL input
    SystemType result4 = derive_system_from_path(NULL);
    assert_system_type("NULL input", SYSTEM_UNKNOWN, result4);
    
    // Test case 5: No extension
    const char* input5 = "games/mario";
    SystemType result5 = derive_system_from_path(input5);
    assert_system_type("No extension", SYSTEM_UNKNOWN, result5);
    
    // Test case 6: Unsupported extension
    const char* input6 = "games/document.txt";
    SystemType result6 = derive_system_from_path(input6);
    assert_system_type("Unsupported extension", SYSTEM_UNKNOWN, result6);
}

// Run all emulator_selection tests
int run_emulator_selection_tests() {
    printf("=== Running Emulator Selection Tests ===\n");
    
    test_derive_system_from_path();
    test_derive_emulator_from_path();
    
    printf("=== Emulator Selection Tests Complete ===\n\n");
    return 0; // Return 0 for success
}
