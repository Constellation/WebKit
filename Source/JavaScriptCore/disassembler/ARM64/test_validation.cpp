#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    struct TestCase {
        uint32_t opcode;
        const char* mnemonic;
    } tests[] = {
        {0x8b000020, "add"},
        {0xcb000020, "sub"},
        {0x0b000824, "add"},
        {0xd3607c41, "lsl"},
        {0xd340fc41, "lsr"},
        {0x9340fc41, "asr"},
        {0x0e013c00, "umov"},
        {0x4f08a401, "sxtl2"},
        {0x6f08a401, "uxtl2"},
        {0x4e010420, "dup"},
        {0x4ea28420, "add"},
        {0x38601820, "ldrb"},
        {0xf8408423, "ldr"},
        {0xa9007bfd, "stp"},
        {0x1e202000, "fmul"},
        {0x0e20dc00, "fmulx"},
        {0x4e000000, "tbl"},
        {0x0c407000, "ld1"},
        {0x54000000, "b"},
        {0x36000000, "tbz"},
    };
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& test : tests) {
        const auto* entry = findInstruction(test.opcode);
        
        if (!entry) {
            failed++;
            continue;
        }
        
        if (strcmp(entry->mnemonic, test.mnemonic) == 0) {
            char buffer[128];
            uint32_t pc = 0;
            formatInstruction(entry, test.opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));
            printf("✅ 0x%08x: %s\n", test.opcode, buffer);
            passed++;
        } else {
            printf("❌ 0x%08x: Expected '%s', got '%s'\n", 
                   test.opcode, test.mnemonic, entry->mnemonic);
            failed++;
        }
    }
    
    printf("\nPassed: %d/%d\n", passed, passed + failed);
    return failed > 0 ? 1 : 0;
}
