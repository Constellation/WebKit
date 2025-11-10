#\!/usr/bin/env python3
"""Analyze instruction table for optimization opportunities"""

import re
from collections import defaultdict

def analyze_instruction_table():
    """Analyze A64InstructionTable.cpp to find optimization opportunities"""

    with open('A64InstructionTable.cpp', 'r') as f:
        content = f.read()

    # Extract instruction entries
    # Format: { "mnemonic", 0xmask, 0xpattern, offset, count, flags }
    pattern = r'\{\s*"([^"]+)",\s*0x([0-9a-fA-F]+)U,\s*0x([0-9a-fA-F]+)U,'
    entries = re.findall(pattern, content)

    print(f"Total instructions: {len(entries)}")
    print()

    # Analyze by top bits (bits 25-28: major instruction class)
    by_top_bits = defaultdict(list)
    for mnemonic, mask, pattern in entries:
        mask_val = int(mask, 16)
        pattern_val = int(pattern, 16)

        # Extract bits 25-28 if they're fixed
        top_bits_mask = (mask_val >> 25) & 0xF
        if top_bits_mask == 0xF:  # All 4 bits are fixed
            top_bits = (pattern_val >> 25) & 0xF
            by_top_bits[top_bits].append((mnemonic, mask_val, pattern_val))
        else:
            by_top_bits['variable'].append((mnemonic, mask_val, pattern_val))

    print("Distribution by top 4 bits (bits 25-28):")
    for key in sorted(by_top_bits.keys(), key=lambda x: (x == 'variable', x)):
        count = len(by_top_bits[key])
        if key == 'variable':
            print(f"  Variable: {count}")
        else:
            print(f"  0x{key:X}: {count}")
    print()

    # Analyze duplicate patterns (same mask and pattern but different mnemonics)
    pattern_groups = defaultdict(list)
    for mnemonic, mask, pattern in entries:
        mask_val = int(mask, 16)
        pattern_val = int(pattern, 16)
        key = (mask_val, pattern_val)
        pattern_groups[key].append(mnemonic)

    duplicates = {k: v for k, v in pattern_groups.items() if len(v) > 1}
    print(f"Unique mask/pattern combinations: {len(pattern_groups)}")
    print(f"Duplicate combinations: {len(duplicates)}")
    print()

    if duplicates:
        print("Sample duplicates (same mask/pattern, different mnemonics):")
        for i, (key, mnemonics) in enumerate(list(duplicates.items())[:10]):
            mask_val, pattern_val = key
            print(f"  0x{mask_val:08x} / 0x{pattern_val:08x}: {', '.join(mnemonics[:5])}")
            if i >= 9:
                break
    print()

    # Estimate hash table size
    print("Hash table strategy analysis:")
    print(f"  Using bits 25-28 (4 bits, 16 buckets):")
    max_bucket = max(len(v) for k, v in by_top_bits.items() if k \!= 'variable')
    avg_bucket = sum(len(v) for k, v in by_top_bits.items() if k \!= 'variable') / 15
    print(f"    Max bucket size: {max_bucket}")
    print(f"    Avg bucket size: {avg_bucket:.1f}")
    print(f"    Variable (need fallback): {len(by_top_bits.get('variable', []))}")

    # Analyze by bits 21-28 (8 bits, 256 buckets)
    by_top_8bits = defaultdict(int)
    for mnemonic, mask, pattern in entries:
        mask_val = int(mask, 16)
        pattern_val = int(pattern, 16)

        top_8_mask = (mask_val >> 21) & 0xFF
        if top_8_mask == 0xFF:  # All 8 bits are fixed
            top_8 = (pattern_val >> 21) & 0xFF
            by_top_8bits[top_8] += 1

    if by_top_8bits:
        max_8bit_bucket = max(by_top_8bits.values())
        avg_8bit_bucket = sum(by_top_8bits.values()) / len(by_top_8bits)
        coverage = sum(by_top_8bits.values()) / len(entries) * 100
        print(f"\n  Using bits 21-28 (8 bits, 256 buckets):")
        print(f"    Max bucket size: {max_8bit_bucket}")
        print(f"    Avg bucket size: {avg_8bit_bucket:.1f}")
        print(f"    Coverage: {coverage:.1f}% of instructions")
        print(f"    Buckets used: {len(by_top_8bits)}/256")

if __name__ == '__main__':
    analyze_instruction_table()
