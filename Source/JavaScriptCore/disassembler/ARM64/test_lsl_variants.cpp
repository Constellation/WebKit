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
    printf("=== Bitfield Shift Alias Tests ===\n\n");
    
    // LSL - Logical Shift Left
    // LSL Xd, Xn, #shift = UBFM Xd, Xn, #(-shift MOD 64), #(63-shift)
    test(0xD37AF400, "lsl      x0, x0, #6");    // immr=58, imms=57
    test(0xD37EF420, "lsl      x1, x1, #2");    // immr=62, imms=61
    test(0x531E7400, "lsl      w0, w0, #2");    // immr=30, imms=29
    
    // LSR - Logical Shift Right  
    // LSR Xd, Xn, #shift = UBFM Xd, Xn, #shift, #63 (for 64-bit)
    test(0xD35FFC00, "lsr      x0, x0, #31");   // immr=31, imms=63
    test(0x53017C00, "lsr      w0, w0, #1");    // immr=1, imms=31
    
    // ASR - Arithmetic Shift Right
    // ASR Xd, Xn, #shift = SBFM Xd, Xn, #shift, #63 (for 64-bit)  
    test(0x9341FC00, "asr      x0, x0, #1");    // immr=1, imms=63
    test(0x13017C00, "asr      w0, w0, #1");    // immr=1, imms=31
    
    printf("\n=== All Bitfield Shift Tests Complete ===\n");
    return 0;
}
