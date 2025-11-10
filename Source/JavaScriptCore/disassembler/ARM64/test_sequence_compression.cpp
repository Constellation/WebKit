#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

struct TestCase {
    uint32_t opcode;
    const char* expected;
};

int main() {
    TestCase tests[] = {
        // Shift operations
        {0x0b000824, "add      w4, w1, w0, lsl #2"},
        {0xd3607c41, "lsl      x1, x2, #32"},
        {0xd340fc41, "lsr      x1, x2, #0"},
        
        // SIMD operations
        {0x0e013c00, "umov     w0, v0.b[0]"},
        {0x4f08a401, "sxtl2    v1.8h, v0.16b"},
        {0x6f08a401, "uxtl2    v1.8h, v0.16b"},
        {0x4e010420, "dup      v0.16b, v1.b[0]"},
        {0x4ea28420, "add      v0.4s, v1.4s, v2.4s"},
        
        // Memory operations
        {0x38601820, "ldrb     w0, [x1, w0, uxtw]"},
        {0xf8408423, "ldr      x3, [x1], #8"},
        {0xa9007bfd, "stp      fp, lr, [sp]"},
        
        // Floating point
        {0x1e602000, "fmul     d0, d0, d0"},
        {0x0e20dc00, "fmul     v0.2s, v0.2s, v0.2s"},
        
        // NEON table lookup
        {0x4e000000, "tbl      v0.16b, { v0.16b }, v0.16b"},
        
        // Load register list
        {0x0c407000, "ld1      { v0.8b }, [x0]"},
        
        // Conditional
        {0x54000020, "b.eq     0x0 (<0>)"},
        {0x36000020, "tbz      w0, #0, 0x0 (<0>)"},
    };
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& test : tests) {
        const auto* entry = findInstruction(test.opcode);
        char buffer[128];
        uint32_t pc = 0;
        formatInstruction(entry, test.opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        
        if (strcmp(buffer, test.expected) == 0) {
            passed++;
            printf("✅ 0x%08x: %s\n", test.opcode, buffer);
        } else {
            failed++;
            printf("❌ 0x%08x: Expected: %s\n", test.opcode, test.expected);
            printf("           Got:      %s\n", buffer);
        }
    }
    
    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d\n", passed, passed + failed);
    printf("Failed: %d/%d\n", failed, passed + failed);
    
    return failed > 0 ? 1 : 0;
}
