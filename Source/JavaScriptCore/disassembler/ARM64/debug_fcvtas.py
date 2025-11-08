#!/usr/bin/env python3
"""Debug FCVTAS parsing"""

import xml.etree.ElementTree as ET
import re

xml_file = "/Users/yusukesuzuki/dev/ARM64/ISA_A64_xml_A_profile-2024-06/ISA_A64_xml_A_profile-2024-06/fcvtas_advsimd.xml"

tree = ET.parse(xml_file)
root = tree.getroot()

# Find the asisdmisc_R encoding
for encoding in root.findall('.//encoding[@name="FCVTAS_asisdmisc_R"]'):
    print(f"Found encoding: {encoding.get('name')}\n")

    asmtemplate = encoding.find('.//asmtemplate')
    if asmtemplate is None:
        continue

    print("Template parts:")
    if asmtemplate.text:
        print(f"  TEXT: {repr(asmtemplate.text)}")

    for child in asmtemplate:
        if child.tag == 'a':
            link = child.get('link', '')
            hover = child.get('hover', '')
            print(f"\n  OPERAND:")
            print(f"    link: {link}")
            print(f"    hover: {hover[:80]}...")

            # Simulate parser logic
            link_lower = link.lower()
            hover_lower = hover.lower()

            print(f"    link_lower: {link_lower}")

            # Check what patterns this matches
            patterns_matched = []

            # Check FP register patterns
            if any(p in link_lower for p in ['hd', 'hn']):
                patterns_matched.append("FP halfword (hd/hn)")
            if any(p in link_lower for p in ['sd', 'sn']):
                patterns_matched.append("FP single (sd/sn)")
            if any(p in link_lower for p in ['dd', 'dn']):
                patterns_matched.append("FP double (dd/dn)")
            if any(p in link_lower for p in ['bd', 'bn']):
                patterns_matched.append("FP byte (bd/bn)")

            # Check option patterns
            if 'option' in link_lower:
                patterns_matched.append("Option/specifier")

            # Check single letter
            if len(link_lower) == 1 or re.match(r'^[a-z]__\d+$', link_lower):
                patterns_matched.append("Single letter (likely register number)")

            if patterns_matched:
                print(f"    Matches: {', '.join(patterns_matched)}")
            else:
                print(f"    Matches: NONE - NOT HANDLED!")

        elif child.tag == 'text':
            print(f"  TEXT: {repr(child.text)}")
        if child.tail:
            print(f"  TEXT: {repr(child.tail)}")
