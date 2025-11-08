#!/usr/bin/env python3
"""
Analyze ARM64 XML files to find all operand link patterns and identify missing support
"""

import xml.etree.ElementTree as ET
import os
import sys
from collections import defaultdict, Counter
import re

def analyze_xml_directory(xml_dir):
    """Analyze all XML files and extract operand patterns"""

    link_patterns = defaultdict(list)  # link -> list of (hover, xml_file)
    link_types = Counter()  # Count frequency of each link pattern

    xml_files = [f for f in os.listdir(xml_dir) if f.endswith('.xml')]
    print(f"Analyzing {len(xml_files)} XML files...\n")

    for xml_file in xml_files:
        try:
            tree = ET.parse(os.path.join(xml_dir, xml_file))
            root = tree.getroot()

            # Find all asmtemplate elements
            for asmtemplate in root.findall('.//asmtemplate'):
                for child in asmtemplate:
                    if child.tag == 'a':
                        link = child.get('link', '')
                        hover = child.get('hover', '')

                        if link:
                            link_patterns[link].append((hover[:80], xml_file))
                            link_types[link] += 1
        except Exception as e:
            print(f"Error parsing {xml_file}: {e}")

    return link_patterns, link_types

def categorize_links(link_types):
    """Categorize links by type"""

    categories = {
        'registers': [],
        'immediates': [],
        'labels': [],
        'shifts': [],
        'extends': [],
        'conditions': [],
        'memory': [],
        'options': [],
        'other': []
    }

    for link, count in link_types.items():
        link_lower = link.lower()

        if any(p in link_lower for p in ['xd', 'xn', 'xm', 'wd', 'wn', 'wm', 'xt', 'wt', 'sp', 'zr']):
            categories['registers'].append((link, count))
        elif any(p in link_lower for p in ['bd', 'hd', 'sd', 'dd', 'qd', 'vd']):
            categories['registers'].append((link, count))
        elif 'imm' in link_lower or 'amount' in link_lower:
            categories['immediates'].append((link, count))
        elif 'label' in link_lower:
            categories['labels'].append((link, count))
        elif 'shift' in link_lower:
            categories['shifts'].append((link, count))
        elif 'extend' in link_lower:
            categories['extends'].append((link, count))
        elif 'cond' in link_lower:
            categories['conditions'].append((link, count))
        elif 'option' in link_lower or 'r_option' == link_lower:
            categories['options'].append((link, count))
        elif any(p in link_lower for p in ['memory', 'address']):
            categories['memory'].append((link, count))
        else:
            categories['other'].append((link, count))

    return categories

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <xml_directory>")
        sys.exit(1)

    xml_dir = sys.argv[1]

    # Analyze all XML files
    link_patterns, link_types = analyze_xml_directory(xml_dir)

    # Categorize links
    categories = categorize_links(link_types)

    # Print summary
    print("=" * 80)
    print("OPERAND LINK PATTERN SUMMARY")
    print("=" * 80)

    for category, links in categories.items():
        if links:
            print(f"\n{category.upper()} ({len(links)} unique patterns):")
            print("-" * 80)
            # Sort by frequency
            links.sort(key=lambda x: x[1], reverse=True)
            for link, count in links[:20]:  # Show top 20
                print(f"  {link:40s} ({count:4d} uses)")
            if len(links) > 20:
                print(f"  ... and {len(links) - 20} more")

    # Show some examples of "other" category for investigation
    if categories['other']:
        print("\n" + "=" * 80)
        print("EXAMPLES OF 'OTHER' CATEGORY (need investigation):")
        print("=" * 80)
        for link, count in categories['other'][:30]:
            examples = link_patterns[link][:2]
            print(f"\n{link} ({count} uses):")
            for hover, xml_file in examples:
                print(f"  - {hover}...")
                print(f"    (from {xml_file})")

    print(f"\n\nTotal unique link patterns: {len(link_types)}")
    print(f"Total link uses: {sum(link_types.values())}")

if __name__ == '__main__':
    main()
