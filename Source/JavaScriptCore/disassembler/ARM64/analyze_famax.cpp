#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

static inline uint32_t extractBits(uint32_t value, unsigned start, unsigned width) {
    return (value >> start) & ((1U << width) - 1);
}

int main() {
    uint32_t famax_single_q0 = 0x2ea01c00;  // Should be FAMAX v0.2s
    uint32_t famax_half_q0   = 0x2ec01c00;  // FAMAX v0.4h (this works\!)
    
    printf("=== Analyzing FAMAX Opcodes ===\n\n");
    
    printf("FAMAX single Q=0: 0x%08x\n", famax_single_q0);
    printf("  bits[28:24] = 0x%x (should be 0xE for SIMD FP)\n", extractBits(famax_single_q0, 24, 5));
    printf("  bit[23]     = %u (should be 1 for FP with size)\n", extractBits(famax_single_q0, 23, 1));
    printf("  bits[22:21] = 0x%x (size field)\n", extractBits(famax_single_q0, 21, 2));
    printf("  bits[15:10] = 0x%x (should be 0x07 for FAMAX/FAMIN)\n", extractBits(famax_single_q0, 10, 6));
    printf("  bit[30] Q   = %u\n", extractBits(famax_single_q0, 30, 1));
    
    const auto* entry = findInstruction(famax_single_q0);
    if (entry) {
        printf("  Matched: %s (mask=0x%08x, pattern=0x%08x)\n", 
               entry->mnemonic, entry->mask, entry->pattern);
        
        char buffer[256];
        uint32_t pc = 0;
        formatInstruction(entry, famax_single_q0, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("  Output: %s\n", buffer);
    } else {
        printf("  NO MATCH\n");
    }
    
    printf("\nFAMAX half Q=0: 0x%08x\n", famax_half_q0);
    printf("  bits[28:24] = 0x%x\n", extractBits(famax_half_q0, 24, 5));
    printf("  bit[23]     = %u\n", extractBits(famax_half_q0, 23, 1));
    printf("  bits[22:21] = 0x%x (size field)\n", extractBits(famax_half_q0, 21, 2));
    printf("  bits[15:10] = 0x%x\n", extractBits(famax_half_q0, 10, 6));
    printf("  bit[30] Q   = %u\n", extractBits(famax_half_q0, 30, 1));
    
    entry = findInstruction(famax_half_q0);
    if (entry) {
        printf("  Matched: %s (mask=0x%08x, pattern=0x%08x)\n", 
               entry->mnemonic, entry->mask, entry->pattern);
        
        char buffer[256];
        uint32_t pc = 0;
        formatInstruction(entry, famax_half_q0, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("  Output: %s\n", buffer);
    } else {
        printf("  NO MATCH\n");
    }
    
    // Search for all FAMAX entries in table
    printf("\n=== Searching instruction table for FAMAX ===\n");
    int count = 0;
    for (size_t i = 0; i < g_instructionTableSize; i++) {
        if (strcmp(g_instructionTable[i].mnemonic, "famax") == 0) {
            printf("Entry %zu: mask=0x%08x pattern=0x%08x\n", 
                   i, g_instructionTable[i].mask, g_instructionTable[i].pattern);
            count++;
        }
    }
    printf("Total FAMAX entries: %d\n", count);
    
    return 0;
}
