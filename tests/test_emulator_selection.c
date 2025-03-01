#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/emulator_selection.h"

// Test function prototypes
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

// Run all emulator_selection tests
int run_emulator_selection_tests() {
    printf("=== Running Emulator Selection Tests ===\n");
    
    test_derive_emulator_from_path();
    
    printf("=== Emulator Selection Tests Complete ===\n\n");
    return 0; // Return 0 for success
}
