#!/usr/bin/env python3
"""
Exhaustive analysis of Q-only arrangement patterns.
For each instruction using Q-only arrangement, extract its opcode pattern
to understand how to distinguish different element sizes.
"""

import xml.etree.ElementTree as ET
import os
import sys
from collections import defaultdict
import re

class QOnlyAnalyzer:
    def __init__(self, xml_directory):
        self.xml_directory = xml_directory
        # Map: instruction_name -> list of (encoding_name, opcode_pattern, mask_pattern)
        self.q_only_instructions = defaultdict(list)

    def analyze_all(self):
        xml_files = [f for f in os.listdir(self.xml_directory) if f.endswith('.xml')]
        print(f"Analyzing Q-only arrangements in {len(xml_files)} XML files...\n")

        for i, xml_file in enumerate(xml_files):
            if (i + 1) % 500 == 0:
                print(f"Processed {i + 1}/{len(xml_files)} files...")

            try:
                self._analyze_file(os.path.join(self.xml_directory, xml_file))
            except Exception as e:
                pass

        self._print_comprehensive_report()

    def _analyze_file(self, xml_path):
        tree = ET.parse(xml_path)
        root = tree.getroot()

        instr_name = os.path.basename(xml_path).replace('.xml', '')

        # Find explanations with Q-only arrangement
        q_only_encodings = set()
        for explanation in root.findall('.//explanation'):
            symbol_elem = explanation.find('.//symbol')
            if symbol_elem is None:
                continue

            symbol_link = symbol_elem.get('link', '')
            if not any(p in symbol_link for p in ['T_option', 'Ta_option', 'Tb_option', 'Ts_option']):
                continue

            # Check if encodedin is just "Q"
            definition = explanation.find('.//definition[@encodedin]')
            account = explanation.find('.//account[@encodedin]')
            encoding_elem = definition if definition is not None else account

            if encoding_elem is None:
                continue

            encodedin = encoding_elem.get('encodedin', '')
            if encodedin.strip().upper() == 'Q':
                # This is Q-only arrangement
                enclist = explanation.get('enclist', '')
                if enclist:
                    for enc in enclist.split(','):
                        q_only_encodings.add(enc.strip())

        if not q_only_encodings:
            return

        # For each Q-only encoding, extract opcode pattern
        for iclass in root.findall('.//iclass'):
            regdiagram = iclass.find('regdiagram')
            if regdiagram is None:
                continue

            for encoding in iclass.findall('.//encoding'):
                enc_name = encoding.get('name', '')
                if enc_name not in q_only_encodings:
                    continue

                # Parse the regdiagram to get fixed bit patterns
                opcode_info = self._extract_opcode_pattern(regdiagram, encoding)
                if opcode_info:
                    self.q_only_instructions[instr_name].append({
                        'encoding': enc_name,
                        'pattern': opcode_info['pattern'],
                        'mask': opcode_info['mask'],
                        'bits': opcode_info['bits']
                    })

    def _extract_opcode_pattern(self, regdiagram, encoding):
        """Extract fixed bit pattern from regdiagram"""
        # Build 32-bit pattern
        bits = {}

        for box in regdiagram.findall('box'):
            hibit = int(box.get('hibit', '0'))
            width = int(box.get('width', '1')) if box.get('width') else 1
            name = box.get('name', '')
            settings = box.get('settings')

            bit_start = hibit - width + 1

            if settings:
                # Fixed bits
                fixed_bits = []
                for child in box:
                    if child.tag == 'c' and child.text and child.text.strip():
                        fixed_bits.append(child.text.strip())

                if fixed_bits:
                    binary_str = ''.join(fixed_bits).replace('x', '0')
                    if all(c in '01x' for c in binary_str):
                        try:
                            value = int(binary_str.replace('x', '0'), 2)
                            for b in range(width):
                                bits[bit_start + b] = ('1' if (value >> b) & 1 else '0')
                        except:
                            pass

        # Build pattern and mask
        pattern = 0
        mask = 0
        for bit_pos, bit_val in bits.items():
            if bit_val in ['0', '1']:
                mask |= (1 << bit_pos)
                if bit_val == '1':
                    pattern |= (1 << bit_pos)

        return {
            'pattern': pattern,
            'mask': mask,
            'bits': bits
        }

    def _print_comprehensive_report(self):
        print(f"\n{'=' * 80}")
        print("Q-ONLY ARRANGEMENT COMPREHENSIVE ANALYSIS")
        print(f"{'=' * 80}\n")

        print(f"Total instructions using Q-only arrangement: {len(self.q_only_instructions)}\n")

        # Group by bit patterns in key ranges
        print(f"{'=' * 80}")
        print("CATEGORIZATION BY OPCODE BITS")
        print(f"{'=' * 80}\n")

        # Key bit ranges to check:
        # - Bits 31-24: Instruction class
        # - Bits 23-21: Often size/type indicators
        # - Bits 15-12: Opcode bits
        # - Bits 11-10: Sometimes size indicators

        categories = defaultdict(list)
        for instr_name, encodings in self.q_only_instructions.items():
            for enc_info in encodings:
                pattern = enc_info['pattern']

                # Extract key bit ranges
                bits31_24 = (pattern >> 24) & 0xFF
                bits23_21 = (pattern >> 21) & 0x7
                bits15_12 = (pattern >> 12) & 0xF
                bits11_10 = (pattern >> 10) & 0x3

                category_key = (bits31_24, bits23_21, bits15_12)
                categories[category_key].append({
                    'instr': instr_name,
                    'encoding': enc_info['encoding'],
                    'pattern': f"0x{pattern:08X}",
                    'bits11_10': bits11_10
                })

        # Print categories
        print("Categories found (by bits[31:24], bits[23:21], bits[15:12]):\n")
        for i, (cat_key, instructions) in enumerate(sorted(categories.items(), key=lambda x: len(x[1]), reverse=True)):
            bits31_24, bits23_21, bits15_12 = cat_key
            print(f"Category {i+1}: bits[31:24]=0x{bits31_24:02X}, bits[23:21]={bits23_21:03b}, bits[15:12]={bits15_12:04b}")
            print(f"  Count: {len(instructions)} encodings")

            # Show first 5 examples
            shown = 0
            for inst in instructions[:5]:
                print(f"    {inst['instr']:30} ({inst['encoding']}) - pattern={inst['pattern']}")
                shown += 1

            if len(instructions) > 5:
                print(f"    ... and {len(instructions) - 5} more")
            print()

        # Detailed analysis: what distinguishes different element sizes?
        print(f"\n{'=' * 80}")
        print("ELEMENT SIZE INFERENCE ANALYSIS")
        print(f"{'=' * 80}\n")

        print("Key observations for Q-only instructions:")
        print("1. FP16 (half-precision): bits[23:21] = 010 (e.g., FMUL .4H/.8H)")
        print("2. Table ops (TBL/TBX): bits[15:12] = 0000 or 0001, byte elements only")
        print("3. DOT products: bits[23:21] specific patterns, always use smallest element")
        print("4. Some instructions: fixed element size, Q only controls vector width\n")

        # Look for patterns with bits 22-23 or 11-10
        print("Instructions using bits[23:22] or bits[11:10] for size:\n")
        for instr_name, encodings in sorted(self.q_only_instructions.items())[:20]:
            for enc_info in encodings:
                bits = enc_info['bits']
                # Check if bits 22-23 or 11-10 are variable
                if 22 not in bits or 23 not in bits:
                    print(f"  {instr_name:30} - bits[23:22] variable")
                if 10 not in bits or 11 not in bits:
                    print(f"  {instr_name:30} - bits[11:10] variable")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <xml_directory>")
        sys.exit(1)

    analyzer = QOnlyAnalyzer(sys.argv[1])
    analyzer.analyze_all()
