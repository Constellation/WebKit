#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

using namespace JSC;

int main() {
    printf("Analyzing ALL SHIFT_TYPE operands in instruction table\n");
    printf("=======================================================\n\n");

    std::set<std::string> patterns;

    for (size_t i = 0; i < ARM64Disassembler::g_instructionTableSize; i++) {
        const auto& entry = ARM64Disassembler::g_instructionTable[i];
        
        for (unsigned j = 0; j < entry.operandCount; j++) {
            const auto& op = ARM64Disassembler::g_operandTable[entry.operandOffset + j];
            
            if (op.type == 51) { // SHIFT_TYPE
                char pattern[200];
                snprintf(pattern, sizeof(pattern), 
                        "%-40s f1[%3u:%u] f2[%3u:%u]",
                        entry.mnemonic,
                        op.field1_start, op.field1_width,
                        op.field2_start, op.field2_width);
                patterns.insert(pattern);
            }
        }
    }

    printf("Found %zu unique SHIFT_TYPE patterns:\n\n", patterns.size());
    for (const auto& p : patterns) {
        printf("%s\n", p.c_str());
    }

    return 0;
}
