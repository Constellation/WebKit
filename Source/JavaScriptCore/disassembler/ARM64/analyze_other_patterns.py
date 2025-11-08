#\!/usr/bin/env python3
import xml.etree.ElementTree as ET
import os
import sys
from collections import Counter

xml_dir = sys.argv[1]
other_links = Counter()

for xml_file in os.listdir(xml_dir):
    if not xml_file.endswith('.xml'):
        continue
    try:
        tree = ET.parse(os.path.join(xml_dir, xml_file))
        root = tree.getroot()
        
        for asmtemplate in root.findall('.//asmtemplate'):
            for child in asmtemplate:
                if child.tag == 'a':
                    link = child.get('link', '').lower()
                    
                    # Skip known patterns
                    if any(p in link for p in ['xd', 'xn', 'xm', 'xt', 'xa', 'xs',
                                               'wd', 'wn', 'wm', 'wt', 'wa', 'ws',
                                               'vd', 'vn', 'vm', 'vt', 'va',
                                               'bd', 'bn', 'bt', 'hd', 'hn', 'ht',
                                               'sd', 'sn', 'st', 'dd', 'dn', 'dt',
                                               'qd', 'qn', 'qt',
                                               'zd', 'zn', 'zm', 'za',
                                               'pd', 'pn', 'pm', 'pg',
                                               'imm', 'memory', 'address',
                                               'shift', 'extend', 'option',
                                               'cond', 'label']):
                        continue
                    
                    other_links[link] += 1
    except:
        pass

print("TOP 50 'OTHER' OPERAND LINKS:")
for link, count in other_links.most_common(50):
    print(f"  {link:30s}: {count:5d}")
