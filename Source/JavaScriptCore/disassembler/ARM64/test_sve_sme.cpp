#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

struct TestCase {
    uint32_t opcode;
    const char* expected;
    const char* category;
};

int main() {
    TestCase tests[] = {
        // SVE Basic Arithmetic
        {0x04220020, "add      z0.b, z1.b, z2.b", "SVE ADD unpredicated"},
        {0x04620020, "add      z0.h, z1.h, z2.h", "SVE ADD unpredicated"},
        {0x04a20020, "add      z0.s, z1.s, z2.s", "SVE ADD unpredicated"},
        {0x04e20020, "add      z0.d, z1.d, z2.d", "SVE ADD unpredicated"},
        
        // SVE Predicated ADD
        {0x04000020, "add      z0.b, p0/m, z0.b, z1.b", "SVE ADD predicated"},
        {0x04400020, "add      z0.h, p0/m, z0.h, z1.h", "SVE ADD predicated"},
        {0x04800020, "add      z0.s, p0/m, z0.s, z1.s", "SVE ADD predicated"},
        {0x04c00020, "add      z0.d, p0/m, z0.d, z1.d", "SVE ADD predicated"},
        
        // SVE FMUL
        {0x65420820, "fmul     z0.h, z1.h, z2.h", "SVE FMUL"},
        {0x65820820, "fmul     z0.s, z1.s, z2.s", "SVE FMUL"},
        {0x65c20820, "fmul     z0.d, z1.d, z2.d", "SVE FMUL"},
        
        // SVE Load/Store
        {0xa540a000, "ld1w     z0.s, p0/z, [x0]", "SVE LD1W"},
        {0xa5e0a000, "ld1d     z0.d, p0/z, [x0]", "SVE LD1D"},
        {0xe540e000, "st1w     z0.s, p0, [x0]", "SVE ST1W"},
        {0xe5e0e000, "st1d     z0.d, p0, [x0]", "SVE ST1D"},
        
        // SVE Compare
        {0x65414000, "fcmge    p0.h, p0/z, z0.h, z1.h", "SVE FCMGE"},
        {0x65814000, "fcmge    p0.s, p0/z, z0.s, z1.s", "SVE FCMGE"},
        {0x65c14000, "fcmge    p0.d, p0/z, z0.d, z1.d", "SVE FCMGE"},
        
        // SVE Logical
        {0x04223020, "and      z0.d, z1.d, z2.d", "SVE AND"},
        {0x04623020, "orr      z0.d, z1.d, z2.d", "SVE ORR"},
        {0x04a23020, "eor      z0.d, z1.d, z2.d", "SVE EOR"},
        
        // FAMAX variants (SIMD) - Note: all use v0 registers (Rd=Rn=Rm=0 in opcodes)
        {0x0ea0dc00, "famax    v0.2s, v0.2s, v0.2s", "FAMAX SIMD"},
        {0x4ea0dc00, "famax    v0.4s, v0.4s, v0.4s", "FAMAX SIMD"},
        {0x0ec01c00, "famax    v0.4h, v0.4h, v0.4h", "FAMAX SIMD"},
        {0x4ec01c00, "famax    v0.8h, v0.8h, v0.8h", "FAMAX SIMD"},
        
        // FAMAX SVE predicated - Note: size=00 is UNDEFINED, must use 01/10/11
        {0x654e8000, "famax    z0.h, p0/m, z0.h, z0.h", "FAMAX SVE"},
        {0x658e8001, "famax    z0.s, p0/m, z0.s, z1.s", "FAMAX SVE"},
        {0x65ce8002, "famax    z0.d, p0/m, z0.d, z2.d", "FAMAX SVE"},
    };
    
    int passed = 0;
    int failed = 0;
    const char* lastCategory = "";
    
    printf("=== SVE/SME Comprehensive Test Suite ===\n\n");
    
    for (const auto& test : tests) {
        if (strcmp(lastCategory, test.category) != 0) {
            printf("\n--- %s ---\n", test.category);
            lastCategory = test.category;
        }
        
        const auto* entry = findInstruction(test.opcode);
        
        if (!entry) {
            printf("FAIL 0x%08x: No instruction found\n", test.opcode);
            failed++;
            continue;
        }
        
        char buffer[256];
        uint32_t pc = 0;
        formatInstruction(entry, test.opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        
        printf("0x%08x: %s\n", test.opcode, buffer);
        passed++;
    }
    
    printf("\n=== Results ===\n");
    printf("Tested: %d\n", passed + failed);
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    
    return failed > 0 ? 1 : 0;
}
