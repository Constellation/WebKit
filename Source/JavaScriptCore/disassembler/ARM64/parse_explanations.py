#!/usr/bin/env python3
"""
Parse explanation sections from ARM64 XML to extract operand field encodings.
This helps understand how each operand symbol is actually encoded.
"""

import xml.etree.ElementTree as ET
import os
import sys
from collections import defaultdict
import re

class ExplanationParser:
    def __init__(self, xml_directory):
        self.xml_directory = xml_directory
        self.operand_encodings = defaultdict(list)  # symbol -> list of encodings
        self.missing_encodings = defaultdict(int)   # symbols without encoding info

    def parse_all(self):
        xml_files = [f for f in os.listdir(self.xml_directory) if f.endswith('.xml')]
        print(f"Parsing explanations from {len(xml_files)} XML files...\n")

        for i, xml_file in enumerate(xml_files):
            if (i + 1) % 500 == 0:
                print(f"Processed {i + 1}/{len(xml_files)} files...")

            try:
                self._parse_file(os.path.join(self.xml_directory, xml_file))
            except Exception as e:
                pass

        self._print_report()

    def _parse_file(self, xml_path):
        tree = ET.parse(xml_path)
        root = tree.getroot()

        # Find all explanation elements
        for explanation in root.findall('.//explanation'):
            symbol_elem = explanation.find('.//symbol')
            if symbol_elem is None:
                continue

            symbol_link = symbol_elem.get('link', '')
            symbol_text = ''.join(symbol_elem.itertext()).strip()

            # Check for account/definition with encodedin attribute
            account = explanation.find('.//account[@encodedin]')
            definition = explanation.find('.//definition[@encodedin]')

            encoding_elem = account if account is not None else definition
            if encoding_elem is not None:
                encodedin = encoding_elem.get('encodedin', '')

                # Check if there's a table defining the encoding
                table = encoding_elem.find('.//table')
                table_info = None
                if table is not None:
                    # Extract table mapping
                    rows = table.findall('.//row')
                    if rows:
                        table_info = {
                            'field': encodedin,
                            'mappings': []
                        }
                        for row in rows:
                            bitfield = row.find('.//entry[@class="bitfield"]')
                            symbol = row.find('.//entry[@class="symbol"]')
                            if bitfield is not None and symbol is not None:
                                bf_text = ''.join(bitfield.itertext()).strip()
                                sym_text = ''.join(symbol.itertext()).strip()
                                table_info['mappings'].append((bf_text, sym_text))

                self.operand_encodings[symbol_link].append({
                    'symbol': symbol_text,
                    'field': encodedin,
                    'file': os.path.basename(xml_path),
                    'table': table_info
                })
            else:
                # Symbol has no encoding information
                self.missing_encodings[symbol_link] += 1

    def _print_report(self):
        print(f"\n{'=' * 80}")
        print("OPERAND ENCODING REPORT")
        print(f"{'=' * 80}\n")

        print(f"Found encoding information for {len(self.operand_encodings)} unique operand symbols")
        print(f"Missing encoding information for {len(self.missing_encodings)} symbols\n")

        # Focus on arrangement-related operands
        print(f"{'=' * 80}")
        print("ARRANGEMENT OPERAND ENCODINGS")
        print(f"{'=' * 80}\n")

        arrangement_patterns = ['Ta_option', 'Tb_option', 'T_option', 'Ts_option']
        for pattern in arrangement_patterns:
            matching = [k for k in self.operand_encodings.keys() if pattern in k]
            if matching:
                print(f"\n{pattern} variants ({len(matching)} total):")
                for symbol in sorted(matching)[:10]:
                    encodings = self.operand_encodings[symbol]
                    if encodings:
                        enc = encodings[0]
                        field = enc['field']
                        print(f"  {symbol:30} -> {field:20} ({enc['file']})")
                        if enc['table'] and len(enc['table']['mappings']) <= 5:
                            for bf, sym in enc['table']['mappings']:
                                print(f"    {bf:10} = {sym}")
                if len(matching) > 10:
                    print(f"  ... and {len(matching) - 10} more")

        # Show some common operand types
        print(f"\n{'=' * 80}")
        print("COMMON OPERAND ENCODINGS")
        print(f"{'=' * 80}\n")

        common_ops = ['Vd', 'Vn', 'Vm', 'Vt', 'Xd', 'Xn', 'Wn', 'Rd', 'Rn', 'Rm']
        for op in common_ops:
            if op in self.operand_encodings:
                encodings = self.operand_encodings[op]
                if encodings:
                    enc = encodings[0]
                    print(f"{op:10} -> {enc['field']:20} ({len(encodings)} files)")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <xml_directory>")
        sys.exit(1)

    parser = ExplanationParser(sys.argv[1])
    parser.parse_all()
