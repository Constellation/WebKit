#!/usr/bin/env python3
"""Debug script to trace MOVK parsing"""

import xml.etree.ElementTree as ET
import sys
import re

xml_file = "/Users/yusukesuzuki/dev/ARM64/ISA_A64_xml_A_profile-2024-06/ISA_A64_xml_A_profile-2024-06/movk.xml"

tree = ET.parse(xml_file)
root = tree.getroot()

# Find the 64-bit encoding
for encoding in root.findall('.//encoding[@name="MOVK_64_movewide"]'):
    print(f"Found encoding: {encoding.get('name')}")

    asmtemplate = encoding.find('.//asmtemplate')
    if asmtemplate is None:
        continue

    # Extract parts preserving order
    parts = []
    if asmtemplate.text:
        parts.append(('text', asmtemplate.text))

    for child in asmtemplate:
        if child.tag == 'text' and child.text:
            parts.append(('text', child.text))
        elif child.tag == 'a':
            parts.append(('operand', child.get('link', ''), child.get('hover', '')))
        if child.tail:
            parts.append(('text', child.tail))

    print("\nParts in order:")
    for i, part in enumerate(parts):
        if part[0] == 'text':
            print(f"  {i}: TEXT: {repr(part[1])}")
        else:
            print(f"  {i}: OPERAND: link={part[1]}, hover={part[2][:60]}...")

    # Process operands
    print("\nProcessing operands:")
    operands = []
    for i, part in enumerate(parts):
        if part[0] == 'operand':
            link, hover = part[1], part[2]

            # Extract field names
            field_names = re.findall(r'"([A-Za-z0-9_]+)"', hover)
            primary_field = field_names[0] if field_names else None

            link_lower = link.lower()
            hover_lower = hover.lower()

            print(f"\n  Operand {len(operands)}:")
            print(f"    Link: {link}")
            print(f"    Primary field: {primary_field}")
            print(f"    'imm' in link: {'imm' in link_lower}")
            print(f"    'shift' in link: {'shift__' in link_lower}")

            # Determine type
            op_type = None
            if link_lower.startswith('xd') or 'xzr' in link_lower:
                op_type = "REG_GPR_XZR"
            elif 'imm' in link_lower:
                if 'shift' in hover_lower:
                    op_type = "IMM_SHIFTED"
                elif 'signed' in hover_lower or 'offset' in hover_lower:
                    op_type = "IMM_SINT"
                else:
                    op_type = "IMM_UINT"
            elif link_lower.startswith('shift__'):
                op_type = "IMM_UINT (shift amount)"

            print(f"    Inferred type: {op_type}")
            operands.append((op_type, primary_field))

    print(f"\n\nFinal operand list (in order):")
    for i, (op_type, field) in enumerate(operands):
        print(f"  {i}: {op_type} - field '{field}'")
