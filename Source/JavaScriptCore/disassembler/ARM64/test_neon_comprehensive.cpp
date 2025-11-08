#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_opcode(uint32_t opcode, const char* expected) {
    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));
    
    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("0x%08x: %s\n", opcode, buffer);
        if (expected) printf("Expected:   %s\n", expected);
    } else {
        printf("0x%08x: NOT FOUND\n", opcode);
    }
    printf("\n");
}

int main() {
    printf("=== NEON/SIMD Instruction Tests ===\n\n");

    printf("--- INS (Insert Element) ---\n");
    test_opcode(0x4E011C01, "ins      v1.b[0], w0");
    test_opcode(0x4E021C01, "ins      v1.h[0], w0");
    test_opcode(0x4E041C01, "ins      v1.s[0], w0");
    test_opcode(0x6E081C01, "ins      v1.d[0], x0");

    printf("--- MOV (Element to Element) ---\n");
    test_opcode(0x6E044401, "mov      v1.s[0], v0.s[0]");
    test_opcode(0x6E0C4C21, "mov      v1.d[1], v1.d[1]");

    printf("--- DUP (Duplicate Element to Vector) ---\n");
    test_opcode(0x0E010420, "dup      v0.8b, v1.b[0]");
    test_opcode(0x4E010420, "dup      v0.16b, v1.b[0]");
    test_opcode(0x0E020420, "dup      v0.4h, v1.h[0]");
    test_opcode(0x4E020420, "dup      v0.8h, v1.h[0]");
    test_opcode(0x0E040420, "dup      v0.2s, v1.s[0]");
    test_opcode(0x4E040420, "dup      v0.4s, v1.s[0]");
    test_opcode(0x4E080420, "dup      v0.2d, v1.d[0]");

    printf("--- ADD/SUB (Vector) ---\n");
    test_opcode(0x0E228420, "add      v0.8b, v1.8b, v2.8b");
    test_opcode(0x4E228420, "add      v0.16b, v1.16b, v2.16b");

    printf("--- MUL (Vector) ---\n");
    test_opcode(0x0E629C20, "mul      v0.8b, v1.8b, v2.8b");

    printf("--- SXTL (Sign Extend Long) ---\n");
    test_opcode(0x0F08A401, "sxtl     v1.8h, v0.8b");
    test_opcode(0x0F10A401, "sxtl     v1.4s, v0.4h");

    printf("\n=== Test Complete ===\n");
    return 0;
}
