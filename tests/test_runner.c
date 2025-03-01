#include <stdio.h>

// Test function prototypes
int run_path_utils_tests();
int run_emulator_selection_tests();

int main() {
    printf("Starting test suite...\n\n");
    
    // Run all test modules
    int failures = 0;
    
    failures += run_path_utils_tests();
    failures += run_emulator_selection_tests();
    
    if (failures == 0) {
        printf("All tests passed successfully!\n");
    } else {
        printf("FAILED: %d test(s) failed. Check the output above for details.\n", failures);
    }
    
    return failures > 0 ? 1 : 0; // Return 1 if any failures, 0 otherwise
}
