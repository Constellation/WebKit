#!/usr/bin/env python3
"""
Analyze alias precedence patterns across all ARM64 XML files.
Goal: Understand alias relationships and design a systematic precedence system.
"""

import xml.etree.ElementTree as ET
import os
import sys
from collections import defaultdict
import re

class AliasAnalyzer:
    def __init__(self, xml_directory):
        self.xml_directory = xml_directory
        self.aliases = []  # List of (base_instruction, alias_name, alias_condition, xml_file)
        self.base_instructions = defaultdict(list)  # base -> list of aliases

    def analyze_all(self):
        xml_files = [f for f in os.listdir(self.xml_directory) if f.endswith('.xml')]
        print(f"Analyzing {len(xml_files)} XML files for alias patterns...\n")

        for xml_file in xml_files:
            try:
                self._analyze_file(os.path.join(self.xml_directory, xml_file))
            except Exception as e:
                pass

        self._print_analysis()

    def _analyze_file(self, xml_path):
        tree = ET.parse(xml_path)
        root = tree.getroot()

        # Check if this is an alias
        root_type = root.get('type')
        if root_type != 'alias':
            return

        # Get alias info from docvars
        docvars = root.find('.//docvars')
        if not docvars:
            return

        # Try to find elements with explicit checks
        alias_mnemonic_elem = None
        base_mnemonic_elem = None

        for docvar in docvars.findall('./docvar'):
            key = docvar.get('key')
            if key == 'alias_mnemonic':
                alias_mnemonic_elem = docvar
            elif key == 'mnemonic':
                base_mnemonic_elem = docvar

        if alias_mnemonic_elem is None or base_mnemonic_elem is None:
            return

        alias_name = alias_mnemonic_elem.get('value')
        base_name = base_mnemonic_elem.get('value')

        # Get alias conditions from all encodings
        for encoding in root.findall('.//encoding'):
            equivalent_to = encoding.find('.//equivalent_to')
            if equivalent_to:
                aliascond = equivalent_to.find('.//aliascond')
                if aliascond is not None:
                    condition_text = self._extract_condition_text(aliascond)
                    self.aliases.append({
                        'base': base_name,
                        'alias': alias_name,
                        'condition': condition_text,
                        'xml_file': os.path.basename(xml_path),
                        'encoding_name': encoding.get('name', '')
                    })
                    self.base_instructions[base_name].append(alias_name)

    def _extract_condition_text(self, aliascond):
        """Extract the text content of alias condition, including child elements"""
        parts = []
        if aliascond.text:
            parts.append(aliascond.text.strip())
        for child in aliascond:
            if child.text:
                parts.append(child.text.strip())
            if child.tail:
                parts.append(child.tail.strip())
        return ' '.join(parts)

    def _print_analysis(self):
        print(f"Found {len(self.aliases)} alias relationships")
        print(f"Base instructions with aliases: {len(self.base_instructions)}\n")

        # Group by base instruction
        print("=" * 80)
        print("ALIAS PATTERNS BY BASE INSTRUCTION")
        print("=" * 80)

        for base, aliases in sorted(self.base_instructions.items()):
            if len(set(aliases)) > 1:  # Only show if multiple different aliases
                print(f"\n{base} has {len(set(aliases))} different aliases:")

                # Get all alias records for this base
                base_aliases = [a for a in self.aliases if a['base'] == base]

                # Group by alias name
                by_alias = defaultdict(list)
                for a in base_aliases:
                    by_alias[a['alias']].append(a)

                for alias_name, records in sorted(by_alias.items()):
                    print(f"\n  {alias_name}:")
                    for record in records[:3]:  # Show first 3 examples
                        cond = record['condition'][:80] + '...' if len(record['condition']) > 80 else record['condition']
                        print(f"    Condition: {cond}")

        # Analyze common patterns
        print("\n" + "=" * 80)
        print("COMMON ALIAS CONDITION PATTERNS")
        print("=" * 80)

        patterns = defaultdict(list)
        for alias in self.aliases:
            cond = alias['condition']
            # Classify the pattern
            if '==' in cond and '+' in cond:
                patterns['Equality with arithmetic'].append(alias)
            elif '==' in cond:
                patterns['Simple equality'].append(alias)
            elif '<' in cond or '>' in cond:
                patterns['Comparison'].append(alias)
            elif '!' in cond or '!=' in cond:
                patterns['Inequality'].append(alias)
            else:
                patterns['Other'].append(alias)

        for pattern_type, aliases in sorted(patterns.items()):
            print(f"\n{pattern_type}: {len(aliases)} instances")
            # Show examples
            examples = aliases[:3]
            for ex in examples:
                cond = ex['condition'][:60] + '...' if len(ex['condition']) > 60 else ex['condition']
                print(f"  {ex['alias']} ({ex['base']}): {cond}")

        # Focus on UBFM/SBFM/BFM aliases (bitfield instructions)
        print("\n" + "=" * 80)
        print("BITFIELD INSTRUCTION ALIASES (UBFM, SBFM, BFM)")
        print("=" * 80)

        bitfield_bases = ['UBFM', 'SBFM', 'BFM']
        for base in bitfield_bases:
            if base in self.base_instructions:
                print(f"\n{base} aliases:")
                base_aliases = [a for a in self.aliases if a['base'] == base]

                # Sort by alias name
                by_alias = defaultdict(list)
                for a in base_aliases:
                    by_alias[a['alias']].append(a)

                for alias_name, records in sorted(by_alias.items()):
                    print(f"\n  {alias_name}: ({len(records)} variants)")
                    # Show unique conditions
                    unique_conds = set(r['condition'] for r in records)
                    for cond in sorted(unique_conds)[:2]:
                        cond_display = cond[:100] + '...' if len(cond) > 100 else cond
                        print(f"    {cond_display}")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <xml_directory>")
        sys.exit(1)

    analyzer = AliasAnalyzer(sys.argv[1])
    analyzer.analyze_all()
