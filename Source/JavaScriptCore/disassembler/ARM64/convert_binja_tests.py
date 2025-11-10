#!/usr/bin/env python3
"""
Convert BinaryNinja test cases to our test format
"""

import sys
import re

def parse_test_cases(input_file):
    """Parse BinaryNinja test cases"""
    tests = []

    with open(input_file, 'r') as f:
        for line in f:
            line = line.strip()

            # Skip comments and empty lines
            if line.startswith('//') or not line:
                continue

            # Parse: OPCODE MNEMONIC OPERANDS
            match = re.match(r'^([0-9A-Fa-f]{8})\s+(.+)$', line)
            if not match:
                continue

            opcode_str = match.group(1)
            disasm = match.group(2)

            # Skip SVE/SME instructions (z registers, za registers, p registers)
            if re.search(r'\bz\d+', disasm):
                continue
            if re.search(r'\bza', disasm):
                continue
            if re.search(r'\bp\d+', disasm):
                continue

            # Skip SVE2/SME specific instructions
            sve2_sme_patterns = [
                r'\bst1[bhwd]',  # SVE store
                r'\bld1[bhwdq]',  # SVE load
                r'\bwhile',      # SVE while
                r'\bpsel\b',     # SVE predicate select
                r'\bsmstart\b',  # SME start
                r'\bsmstop\b',   # SME stop
                r'\bmrs\b',      # System register read (incomplete in our disasm)
                r'\bmsr\b',      # System register write (incomplete in our disasm)
                r'\bhint\b',     # Hint instructions
            ]
            if any(re.search(pattern, disasm) for pattern in sve2_sme_patterns):
                continue

            # Skip error cases
            if disasm == 'error':
                continue

            # Convert to our format
            opcode = int(opcode_str, 16)

            tests.append({
                'opcode': opcode,
                'disasm': disasm,
                'hex': opcode_str
            })

    return tests

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input_test_cases.txt> <output.cpp>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    print(f"Parsing {input_file}...")
    tests = parse_test_cases(input_file)
    print(f"Found {len(tests)} non-SVE/SME test cases")

    # Generate test case additions for test_disassembler.cpp
    with open(output_file, 'w') as f:
        f.write("// BinaryNinja ARM64 Test Cases\n")
        f.write("// Auto-generated - add these to test_disassembler.cpp test array\n\n")

        for test in tests:
            # Escape the disasm string
            disasm = test['disasm'].replace('\\', '\\\\').replace('"', '\\"')
            f.write(f"        {{ 0x{test['hex']}, \"BinaryNinja: {disasm}\", \"   {disasm}\", \"BinaryNinja\" }},\n")

    print(f"Generated {output_file} with {len(tests)} test cases")
    print(f"\nTo merge into test_disassembler.cpp:")
    print(f"  1. Copy the generated test cases")
    print(f"  2. Paste them before the closing }}; in test_disassembler.cpp")

if __name__ == '__main__':
    main()
