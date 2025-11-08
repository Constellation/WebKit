#include <cstdio>
#include <cstdint>

int main() {
    // For "dup v0.8b, v1.b[0]": imm5=00001 (Q=0, byte, index 0)
    uint32_t opcode = 0x0E010420;
    printf("Correct opcode for 'dup v0.8b, v1.b[0]': 0x%08x\n", opcode);
    
    // Check what 0x0E040420 decodes to:
    uint32_t test_opcode = 0x0E040420;
    uint32_t imm5 = (test_opcode >> 16) & 0x1F;
    uint32_t Q = (test_opcode >> 30) & 1;
    
    printf("\nTest opcode 0x%08x:\n", test_opcode);
    printf("  imm5=%u (binary: %05b)\n", imm5, imm5);
    printf("  Q=%u\n", Q);
    printf("  This decodes to: dup v0.2s, v1.s[0]\n");
    
    return 0;
}
