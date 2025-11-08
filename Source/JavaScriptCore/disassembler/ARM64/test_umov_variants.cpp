#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_umov(uint32_t opcode, const char* expected) {
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
    printf("=== UMOV Instruction Tests ===\n\n");
    
    // UMOV W variants (32-bit)
    test_umov(0x0E013C00, "umov     w0, v0.b[0]");
    test_umov(0x0E023C00, "umov     w0, v0.h[0]");
    test_umov(0x0E043C00, "umov     w0, v0.s[0]");
    
    // UMOV X variant (64-bit)
    test_umov(0x4E083C00, "umov     x0, v0.d[0]");
    test_umov(0x4E183C00, "umov     x0, v0.d[1]");
    
    printf("\n=== Test Complete ===\n");
    return 0;
}
