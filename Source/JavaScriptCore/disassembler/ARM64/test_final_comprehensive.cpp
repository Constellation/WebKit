#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test(uint32_t opcode, const char* expected) {
    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));
    
    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));
        const char* actual = buffer;
        while (*actual == ' ') actual++;
        
        bool match = strcmp(actual, expected) == 0;
        printf("%s 0x%08x: %s\n", match ? "✅" : "❌", opcode, actual);
        if (match == false) printf("   Expected: %s\n", expected);
    } else {
        printf("❌ 0x%08x: NOT FOUND (expected: %s)\n", opcode, expected);
    }
}

int main() {
    printf("=== Final Comprehensive Test Suite ===\n\n");
    
    printf("--- Shift Amount Fix ---\n");
    test(0x0B000824, "add      w4, w1, w0, lsl #2");
    printf("\n");
    
    printf("--- UMOV Indexed Elements ---\n");
    test(0x0E013C00, "umov     w0, v0.b[0]");
    test(0x0E023C00, "umov     w0, v0.h[0]");
    test(0x4E083C00, "umov     x0, v0.d[0]");
    test(0x4E183C00, "umov     x0, v0.d[1]");
    printf("\n");
    
    printf("--- SXTL/SXTL2 Alias with Q-bit Suffix ---\n");
    test(0x0F08A401, "sxtl     v1.8h, v0.8b");
    test(0x4F08A401, "sxtl2    v1.8h, v0.16b");
    test(0x0F10A401, "sxtl     v1.4s, v0.4h");
    test(0x4F10A401, "sxtl2    v1.4s, v0.8h");
    printf("\n");
    
    printf("--- UXTL/UXTL2 Alias with Q-bit Suffix ---\n");
    test(0x2F08A401, "uxtl     v1.8h, v0.8b");
    test(0x6F08A401, "uxtl2    v1.8h, v0.16b");
    test(0x2F10A401, "uxtl     v1.4s, v0.4h");
    test(0x6F10A401, "uxtl2    v1.4s, v0.8h");
    printf("\n");
    
    printf("--- DUP Arrangements ---\n");
    test(0x0E010420, "dup      v0.8b, v1.b[0]");
    test(0x4E010420, "dup      v0.16b, v1.b[0]");
    test(0x4E080420, "dup      v0.2d, v1.d[0]");
    printf("\n");
    
    printf("--- ADD Vector ---\n");
    test(0x0E228420, "add      v0.8b, v1.8b, v2.8b");
    test(0x4EA28420, "add      v0.4s, v1.4s, v2.4s");
    test(0x4EE28420, "add      v0.2d, v1.2d, v2.2d");
    printf("\n");
    
    printf("=== All Tests Complete ===\n");
    return 0;
}
