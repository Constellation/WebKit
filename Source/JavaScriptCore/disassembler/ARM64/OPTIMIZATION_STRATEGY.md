# Instruction Table Optimization Strategy

## Current Status
- **Total instructions:** 4,013
- **Current search:** Linear O(n) through all entries
- **Duplicate patterns:** 97 (only 2.7% - minimal compression benefit)

## Analysis Results

### Distribution by Bits 25-28 (Major Instruction Class)
```
Bucket  Count  Percentage
------  -----  ----------
0x0     703    17.5%
0x2     1234   30.8%  ← Largest bucket
0x4     86     2.1%
0x5     50     1.2%
0x6     176    4.4%
0x7     418    10.4%
0x8     26     0.6%
0x9     61     1.5%
0xA     91     2.3%
0xB     21     0.5%
0xC     489    12.2%
0xD     151    3.8%
0xE     125    3.1%
0xF     380    9.5%
Var     2      0.0%
```

## Optimization Strategy

### Option 1: Two-Level Hash (Bits 25-28) - **RECOMMENDED**

**Structure:**
```cpp
// Level 1: 16 buckets indexed by bits 25-28
struct Bucket {
    const InstructionEntry* start;
    uint16_t count;
};

const Bucket g_instructionBuckets[17] = {
    { &g_instructionTable[0], 703 },      // 0x0
    { nullptr, 0 },                        // 0x1 (empty)
    { &g_instructionTable[703], 1234 },   // 0x2
    // ...
    { &g_instructionTable[4011], 2 },     // Variable/fallback
};
```

**Lookup algorithm:**
```cpp
const InstructionEntry* findInstruction(uint32_t opcode) {
    // Extract bits 25-28 (major instruction class)
    unsigned index = (opcode >> 25) & 0xF;

    // Check primary bucket
    const auto& bucket = g_instructionBuckets[index];
    for (unsigned i = 0; i < bucket.count; i++) {
        const auto& entry = bucket.start[i];
        if ((opcode & entry.mask) == entry.pattern)
            return &entry;
    }

    // Fallback: Check variable bucket (only 2 instructions)
    const auto& fallback = g_instructionBuckets[16];
    for (unsigned i = 0; i < fallback.count; i++) {
        const auto& entry = fallback.start[i];
        if ((opcode & entry.mask) == entry.pattern)
            return &entry;
    }

    return nullptr;
}
```

**Performance:**
- **Average case:** ~250 comparisons (vs 4013 linear)
- **Worst case:** ~1234 comparisons (bucket 0x2)
- **Speedup:** ~15x average, ~3x worst case
- **Memory overhead:** 17 * (8 bytes + 2 bytes) = 170 bytes

### Option 2: Fine-Grained Hash (Bits 21-28)

**Structure:**
```cpp
// 256 buckets indexed by bits 21-28
const Bucket g_instructionBuckets[257];  // +1 for fallback
```

**Performance:**
- **Average case:** ~15 comparisons
- **Speedup:** ~250x average
- **Memory overhead:** 257 * 10 bytes = 2.5 KB

**Trade-off:** More memory but much faster

### Option 3: Hybrid Approach

For frequently-used instructions, add a fast path:

```cpp
const InstructionEntry* findInstruction(uint32_t opcode) {
    // Fast path for common patterns (top 20 instructions)
    // These cover ~80% of real code
    switch (opcode & 0xFF000000) {  // Top 8 bits
    case 0xD1000000:  // SUB immediate
    case 0x91000000:  // ADD immediate
    case 0xF8000000:  // LDR/STR with offset
    // ... (check exact match inline)
        // Direct check without loop
        if ((opcode & 0xFFC00000U) == 0xF9400000U)
            return &g_commonInstructions[0];  // LDR X
        break;
    }

    // Fall through to hash table
    return findInstructionHashTable(opcode);
}
```

## Implementation Plan

### Phase 1: Generate Bucket Table (Recommended)
1. Modify `CodeGenerator._generate_implementation()` to:
   - Sort instructions by bits 25-28
   - Generate bucket index table
   - Update `findInstruction()` to use two-level lookup

2. Expected code changes:
   - Add bucket structure (10 lines)
   - Add bucket table generation (50 lines)
   - Update finder (20 lines)

### Phase 2: Optional Fine-Tuning
1. Profile real disassembly workloads
2. Add fast path for hot instructions if needed
3. Consider bits 21-28 if buckets 25-28 are still too large

## Estimated Impact

### Current (Linear Search)
- **Average instructions checked:** 2,006 (half of 4013)
- **Cache misses:** High (large table, sequential scan)

### With Two-Level Hash (Bits 25-28)
- **Average instructions checked:** ~125 (half of avg bucket ~250)
- **Speedup:** ~16x
- **Code size increase:** ~300 bytes
- **Complexity:** Low (simple addition)

### With Fine Hash (Bits 21-28)
- **Average instructions checked:** ~7-8
- **Speedup:** ~250x
- **Code size increase:** ~2.5 KB
- **Complexity:** Low

## Recommendation

**Start with Option 1 (Bits 25-28):**
- Easy to implement
- Significant speedup (16x)
- Minimal memory overhead (170 bytes)
- Low risk
- Can upgrade to Option 2 later if needed

**Implementation priority:**
1. Generate bucket table by bits 25-28
2. Update findInstruction() for two-level lookup
3. Test and measure
4. Optionally upgrade to bits 21-28 if profiling shows benefit
