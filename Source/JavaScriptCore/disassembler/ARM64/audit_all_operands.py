#!/usr/bin/env python3
"""
Comprehensive operand audit tool for ARM64 XML files.
Systematically checks ALL operand patterns to ensure parser coverage.
"""

import xml.etree.ElementTree as ET
import os
import sys
from collections import defaultdict
import re

class OperandAuditor:
    def __init__(self, xml_directory):
        self.xml_directory = xml_directory
        self.operand_patterns = defaultdict(list)
        self.link_patterns = defaultdict(int)
        self.special_patterns = defaultdict(list)

    def audit_all(self):
        xml_files = [f for f in os.listdir(self.xml_directory) if f.endswith('.xml')]
        print(f"Auditing {len(xml_files)} XML files for operand patterns...\n")

        for i, xml_file in enumerate(xml_files):
            if (i + 1) % 500 == 0:
                print(f"Processed {i + 1}/{len(xml_files)} files...")

            try:
                self._audit_file(os.path.join(self.xml_directory, xml_file))
            except Exception as e:
                pass

        self._print_report()

    def _audit_file(self, xml_path):
        tree = ET.parse(xml_path)
        root = tree.getroot()

        # Get all asmtemplate elements
        for asmtemplate in root.findall('.//asmtemplate'):
            template_text = self._extract_template(asmtemplate)

            # Check for special patterns
            if '{' in template_text and '}' in template_text:
                # Register list pattern
                list_match = re.search(r'\{[^}]+\}', template_text)
                if list_match:
                    self.special_patterns['register_lists'].append({
                        'file': os.path.basename(xml_path),
                        'pattern': list_match.group(0)
                    })

            # Check for hardcoded arrangements
            hardcoded = re.findall(r'\.\d+[BHSDQ]', template_text)
            if hardcoded:
                for arr in hardcoded:
                    self.special_patterns['hardcoded_arrangements'].append({
                        'file': os.path.basename(xml_path),
                        'arrangement': arr
                    })

            # Check for register sequences
            if '+' in template_text:
                seq_match = re.findall(r'<[^>]*\+\d+[^>]*>', template_text)
                for seq in seq_match:
                    self.special_patterns['register_sequences'].append({
                        'file': os.path.basename(xml_path),
                        'sequence': seq
                    })

            # Extract all operand links
            for operand in asmtemplate.findall('.//a'):
                link = operand.get('link', '')
                hover = operand.get('hover', '')

                if link:
                    self.link_patterns[link] += 1

                    # Store examples
                    if len(self.operand_patterns[link]) < 3:
                        self.operand_patterns[link].append({
                            'file': os.path.basename(xml_path),
                            'hover': hover[:100]
                        })

    def _extract_template(self, asmtemplate):
        """Extract full template text including all children"""
        parts = []
        if asmtemplate.text:
            parts.append(asmtemplate.text)

        for child in asmtemplate:
            if child.tag == 'text' and child.text:
                parts.append(child.text)
            elif child.tag == 'a':
                link = child.get('link', '')
                parts.append(f'<{link}>')
            if child.tail:
                parts.append(child.tail)

        return ''.join(parts)

    def _print_report(self):
        print(f"\n{'=' * 80}")
        print("OPERAND AUDIT REPORT")
        print(f"{'=' * 80}\n")

        # Special patterns that need handling
        print(f"SPECIAL PATTERNS REQUIRING CUSTOM HANDLING")
        print(f"{'-' * 80}")

        print(f"\n1. REGISTER LISTS (in curly braces):")
        print(f"   Found {len(self.special_patterns['register_lists'])} instances")
        # Show unique patterns
        unique_lists = {}
        for item in self.special_patterns['register_lists']:
            pattern = item['pattern']
            if pattern not in unique_lists:
                unique_lists[pattern] = item['file']

        for pattern, file in sorted(unique_lists.items())[:10]:
            print(f"   {pattern:40} ({file})")

        print(f"\n2. HARDCODED ARRANGEMENTS (like .16B, .8B):")
        print(f"   Found {len(self.special_patterns['hardcoded_arrangements'])} instances")
        # Count by arrangement
        arr_counts = defaultdict(int)
        for item in self.special_patterns['hardcoded_arrangements']:
            arr_counts[item['arrangement']] += 1

        for arr, count in sorted(arr_counts.items(), key=lambda x: -x[1])[:15]:
            print(f"   {arr:10} - {count:4} occurrences")

        print(f"\n3. REGISTER SEQUENCES (like <Vn+1>, <Rn+2>):")
        print(f"   Found {len(self.special_patterns['register_sequences'])} instances")
        # Show unique sequences
        unique_seqs = {}
        for item in self.special_patterns['register_sequences']:
            seq = item['sequence']
            if seq not in unique_seqs:
                unique_seqs[seq] = item['file']

        for seq, file in sorted(unique_seqs.items())[:10]:
            print(f"   {seq:40} ({file})")

        # Top operand link patterns
        print(f"\n{'-' * 80}")
        print(f"TOP 50 OPERAND LINK PATTERNS (by frequency)")
        print(f"{'-' * 80}")

        sorted_links = sorted(self.link_patterns.items(), key=lambda x: -x[1])

        for i, (link, count) in enumerate(sorted_links[:50], 1):
            examples = self.operand_patterns.get(link, [])
            example_file = examples[0]['file'] if examples else 'N/A'
            print(f"{i:3}. {link:35} - {count:5} times (e.g., {example_file})")

        # Categorize links
        print(f"\n{'-' * 80}")
        print(f"OPERAND CATEGORIES")
        print(f"{'-' * 80}")

        categories = {
            'GP Registers (X/W)': [],
            'SIMD Registers (V/Q/D/S/H/B)': [],
            'SVE Registers (Z/P)': [],
            'Immediates': [],
            'Labels': [],
            'Conditions': [],
            'Shift/Extend': [],
            'Arrangements': [],
            'Memory': [],
            'Other': []
        }

        for link in self.link_patterns.keys():
            link_lower = link.lower()

            if any(x in link_lower for x in ['xd', 'xn', 'xm', 'wd', 'wn', 'wm', 'xzr', 'wzr', 'xsp', 'wsp']):
                categories['GP Registers (X/W)'].append(link)
            elif any(x in link_lower for x in ['vd', 'vn', 'vm', 'vt', 'qd', 'qn', 'dd', 'dn', 'sd', 'sn', 'hd', 'hn', 'bd', 'bn']):
                categories['SIMD Registers (V/Q/D/S/H/B)'].append(link)
            elif any(x in link_lower for x in ['zd', 'zn', 'zm', 'pd', 'pn', 'pm', 'pg']):
                categories['SVE Registers (Z/P)'].append(link)
            elif 'imm' in link_lower or 'hw' in link_lower:
                categories['Immediates'].append(link)
            elif 'label' in link_lower or 'offset' in link_lower:
                categories['Labels'].append(link)
            elif 'cond' in link_lower:
                categories['Conditions'].append(link)
            elif 'shift' in link_lower or 'extend' in link_lower:
                categories['Shift/Extend'].append(link)
            elif 'option' in link_lower and ('ta' in link_lower or 'tb' in link_lower or 't_' in link_lower):
                categories['Arrangements'].append(link)
            elif 'memory' in link_lower or link_lower.startswith('mem_'):
                categories['Memory'].append(link)
            else:
                categories['Other'].append(link)

        for category, links in categories.items():
            if links:
                print(f"\n{category}: {len(links)} unique patterns")
                for link in sorted(set(links))[:10]:
                    count = self.link_patterns[link]
                    print(f"  - {link:40} ({count} times)")
                if len(links) > 10:
                    print(f"  ... and {len(links) - 10} more")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <xml_directory>")
        sys.exit(1)

    auditor = OperandAuditor(sys.argv[1])
    auditor.audit_all()
