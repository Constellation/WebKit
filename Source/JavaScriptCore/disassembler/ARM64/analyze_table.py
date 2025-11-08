#!/usr/bin/env python3
"""
Analyze generated instruction table to find instructions with missing/incorrect operands
"""

import re

# Read the generated table
with open('A64InstructionTable.cpp', 'r') as f:
    table_content = f.read()

# Extract instruction entries
instruction_pattern = r'\{\s*"([^"]+)",\s*"([^"]+)",\s*0x([0-9a-fA-F]+)U,\s*0x([0-9a-fA-F]+)U,\s*(\d+),\s*(\d+),\s*(\d+)\s*\}'

instructions = []
for match in re.finditer(instruction_pattern, table_content):
    name, mnemonic, mask, pattern, operand_offset, operand_count, flags = match.groups()
    instructions.append({
        'name': name,
        'mnemonic': mnemonic,
        'mask': mask,
        'pattern': pattern,
        'operand_offset': int(operand_offset),
        'operand_count': int(operand_count),
        'flags': int(flags)
    })

print(f"Total instructions: {len(instructions)}\n")

# Group by operand count
by_count = {}
for instr in instructions:
    count = instr['operand_count']
    if count not in by_count:
        by_count[count] = []
    by_count[count].append(instr)

print("=" * 80)
print("INSTRUCTIONS BY OPERAND COUNT")
print("=" * 80)
for count in sorted(by_count.keys()):
    print(f"\n{count} operands: {len(by_count[count])} instructions")
    if count == 0:
        # Show examples of instructions with no operands (likely parsing failures)
        print("  Examples (likely missing operand detection):")
        for instr in by_count[count][:20]:
            print(f"    - {instr['mnemonic']:10s} ({instr['name']})")
        if len(by_count[count]) > 20:
            print(f"    ... and {len(by_count[count]) - 20} more")

# Find common patterns in zero-operand instructions
zero_operands = by_count.get(0, [])
print(f"\n\n=" * 40)
print(f"ANALYSIS OF {len(zero_operands)} INSTRUCTIONS WITH ZERO OPERANDS")
print("=" * 80)

# Group by mnemonic prefix
prefixes = {}
for instr in zero_operands:
    prefix = instr['mnemonic'][:3]
    if prefix not in prefixes:
        prefixes[prefix] = []
    prefixes[prefix].append(instr)

print("\nBy mnemonic prefix (top 20):")
for prefix in sorted(prefixes.keys(), key=lambda p: len(prefixes[p]), reverse=True)[:20]:
    print(f"  {prefix}*: {len(prefixes[prefix]):4d} instructions")

# Check if these are likely SVE/advanced SIMD
sve_count = sum(1 for i in zero_operands if any(x in i['name'].lower() for x in ['sve', 'sme', '_z_', '_p_']))
simd_count = sum(1 for i in zero_operands if any(x in i['name'].lower() for x in ['advsimd', 'simd', '_v']))

print(f"\nLikely SVE/SME instructions: {sve_count}")
print(f"Likely SIMD instructions: {simd_count}")
print(f"Other: {len(zero_operands) - sve_count - simd_count}")

print("\n" + "=" * 80)
print("RECOMMENDATIONS")
print("=" * 80)
print("""
1. Focus on non-SVE instructions with zero operands first
2. SVE/SME instructions (sa_* patterns) are less critical for JavaScript
3. Basic SIMD (NEON) instructions should have operands parsed

Next steps:
- Extract XML for common zero-operand instructions
- Identify missing link patterns
- Add parser support for those patterns
""")
