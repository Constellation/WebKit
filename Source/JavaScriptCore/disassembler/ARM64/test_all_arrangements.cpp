#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>
#include <set>
#include <map>
#include <string>

using namespace JSC::ARM64Disassembler;

// Helper to extract bits
static inline uint32_t extractBits(uint32_t value, unsigned start, unsigned width) {
    return (value >> start) & ((1U << width) - 1);
}

struct InstructionPattern {
    std::string mnemonic;
    uint32_t opcode;
    uint32_t bits2824;
    uint32_t bit23;
    uint32_t bits2322;
    uint32_t bits1510;
    uint32_t Q;
    std::string expectedArrangement;
};

int main() {
    printf("=== Analyzing ALL Q-bit Arrangement Instructions ===\n\n");
    
    // Collect all instructions with Q-bit arrangements
    // Pattern: Instructions using SIMD arranged operands with Q bit
    
    std::map<std::string, std::set<uint32_t>> mnemonicPatterns;
    
    // Test vectors for different instruction families
    struct TestCase {
        const char* family;
        uint32_t opcodeQ0;
        uint32_t opcodeQ1;
        const char* expectedQ0;
        const char* expectedQ1;
    };
    
    TestCase tests[] = {
        // SIMD Arithmetic (3 same) - size:Q encoding
        {"ADD (vector)", 0x0e228400, 0x4e228400, "v0.8b", "v0.16b"},   // size=00
        {"ADD (vector)", 0x0e628400, 0x4e628400, "v0.4h", "v0.8h"},    // size=01
        {"ADD (vector)", 0x0ea28400, 0x4ea28400, "v0.2s", "v0.4s"},    // size=10
        {"ADD (vector)", 0x0ee28400, 0x4ee28400, "v0.1d", "v0.2d"},    // size=11
        
        // FP SIMD arithmetic - sz:Q encoding
        {"FMUL (vector)", 0x2e20dc00, 0x6e20dc00, "v0.2s", "v0.4s"},   // sz=0
        {"FMUL (vector)", 0x2e60dc00, 0x6e60dc00, "v0.1d", "v0.2d"},   // sz=1
        
        // FAMAX/FAMIN - size:Q with special encoding
        {"FAMAX single", 0x0ea1dc00, 0x4ea1dc00, "v0.2s", "v0.4s"},          // U=0, size=10
        {"FAMAX half", 0x0ec01c00, 0x4ec01c00, "v0.4h", "v0.8h"},            // U=0, size=10, opcode=000111
        {"FAMIN single", 0x2ea1dc00, 0x6ea1dc00, "v0.2s", "v0.4s"},          // U=1, size=10
        {"FAMIN half", 0x2ec01c00, 0x6ec01c00, "v0.4h", "v0.8h"},            // U=1, size=10, opcode=000111
        
        // Logical operations - always bytes
        {"AND (vector)", 0x0e201c00, 0x4e201c00, "v0.8b", "v0.16b"},
        {"ORR (vector)", 0x0e221e20, 0x4e221e20, "v0.8b", "v0.16b"},  // size=00, Rd=v0, Rn=v1, Rm=v2
        {"EOR (vector)", 0x2e201c00, 0x6e201c00, "v0.8b", "v0.16b"},
        
        // Table lookup - always bytes
        {"TBL", 0x0e000000, 0x4e000000, "v0.8b", "v0.16b"},
        {"TBX", 0x0e001000, 0x4e001000, "v0.8b", "v0.16b"},
        
        // DUP (element) - imm5 encodes size and index
        {"DUP (element)", 0x0e010400, 0x4e010400, "v0.8b", "v0.16b"},  // imm5=00001 (byte)
        {"DUP (element)", 0x0e020400, 0x4e020400, "v0.4h", "v0.8h"},   // imm5=00010 (halfword)
        {"DUP (element)", 0x0e040400, 0x4e040400, "v0.2s", "v0.4s"},   // imm5=00100 (word)
        {"DUP (element)", 0x0e080400, 0x4e080400, "v0.1d", "v0.2d"},   // imm5=01000 (doubleword)
        
        // EXT - always bytes
        {"EXT", 0x2e000000, 0x6e000000, "v0.8b", "v0.16b"},
        
        // MOVI - bytes for immediate form
        {"MOVI", 0x0f00e400, 0x4f00e400, "v0.8b", "v0.16b"},
        
        // FMOV (immediate, vector) - depends on cmode
        {"FMOV (vector)", 0x0f00f400, 0x4f00f400, "v0.2s", "v0.4s"},  // cmode=1111, op=0
    };
    
    int numTests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    int failed = 0;
    
    printf("Testing %d instruction patterns...\n\n", numTests);
    
    for (int i = 0; i < numTests; i++) {
        // Test Q=0 variant
        const auto* entry0 = findInstruction(tests[i].opcodeQ0);
        if (entry0) {
            char buffer[256];
            uint32_t pc = 0;
            formatInstruction(entry0, tests[i].opcodeQ0, &pc, nullptr, nullptr, buffer, sizeof(buffer));
            
            // Check if expected arrangement is in output
            if (strstr(buffer, tests[i].expectedQ0)) {
                printf("✓ PASS: %s (Q=0) 0x%08x → %s\n", 
                       tests[i].family, tests[i].opcodeQ0, buffer);
                passed++;
            } else {
                printf("✗ FAIL: %s (Q=0) 0x%08x\n", tests[i].family, tests[i].opcodeQ0);
                printf("   Got:      %s\n", buffer);
                printf("   Expected: %s\n", tests[i].expectedQ0);
                failed++;
            }
        } else {
            printf("✗ FAIL: %s (Q=0) 0x%08x - NOT FOUND\n", tests[i].family, tests[i].opcodeQ0);
            failed++;
        }
        
        // Test Q=1 variant
        const auto* entry1 = findInstruction(tests[i].opcodeQ1);
        if (entry1) {
            char buffer[256];
            uint32_t pc = 0;
            formatInstruction(entry1, tests[i].opcodeQ1, &pc, nullptr, nullptr, buffer, sizeof(buffer));
            
            // Check if expected arrangement is in output
            if (strstr(buffer, tests[i].expectedQ1)) {
                printf("✓ PASS: %s (Q=1) 0x%08x → %s\n", 
                       tests[i].family, tests[i].opcodeQ1, buffer);
                passed++;
            } else {
                printf("✗ FAIL: %s (Q=1) 0x%08x\n", tests[i].family, tests[i].opcodeQ1);
                printf("   Got:      %s\n", buffer);
                printf("   Expected: %s\n", tests[i].expectedQ1);
                failed++;
            }
        } else {
            printf("✗ FAIL: %s (Q=1) 0x%08x - NOT FOUND\n", tests[i].family, tests[i].opcodeQ1);
            failed++;
        }
        
        printf("\n");
    }
    
    printf("=== Results ===\n");
    printf("Passed: %d/%d\n", passed, passed + failed);
    printf("Failed: %d/%d\n", failed, passed + failed);
    
    return failed == 0 ? 0 : 1;
}
