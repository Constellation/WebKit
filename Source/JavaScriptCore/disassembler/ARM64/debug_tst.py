#!/usr/bin/env python3
"""Debug script to trace TST parsing"""

import xml.etree.ElementTree as ET
import re

xml_file = "/Users/yusukesuzuki/dev/ARM64/ISA_A64_xml_A_profile-2024-06/ISA_A64_xml_A_profile-2024-06/tst_ands_log_imm.xml"

tree = ET.parse(xml_file)
root = tree.getroot()

# Find the 32-bit encoding
for encoding in root.findall('.//encoding[@name="TST_ANDS_32S_log_imm"]'):
    print(f"Found encoding: {encoding.get('name')}")

    asmtemplate = encoding.find('.//asmtemplate')
    if asmtemplate is None:
        continue

    # Extract parts
    for child in asmtemplate:
        if child.tag == 'a':
            link = child.get('link', '')
            hover = child.get('hover', '')

            print(f"\nOperand:")
            print(f"  Link: {link}")
            print(f"  Hover: {hover[:80]}...")

            # Extract field names
            field_names = re.findall(r'"([A-Za-z0-9_]+)"', hover)
            print(f"  Field names in quotes: {field_names}")

            # Check patterns
            link_lower = link.lower()
            hover_lower = hover.lower()

            print(f"  'imm' in link: {'imm' in link_lower}")
            print(f"  'logical' in hover: {'logical' in hover_lower}")
            print(f"  'bitmask' in hover: {'bitmask' in hover_lower}")
