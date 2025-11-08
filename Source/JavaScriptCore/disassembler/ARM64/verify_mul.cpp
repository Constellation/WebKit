#include <cstdio>
#include <cstdint>

int main() {
    // MUL encoding: size field at bits 23-22
    // For 8-bit (B): size=00, Q=0 → 8B
    uint32_t opcode = 0x0E629C20;
    uint32_t size = (opcode >> 22) & 0x3;
    uint32_t Q = (opcode >> 30) & 1;
    
    printf("Opcode 0x%08x:\n", opcode);
    printf("  size=%u, Q=%u\n", size, Q);
    
    if (size == 0) printf("  Element: B, Arrangement: %s\n", Q ? "16B" : "8B");
    else if (size == 1) printf("  Element: H, Arrangement: %s\n", Q ? "8H" : "4H");
    else if (size == 2) printf("  Element: S, Arrangement: %s\n", Q ? "4S" : "2S");
    
    // Correct opcode for MUL v0.8b, v1.8b, v2.8b:
    // size=00, Q=0, Rd=0, Rn=1, Rm=2
    uint32_t correct = 0x0E229C20;
    printf("\nCorrect opcode for 'mul v0.8b, v1.8b, v2.8b': 0x%08x\n", correct);
    
    return 0;
}
