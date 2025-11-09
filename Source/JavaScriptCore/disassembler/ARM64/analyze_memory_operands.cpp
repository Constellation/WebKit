#include "A64InstructionTable.h"
#include <cstdio>
#include <set>
#include <string>

using namespace JSC;

int main() {
    printf("Analyzing ALL memory operands in instruction table\n");
    printf("==================================================\n\n");

    std::set<std::string> patterns;

    for (size_t i = 0; i < ARM64Disassembler::g_instructionTableSize; i++) {
        const auto& entry = ARM64Disassembler::g_instructionTable[i];
        
        for (unsigned j = 0; j < entry.operandCount; j++) {
            const auto& op = ARM64Disassembler::g_operandTable[entry.operandOffset + j];
            
            // Memory operand types: 60-64
            if (op.type >= 60 && op.type <= 64) {
                char pattern[200];
                const char* type_name = 
                    op.type == 60 ? "MEMORY_BASE" :
                    op.type == 61 ? "MEMORY_OFFSET" :
                    op.type == 62 ? "MEMORY_REG" :
                    op.type == 63 ? "MEMORY_PREIDX" :
                    "MEMORY_POSTIDX";
                
                snprintf(pattern, sizeof(pattern), 
                        "%-15s %-40s f1[%3u:%u] f2[%3u:%u]",
                        type_name, entry.mnemonic,
                        op.field1_start, op.field1_width,
                        op.field2_start, op.field2_width);
                patterns.insert(pattern);
            }
        }
    }

    printf("Found %zu unique memory operand patterns:\n\n", patterns.size());
    for (const auto& p : patterns) {
        printf("%s\n", p.c_str());
    }

    return 0;
}
