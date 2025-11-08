#!/usr/bin/env python3
"""
Check which operand patterns are currently handled by the parser
"""

import re

# Read the parser code
with open('generate_arm64_disassembler.py', 'r') as f:
    parser_code = f.read()

# Extract all link pattern checks from the parser
def find_pattern_checks(code):
    """Find all link pattern checks in the parser"""
    checks = set()

    # Find all string literals in if statements
    # Pattern 1: link_lower.startswith(...)
    for match in re.finditer(r"link_lower\.startswith\(['\"]([^'\"]+)", code):
        checks.add(match.group(1))

    # Pattern 2: 'pattern' in link_lower
    for match in re.finditer(r"['\"]([a-z_]+)['\"] in link_lower", code):
        checks.add(match.group(1))

    # Pattern 3: link_lower == 'pattern'
    for match in re.finditer(r"link_lower == ['\"]([^'\"]+)['\"]", code):
        checks.add(match.group(1))

    # Pattern 4: any(... for p in ['list'])
    for match in re.finditer(r"for p in \[([^\]]+)\]", code):
        items_str = match.group(1)
        items = re.findall(r"['\"]([^'\"]+)['\"]", items_str)
        checks.update(items)

    return checks

handled_patterns = find_pattern_checks(parser_code)

print("=" * 80)
print("CURRENTLY HANDLED PATTERNS IN PARSER")
print("=" * 80)
print(f"Total: {len(handled_patterns)} patterns\n")

# Categorize
categories = {
    'GP Registers': [],
    'FP/SIMD Registers': [],
    'SVE Registers': [],
    'Immediates': [],
    'Labels': [],
    'Conditions': [],
    'Shifts/Extends': [],
    'Memory': [],
    'Options': [],
    'Other': []
}

for pattern in sorted(handled_patterns):
    if any(p in pattern for p in ['xd', 'xn', 'xm', 'wd', 'wn', 'wm', 'xt', 'wt', 'xa', 'wa', 'xs', 'ws']):
        categories['GP Registers'].append(pattern)
    elif any(p in pattern for p in ['bd', 'hd', 'sd', 'dd', 'qd', 'vd', 'vn', 'vm']):
        categories['FP/SIMD Registers'].append(pattern)
    elif any(p in pattern for p in ['zd', 'zn', 'zm', 'za', 'pd', 'pn', 'pm', 'pg']):
        categories['SVE Registers'].append(pattern)
    elif 'imm' in pattern or 'amount' in pattern or 'shift' in pattern:
        categories['Immediates'].append(pattern)
    elif 'label' in pattern:
        categories['Labels'].append(pattern)
    elif 'cond' in pattern:
        categories['Conditions'].append(pattern)
    elif 'extend' in pattern or 'shift' in pattern:
        categories['Shifts/Extends'].append(pattern)
    elif 'memory' in pattern:
        categories['Memory'].append(pattern)
    elif 'option' in pattern:
        categories['Options'].append(pattern)
    else:
        categories['Other'].append(pattern)

for category, patterns in categories.items():
    if patterns:
        print(f"\n{category}:")
        for pattern in patterns:
            print(f"  - {pattern}")

print("\n" + "=" * 80)
print("NOTABLE MISSING PATTERNS (from XML analysis)")
print("=" * 80)

# Common patterns from XML that we should handle
common_missing = [
    ('sa_*', 'SVE register operands with sa_ prefix', 'HIGH'),
    ('V-prefixed', 'SIMD vector registers (Vd, Vn, Vm, Vt)', 'HIGH'),
    ('*_option patterns', 'Size/type specifiers (T_option, V_option, etc.)', 'MEDIUM'),
    ('sa_imm', 'SVE immediate values', 'MEDIUM'),
    ('Multiple Vt', 'Vector register lists (Vt2, Vt3, Vt4)', 'MEDIUM'),
    ('Index patterns', 'Element indices in SIMD operations', 'LOW'),
]

print("\nPriority patterns to add:\n")
for pattern, description, priority in common_missing:
    print(f"[{priority:6s}] {pattern:20s} - {description}")

print("\n" + "=" * 80)
