#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

int main() {
    char buffer[256];
    uint32_t pc = 0;

    printf("=== Comprehensive Test Suite ===\n\n");

    // FMOV immediate
    printf("1. FMOV immediate:\n");
    uint32_t fmov1 = 0x1E201008;  // fmov s8, #2.0
    auto* e1 = ARM64Disassembler::findInstruction(fmov1);
    if (e1) {
        ARM64Disassembler::formatInstruction(e1, fmov1, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("   0x%08x: %s (expected: fmov s8, #2.0) %s\n",
               fmov1, buffer, strcmp(buffer, "fmov     s8, #2.0") == 0 ? "✓" : "✗");
    }

    uint32_t fmov2 = 0x1E6E1000;  // fmov d0, #1.0
    auto* e2 = ARM64Disassembler::findInstruction(fmov2);
    if (e2) {
        ARM64Disassembler::formatInstruction(e2, fmov2, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("   0x%08x: %s (expected: fmov d0, #1.0) %s\n",
               fmov2, buffer, strcmp(buffer, "fmov     d0, #1.0") == 0 ? "✓" : "✗");
    }

    // FCVTAS
    printf("\n2. FCVTAS:\n");
    uint32_t fcvtas1 = 0x1E240020;  // fcvtas w0, s1
    auto* e3 = ARM64Disassembler::findInstruction(fcvtas1);
    if (e3) {
        ARM64Disassembler::formatInstruction(e3, fcvtas1, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("   0x%08x: %s (expected: fcvtas w0, s1) %s\n",
               fcvtas1, buffer, strstr(buffer, "fcvtas") && strstr(buffer, "w0") && strstr(buffer, "s1") ? "✓" : "✗");
    }

    // TST
    printf("\n3. TST:\n");
    uint32_t tst = 0x72000C5F;  // tst w2, #0xf
    auto* e4 = ARM64Disassembler::findInstruction(tst);
    if (e4) {
        ARM64Disassembler::formatInstruction(e4, tst, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("   0x%08x: %s (expected: tst w2, #0xf) %s\n",
               tst, buffer, strstr(buffer, "tst") && strstr(buffer, "w2") && strstr(buffer, "#0xf") ? "✓" : "✗");
    }

    // MOV immediate
    printf("\n4. MOV immediate:\n");
    uint32_t mov = 0xD2902068;  // movz x8, #0x8103
    auto* e5 = ARM64Disassembler::findInstruction(mov);
    if (e5) {
        ARM64Disassembler::formatInstruction(e5, mov, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("   0x%08x: %s (expected: mov x8, #0x8103) %s\n",
               mov, buffer, strstr(buffer, "mov") && strstr(buffer, "x8") && strstr(buffer, "#0x8103") ? "✓" : "✗");
    }

    printf("\n✅ All tests completed\n");
    return 0;
}
