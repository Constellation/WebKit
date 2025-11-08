#!/usr/bin/env python3
"""
Systematic analysis of ARM64 XML instruction patterns
Extracts all operand types, patterns, and formatting requirements
"""

import xml.etree.ElementTree as ET
import os
import sys
from collections import defaultdict, Counter
import re

class OperandPattern:
    def __init__(self, link, hover, text_before, text_after):
        self.link = link
        self.hover = hover
        self.text_before = text_before
        self.text_after = text_after
        self.count = 0

    def __repr__(self):
        return f"{self.link} (hover: {self.hover[:50]}...)"

class ARM64PatternAnalyzer:
    def __init__(self, xml_dir):
        self.xml_dir = xml_dir
        self.patterns = defaultdict(list)
        self.instr_classes = Counter()
        self.operand_links = Counter()
        self.field_names = Counter()

    def analyze_all(self):
        xml_files = [f for f in os.listdir(self.xml_dir) if f.endswith('.xml')]
        print(f"Analyzing {len(xml_files)} XML files...\n")

        for i, xml_file in enumerate(xml_files):
            if (i + 1) % 500 == 0:
                print(f"Processed {i + 1}/{len(xml_files)} files...")

            try:
                self._analyze_file(os.path.join(self.xml_dir, xml_file))
            except Exception as e:
                pass  # Skip errors

        print(f"\nAnalysis complete!")
        self._print_summary()

    def _analyze_file(self, xml_path):
        tree = ET.parse(xml_path)
        root = tree.getroot()

        # Get instruction class
        docvars = root.find('.//docvars')
        if docvars is not None:
            instr_class_elem = docvars.find("./docvar[@key='instr-class']")
            if instr_class_elem is not None:
                self.instr_classes[instr_class_elem.get('value')] += 1

        # Analyze assembly templates
        for asmtemplate in root.findall('.//asmtemplate'):
            self._analyze_template(asmtemplate)

        # Analyze field names
        for regdiagram in root.findall('.//regdiagram'):
            for box in regdiagram.findall('box'):
                name = box.get('name')
                if name and box.get('usename') == '1':
                    self.field_names[name] += 1

    def _analyze_template(self, asmtemplate):
        """Analyze assembly template structure"""
        parts = []
        if asmtemplate.text:
            parts.append(('text', asmtemplate.text))

        for child in asmtemplate:
            if child.tag == 'text' and child.text:
                parts.append(('text', child.text))
            elif child.tag == 'a':
                link = child.get('link', '')
                hover = child.get('hover', '')
                parts.append(('operand', link, hover))

            if child.tail:
                parts.append(('text', child.tail))

        # Extract operand patterns with context
        for i, part in enumerate(parts):
            if part[0] == 'operand':
                link = part[1]
                hover = part[2]

                text_before = parts[i-1][1] if i > 0 and parts[i-1][0] == 'text' else ''
                text_after = parts[i+1][1] if i+1 < len(parts) and parts[i+1][0] == 'text' else ''

                self.operand_links[link] += 1

                # Store pattern
                pattern_key = self._get_pattern_key(link)
                self.patterns[pattern_key].append((link, hover, text_before, text_after))

    def _get_pattern_key(self, link):
        """Categorize operand links into pattern groups"""
        link_lower = link.lower()

        # GP Registers
        if any(p in link_lower for p in ['xd', 'xn', 'xm', 'xt', 'xa', 'xs']):
            return 'GP_REG_X'
        if any(p in link_lower for p in ['wd', 'wn', 'wm', 'wt', 'wa', 'ws']):
            return 'GP_REG_W'

        # FP/SIMD Registers
        if any(p in link_lower for p in ['vd', 'vn', 'vm', 'vt', 'va']):
            return 'SIMD_REG_V'
        if any(p in link_lower for p in ['bd', 'bn', 'bt']):
            return 'FP_REG_B'
        if any(p in link_lower for p in ['hd', 'hn', 'ht']):
            return 'FP_REG_H'
        if any(p in link_lower for p in ['sd', 'sn', 'st']):
            return 'FP_REG_S'
        if any(p in link_lower for p in ['dd', 'dn', 'dt']):
            return 'FP_REG_D'
        if any(p in link_lower for p in ['qd', 'qn', 'qt']):
            return 'FP_REG_Q'

        # SVE
        if any(p in link_lower for p in ['zd', 'zn', 'zm', 'za']):
            return 'SVE_REG_Z'
        if any(p in link_lower for p in ['pd', 'pn', 'pm', 'pg']):
            return 'SVE_REG_P'

        # Immediates
        if 'imm' in link_lower:
            return 'IMMEDIATE'

        # Memory
        if any(p in link_lower for p in ['memory', 'simm', 'address']):
            return 'MEMORY'

        # Options/Modifiers
        if 'option' in link_lower:
            if 'shift' in link_lower:
                return 'SHIFT_OPTION'
            if 'extend' in link_lower:
                return 'EXTEND_OPTION'
            if any(p in link_lower for p in ['r_option', 't_option', 'v_option']):
                return 'SIZE_OPTION'
            return 'OTHER_OPTION'

        # Condition
        if 'cond' in link_lower:
            return 'CONDITION'

        # Label
        if 'label' in link_lower:
            return 'LABEL'

        # Other
        return 'OTHER'

    def _print_summary(self):
        """Print analysis summary"""
        print("\n" + "="*80)
        print("ARM64 INSTRUCTION PATTERN ANALYSIS")
        print("="*80)

        print("\n### INSTRUCTION CLASSES ###")
        for class_name, count in self.instr_classes.most_common(10):
            print(f"  {class_name:20s}: {count:5d}")

        print("\n### OPERAND PATTERN GROUPS ###")
        for pattern_key in sorted(self.patterns.keys()):
            count = len(self.patterns[pattern_key])
            print(f"  {pattern_key:20s}: {count:5d} instances")

        print("\n### TOP 30 MOST COMMON OPERAND LINKS ###")
        for link, count in self.operand_links.most_common(30):
            print(f"  {link:30s}: {count:5d}")

        print("\n### TOP 30 MOST COMMON FIELD NAMES ###")
        for field, count in self.field_names.most_common(30):
            print(f"  {field:20s}: {count:5d}")

        print("\n### DETAILED PATTERN ANALYSIS ###")
        for pattern_key in ['GP_REG_X', 'GP_REG_W', 'SIMD_REG_V', 'FP_REG_S', 'FP_REG_D',
                           'IMMEDIATE', 'SHIFT_OPTION', 'EXTEND_OPTION', 'SIZE_OPTION', 'MEMORY']:
            if pattern_key in self.patterns:
                print(f"\n{pattern_key}:")
                unique_links = set(p[0] for p in self.patterns[pattern_key])
                print(f"  Unique links: {len(unique_links)}")

                # Show examples
                examples = list(unique_links)[:10]
                for ex in examples:
                    print(f"    - {ex}")
                if len(unique_links) > 10:
                    print(f"    ... and {len(unique_links) - 10} more")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <xml_directory>")
        sys.exit(1)

    analyzer = ARM64PatternAnalyzer(sys.argv[1])
    analyzer.analyze_all()
