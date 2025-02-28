#include <stdio.h>

// Test function prototypes
int run_path_utils_tests();

int main() {
    printf("Starting test suite...\n\n");
    
    // Run all test modules
    int result = 0;
    
    result += run_path_utils_tests();
    
    // Add more test modules here as they are created
    // result += run_other_module_tests();
    
    if (result == 0) {
        printf("All tests passed successfully!\n");
    } else {
        printf("Some tests failed. Check the output above for details.\n");
    }
    
    return result;
}
