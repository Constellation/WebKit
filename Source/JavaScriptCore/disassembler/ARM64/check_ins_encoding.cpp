#include <cstdio>
#include <cstdint>

int main() {
    uint32_t opcode = 0x4e081c01;
    uint32_t imm5 = (opcode >> 16) & 0x1F;
    
    printf("Opcode: 0x%08x\n", opcode);
    printf("imm5: 0x%02x (binary: %05b)\n", imm5, imm5);
    printf("Pattern: x1000 (bit 3 set)\n");
    printf("Element: D\n");
    printf("Register: X (should be X, not W)\n");
    
    return 0;
}
