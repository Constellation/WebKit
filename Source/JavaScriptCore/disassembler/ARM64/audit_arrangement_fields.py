#!/usr/bin/env python3
"""
Comprehensive audit of ALL arrangement field encodings across all XML files.
Maps each T_option/Ta_option/Tb_option to its actual field encoding.
"""

import xml.etree.ElementTree as ET
import os
import sys
from collections import defaultdict
import re

class ArrangementFieldAuditor:
    def __init__(self, xml_directory):
        self.xml_directory = xml_directory
        # Map: arrangement_link -> list of (encodedin, instruction_name, encoding_name)
        self.arrangement_fields = defaultdict(list)
        # Statistics
        self.total_files = 0
        self.files_with_arrangements = 0

    def audit_all(self):
        xml_files = [f for f in os.listdir(self.xml_directory) if f.endswith('.xml')]
        print(f"Auditing arrangement fields in {len(xml_files)} XML files...\n")

        for i, xml_file in enumerate(xml_files):
            if (i + 1) % 500 == 0:
                print(f"Processed {i + 1}/{len(xml_files)} files...")

            self.total_files += 1
            try:
                found_arrangements = self._audit_file(os.path.join(self.xml_directory, xml_file))
                if found_arrangements:
                    self.files_with_arrangements += 1
            except Exception as e:
                pass

        self._print_comprehensive_report()

    def _audit_file(self, xml_path):
        tree = ET.parse(xml_path)
        root = tree.getroot()

        # Get instruction name
        instr_name = os.path.basename(xml_path).replace('.xml', '')

        found_any = False

        # Find all explanations with arrangement-related symbols
        for explanation in root.findall('.//explanation'):
            symbol_elem = explanation.find('.//symbol')
            if symbol_elem is None:
                continue

            symbol_link = symbol_elem.get('link', '')

            # Check if this is an arrangement operand
            if not any(pattern in symbol_link for pattern in ['T_option', 'Ta_option', 'Tb_option', 'Ts_option']):
                continue

            # Get the encodedin attribute
            definition = explanation.find('.//definition[@encodedin]')
            account = explanation.find('.//account[@encodedin]')

            encoding_elem = definition if definition is not None else account
            if encoding_elem is None:
                continue

            encodedin = encoding_elem.get('encodedin', '')

            # Get encoding names this applies to
            enclist = explanation.get('enclist', '')
            encoding_names = [e.strip() for e in enclist.split(',')] if enclist else ['<all>']

            for enc_name in encoding_names:
                self.arrangement_fields[symbol_link].append({
                    'field': encodedin,
                    'instruction': instr_name,
                    'encoding': enc_name
                })
                found_any = True

        return found_any

    def _print_comprehensive_report(self):
        print(f"\n{'=' * 80}")
        print("COMPREHENSIVE ARRANGEMENT FIELD AUDIT")
        print(f"{'=' * 80}\n")

        print(f"Files processed: {self.total_files}")
        print(f"Files with arrangements: {self.files_with_arrangements}")
        print(f"Unique arrangement symbols: {len(self.arrangement_fields)}\n")

        # Group by field encoding pattern
        field_patterns = defaultdict(list)
        for symbol, instances in self.arrangement_fields.items():
            for inst in instances:
                field = inst['field']
                field_patterns[field].append({
                    'symbol': symbol,
                    'instruction': inst['instruction'],
                    'encoding': inst['encoding']
                })

        print(f"{'=' * 80}")
        print("ARRANGEMENT FIELD ENCODINGS (grouped by field pattern)")
        print(f"{'=' * 80}\n")

        # Sort by frequency
        sorted_patterns = sorted(field_patterns.items(), key=lambda x: len(x[1]), reverse=True)

        for field, instances in sorted_patterns:
            print(f"\nField: '{field}' ({len(instances)} uses)")
            print(f"{'-' * 80}")

            # Show examples
            shown = 0
            seen_instructions = set()
            for inst in instances:
                if inst['instruction'] not in seen_instructions and shown < 5:
                    print(f"  {inst['symbol']:25} in {inst['instruction']:30} ({inst['encoding']})")
                    seen_instructions.add(inst['instruction'])
                    shown += 1

            if len(seen_instructions) < len(instances):
                remaining = len(set(i['instruction'] for i in instances)) - len(seen_instructions)
                if remaining > 0:
                    print(f"  ... and {remaining} more instructions")

        # Analyze field patterns to determine bit positions needed
        print(f"\n{'=' * 80}")
        print("FIELD PATTERN ANALYSIS")
        print(f"{'=' * 80}\n")

        print("Field patterns found:")
        for field in sorted(field_patterns.keys()):
            count = len(field_patterns[field])
            print(f"  '{field}' - {count} uses")

        print("\nBit position patterns to support:")
        print("  1. Q (bit 30, width 1) - for half-precision and byte arrangements")
        print("  2. sz:Q (bit 22 + bit 30) - for single/double precision")
        print("  3. size:Q (bits 23-22 + bit 30) - for general SIMD arithmetic")
        print("  4. immh (bits 22-19) - for shift/extend instructions")
        print("  5. immh:Q (bits 22-19 + bit 30) - for source arrangements in shifts")
        print("  6. imm5:Q (bits 20-16 + bit 30) - for DUP and similar")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <xml_directory>")
        sys.exit(1)

    auditor = ArrangementFieldAuditor(sys.argv[1])
    auditor.audit_all()
