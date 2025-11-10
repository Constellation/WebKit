#!/usr/bin/env python3
"""
ARM64 Disassembler Code Generator v3

Complete implementation with:
- Field position metadata
- Full operand formatters for all types
- Logical immediate decoding
- Memory addressing modes
- Special case handling

Usage:
    python3 generate_arm64_disassembler_v3.py <xml_directory> <output_directory>
"""

import xml.etree.ElementTree as ET
import sys
import os
from collections import defaultdict, namedtuple, OrderedDict
from typing import List, Dict, Set, Tuple, Optional
import re

# Data structures
InstructionEncoding = namedtuple('InstructionEncoding', [
    'name', 'mnemonic', 'mask', 'pattern', 'fields', 'operands', 'xml_file',
    'is_64bit_variant', 'aliases', 'has_q_suffix'
])

BitField = namedtuple('BitField', [
    'name', 'bit_start', 'bit_width', 'is_fixed', 'fixed_value'
])

Operand = namedtuple('Operand', [
    'type', 'subtype', 'field_name', 'field_name2', 'is_optional', 'description'
])

class ARM64InstructionParser:
    """Enhanced parser with complete field tracking and explanation parsing"""

    def __init__(self, xml_directory: str):
        self.xml_directory = xml_directory
        self.instructions: List[InstructionEncoding] = []
        self.errors: List[str] = []
        # Map: (encoding_name, symbol_link) -> field encoding
        self.operand_explanations: Dict[Tuple[str, str], str] = {}

    def parse_all(self) -> List[InstructionEncoding]:
        """Parse all XML files"""
        xml_files = [f for f in os.listdir(self.xml_directory) if f.endswith('.xml')]
        print(f"Found {len(xml_files)} XML files")

        for i, xml_file in enumerate(xml_files):
            if (i + 1) % 100 == 0:
                print(f"Parsed {i + 1}/{len(xml_files)} files...")

            try:
                self._parse_file(os.path.join(self.xml_directory, xml_file))
            except Exception as e:
                self.errors.append(f"Error parsing {xml_file}: {e}")

        print(f"Parsed {len(xml_files)} files, found {len(self.instructions)} instruction encodings")

        if self.errors:
            print(f"Encountered {len(self.errors)} errors")

        return self.instructions

    def _parse_file(self, xml_path: str):
        """Parse a single XML file"""
        tree = ET.parse(xml_path)
        root = tree.getroot()

        # Parse explanations first to get field encodings for operands
        self._parse_explanations(root)

        mnemonic = None
        alias_mnemonic = None
        docvars = root.find('.//docvars')
        if docvars is not None:
            mnemonic_elem = docvars.find("./docvar[@key='mnemonic']")
            if mnemonic_elem is not None:
                mnemonic = mnemonic_elem.get('value')
            # Check for alias mnemonic (like SXTL for SSHLL)
            alias_elem = docvars.find("./docvar[@key='alias_mnemonic']")
            if alias_elem is not None:
                alias_mnemonic = alias_elem.get('value')

        for iclass in root.findall('.//iclass'):
            regdiagram = iclass.find('regdiagram')
            if regdiagram is None:
                continue

            for encoding in iclass.findall('.//encoding'):
                try:
                    instr = self._parse_encoding(encoding, regdiagram, mnemonic,
                                                 os.path.basename(xml_path), root, alias_mnemonic)
                    if instr:
                        self.instructions.append(instr)
                except Exception as e:
                    self.errors.append(f"Error in {xml_path}: {e}")

    def _parse_explanations(self, root):
        """Parse explanation sections to extract operand field encodings"""
        for explanation in root.findall('.//explanation'):
            symbol_elem = explanation.find('.//symbol')
            if symbol_elem is None:
                continue

            symbol_link = symbol_elem.get('link', '')
            if not symbol_link:
                continue

            # Check for enclist attribute to know which encodings this applies to
            enclist = explanation.get('enclist', '')
            encoding_names = [e.strip() for e in enclist.split(',')] if enclist else []

            # Check for account/definition with encodedin attribute
            account = explanation.find('.//account[@encodedin]')
            definition = explanation.find('.//definition[@encodedin]')

            encoding_elem = account if account is not None else definition
            if encoding_elem is not None:
                encodedin = encoding_elem.get('encodedin', '')

                # Store for each encoding in enclist
                if encoding_names:
                    for enc_name in encoding_names:
                        key = (enc_name, symbol_link)
                        self.operand_explanations[key] = encodedin
                else:
                    # No enclist, store with empty encoding name (applies to all)
                    key = ('', symbol_link)
                    self.operand_explanations[key] = encodedin

    def _parse_encoding(self, encoding_elem, regdiagram, default_mnemonic: str,
                       xml_file: str, root, alias_mnemonic: Optional[str] = None) -> Optional[InstructionEncoding]:
        """Parse encoding with field position tracking"""
        encoding_name = encoding_elem.get('name')
        if not encoding_name:
            return None

        fields = self._parse_regdiagram(regdiagram)

        # Override with encoding-specific boxes
        # Use bitdiffs attribute to get correct fixed field values
        bitdiffs = encoding_elem.get('bitdiffs', '')
        if bitdiffs:
            # Parse bitdiffs like "opc == 10" to extract field assignments
            field_dict = {f.name: f for f in fields}
            for assignment in bitdiffs.split('&&'):
                assignment = assignment.strip()
                if '==' in assignment:
                    parts = assignment.split('==')
                    if len(parts) == 2:
                        field_name = parts[0].strip()
                        value_str = parts[1].strip()
                        # Convert binary string to integer
                        try:
                            if value_str.startswith('0b'):
                                value = int(value_str, 2)
                            else:
                                # Assume it's a binary string like "10" or "00"
                                if all(c in '01' for c in value_str):
                                    value = int(value_str, 2)
                                else:
                                    continue

                            # Update the field in field_dict
                            if field_name in field_dict:
                                old_field = field_dict[field_name]
                                # Create new fixed field with correct value
                                field_dict[field_name] = BitField(
                                    old_field.name,
                                    old_field.bit_start,
                                    old_field.bit_width,
                                    True,  # is_fixed
                                    value
                                )
                        except ValueError:
                            pass
            fields = list(field_dict.values())
        else:
            # Fallback to parsing encoding boxes
            encoding_boxes = encoding_elem.findall('box')
            if encoding_boxes:
                encoding_fields = self._parse_boxes(encoding_boxes)
                field_dict = {f.name: f for f in fields}
                for ef in encoding_fields:
                    if ef.name in field_dict:
                        field_dict[ef.name] = ef
                fields = list(field_dict.values())

        if not fields:
            return None

        mask, pattern = self._calculate_mask_and_pattern(fields)
        is_64bit = self._is_64bit_variant(fields, encoding_elem)

        asmtemplate_elem = encoding_elem.find('.//asmtemplate')
        mnemonic = alias_mnemonic or default_mnemonic  # Prefer alias if available
        operands = []
        has_q_suffix = False

        if asmtemplate_elem is not None:
            parsed_mnemonic, operands, has_q_suffix = self._parse_asmtemplate(
                asmtemplate_elem, fields, is_64bit, encoding_elem, alias_mnemonic, encoding_name)
            if parsed_mnemonic and not alias_mnemonic:
                # Only use parsed mnemonic if we don't have an alias
                mnemonic = parsed_mnemonic

        aliases = self._parse_aliases(root, encoding_name)
        field_dict = {f.name: (f.bit_start, f.bit_width) for f in fields if not f.is_fixed}

        return InstructionEncoding(
            name=encoding_name,
            mnemonic=mnemonic or "UNKNOWN",
            mask=mask,
            pattern=pattern,
            fields=field_dict,
            operands=operands,
            xml_file=xml_file,
            is_64bit_variant=is_64bit,
            aliases=aliases,
            has_q_suffix=has_q_suffix
        )

    def _is_64bit_variant(self, fields: List[BitField], encoding_elem) -> bool:
        """Determine if 64-bit variant"""
        for field in fields:
            if field.name == 'sf' and field.is_fixed:
                return field.fixed_value == 1

        encoding_name = encoding_elem.get('name', '')
        return '_64_' in encoding_name

    def _parse_aliases(self, root, encoding_name: str) -> List[str]:
        """Parse aliases"""
        aliases = []
        alias_list = root.find('.//alias_list')
        if alias_list is not None:
            for aliasref in alias_list.findall('aliasref'):
                alias_file = aliasref.get('aliasfile', '')
                if alias_file:
                    aliases.append(alias_file.replace('.xml', ''))
        return aliases

    def _parse_asmtemplate(self, asmtemplate_elem, fields: List[BitField],
                          is_64bit: bool, encoding_elem, alias_mnemonic: Optional[str] = None,
                          encoding_name: str = '') -> Tuple[str, List[Operand], bool]:
        """Parse assembly template with proper memory operand detection"""
        mnemonic = None
        operands = []
        field_map = {f.name: f for f in fields}
        has_q_suffix = False

        # Helper to get field encoding from explanation
        def get_field_encoding(symbol_link: str) -> Optional[str]:
            """Get field encoding for a symbol from explanation data

            Note: symbol_link may be lowercase but explanations are stored with original case.
            Try both.
            """
            # Try with specific encoding name first
            key = (encoding_name, symbol_link)
            if key in self.operand_explanations:
                return self.operand_explanations[key]

            # Try with empty encoding name (applies to all)
            key = ('', symbol_link)
            if key in self.operand_explanations:
                return self.operand_explanations[key]

            # Try case-insensitive match for encoding-specific
            if encoding_name:
                for (enc, sym), field in self.operand_explanations.items():
                    if enc == encoding_name and sym.lower() == symbol_link.lower():
                        return field

            # Try case-insensitive match for global
            for (enc, sym), field in self.operand_explanations.items():
                if enc == '' and sym.lower() == symbol_link.lower():
                    return field

            return None

        # Extract parts preserving order
        parts = []
        if asmtemplate_elem.text:
            parts.append(('text', asmtemplate_elem.text))

        for child in asmtemplate_elem:
            if child.tag == 'text' and child.text:
                parts.append(('text', child.text))
            elif child.tag == 'a':
                parts.append(('operand', child.get('link', ''), child.get('hover', '')))
            if child.tail:
                parts.append(('text', child.tail))

        # Extract mnemonic and detect {2} pattern
        for i, part in enumerate(parts):
            if part[0] == 'text':
                text = part[1].strip()
                if i == 0 and not mnemonic:
                    # Check for {2} pattern (e.g., "SXTL{2}")
                    match = re.match(r'^([A-Z][A-Z0-9.]*)\{2\}', text)
                    if match:
                        mnemonic = match.group(1)
                        has_q_suffix = True
                    # Check for split {2} pattern: "SXTL{" + operand + "}"
                    elif text.endswith('{') and i + 2 < len(parts):
                        # Check if next is operand with "2_option" in link
                        if parts[i + 1][0] == 'operand' and '2_option' in parts[i + 1][1].lower():
                            # Check if part after that is "}"
                            if parts[i + 2][0] == 'text' and parts[i + 2][1].strip().startswith('}'):
                                # This is SXTL{2} pattern
                                mnemonic = text.rstrip('{').strip()
                                has_q_suffix = True
                    else:
                        match = re.match(r'^([A-Z][A-Z0-9.]*)', text)
                        if match:
                            mnemonic = match.group(1)
                    break

        # Parse operands with memory grouping
        i = 0
        last_r_option = None  # Track R_option (width specifier) for next register
        last_v_option = None  # Track V_option (SIMD width specifier) for next register
        last_t_option = None  # Track T_option (element size specifier) for next register

        while i < len(parts):
            part = parts[i]

            if part[0] == 'operand':
                link, hover = part[1], part[2]
                link_lower = link.lower()

                # Check if this is R_option (width specifier)
                if link_lower in ['r_option', 'r_option__2', 'r_option__3', 'r_option__4']:
                    # Store hover to determine width for next operand
                    last_r_option = hover
                    i += 1
                    # Check if immediately followed by Rn_option/Rm_option (register number)
                    if i < len(parts) and parts[i][0] == 'operand':
                        next_link = parts[i][1].lower()
                        next_hover = parts[i][2]
                        # Pattern: R_option followed by register number operand
                        if next_link in ['rn_option', 'rn_option__2', 'rm_option', 'rm_option__2', 'rd_option']:
                            # Extract register number field
                            reg_fields = re.findall(r'"([A-Za-z0-9_]+)"', next_hover)
                            reg_field = reg_fields[0] if reg_fields else 'Rn'

                            # Check if width is encoded in imm5 (like INS instruction)
                            # Look for "imm5" in the hover text or check if imm5 field exists
                            if 'imm5' in last_r_option.lower() or 'imm5' in field_map:
                                # Width is determined by imm5 at runtime (bit 3: D=X, else W)
                                # Create REG_GPR_SIZED operand
                                # field1 = register number, field2 = imm5 (size field)
                                operand = Operand('REG_GPR_SIZED', None, reg_field, 'imm5', False, next_hover)
                            else:
                                # Determine register width from R_option hover
                                is_64bit = 'x' in last_r_option.lower() or '64' in last_r_option or 'X' in last_r_option

                                # Create appropriate register operand
                                if is_64bit:
                                    # Check for ZR variant
                                    if 'zr' in last_r_option.lower():
                                        operand = Operand('REG_GPR_XZR', None, reg_field, None, False, next_hover)
                                    else:
                                        operand = Operand('REG_GPR_X', None, reg_field, None, False, next_hover)
                                else:
                                    # W register
                                    if 'zr' in last_r_option.lower() or 'wzr' in last_r_option.lower():
                                        operand = Operand('REG_GPR_WZR', None, reg_field, None, False, next_hover)
                                    else:
                                        operand = Operand('REG_GPR_W', None, reg_field, None, False, next_hover)

                            operands.append(operand)
                            i += 1  # Skip the Rn_option we just processed
                            last_r_option = None
                            continue
                    # If not followed by register number, just continue (R_option alone is skipped)
                    continue

                # Check if this is V_option (SIMD width specifier)
                if link_lower.startswith('v_option'):
                    # Store hover and link to determine SIMD register size
                    last_v_option = (hover, link)
                    i += 1
                    continue

                # Check if this is T_option (element size specifier)
                if link_lower.startswith('t_option'):
                    # Store hover and link to determine element size
                    last_t_option = (hover, link)
                    i += 1
                    continue

                # Check if this is an arrangement specifier (Ta_option, Tb_option, etc.)
                # These come after a "." following a SIMD register
                # Skip them here - they'll be handled by the preceding register operand
                if '_option' in link_lower and not link_lower in ['r_option', 'r_option__2', 'r_option__3']:
                    # Check if previous was a SIMD register followed by "."
                    if i > 0:
                        # Look back to see if this is an arrangement for a register
                        for j in range(i - 1, -1, -1):
                            if parts[j][0] == 'text':
                                if '.' in parts[j][1]:
                                    # This is an arrangement specifier, skip it
                                    i += 1
                                    continue
                                break
                            elif parts[j][0] == 'operand':
                                break

                # Look ahead for arrangement specifier pattern: SIMD register followed by "." and arrangement
                # Pattern: Vd/Vn/Vm/Vt + "." + Ta_option/Tb_option/etc
                # Note: links may have suffixes like Vn__2, so check with startswith
                link_base = re.sub(r'__\d+$', '', link_lower)  # Remove __N suffix
                if link_base in ['vd', 'vn', 'vm', 'vt', 'va'] and i + 2 < len(parts):
                    # IMPORTANT: Check for register list FIRST before processing as standalone arrangement
                    # Pattern: { <Vt>.<T> } → register list (like LD1R)
                    # vs: <Vt>.<T> → standalone arranged register (like FMUL)

                    is_in_register_list = False
                    if i > 0:
                        prev_text = None
                        for j in range(i - 1, -1, -1):
                            if parts[j][0] == 'text':
                                prev_text = parts[j][1]
                                break

                        if prev_text and ('{ ' in prev_text or ',{ ' in prev_text or ', { ' in prev_text):
                            is_in_register_list = True

                    # If inside register list, skip standalone arrangement handling
                    if is_in_register_list:
                        # Don't process here - will be handled by register list logic below
                        pass
                    # Check if next is "." and then an arrangement option
                    elif parts[i + 1][0] == 'text' and '.' in parts[i + 1][1]:
                        if parts[i + 2][0] == 'operand':
                            arrangement_link = parts[i + 2][1].lower()
                            arrangement_hover = parts[i + 2][2]

                            # Check if this is an indexed element: Vd.Ts[index]
                            # Look for "[" after the element size
                            is_indexed = False
                            if i + 3 < len(parts) and parts[i + 3][0] == 'text' and '[' in parts[i + 3][1]:
                                # This is indexed element syntax
                                is_indexed = True

                            if is_indexed and i + 4 < len(parts) and parts[i + 4][0] == 'operand':
                                # Pattern: Vd.Ts[index]
                                # Get register field
                                reg_fields = re.findall(r'"([A-Za-z0-9_]+)"', hover)
                                registerField = reg_fields[0] if reg_fields else 'Rd'

                                # Get index field from parts[i+4]
                                index_link = parts[i + 4][1].lower()
                                index_hover = parts[i + 4][2]
                                index_fields = re.findall(r'"([A-Za-z0-9_:]+)"', index_hover)
                                indexField = index_fields[0] if index_fields else 'imm5'

                                # Get element size field name
                                elem_fields = re.findall(r'"([A-Za-z0-9_:]+)"', arrangement_hover)
                                elem_size_field = elem_fields[0] if elem_fields else None

                                # Determine how element size is encoded
                                # Common patterns:
                                # - Ts_option with imm5 field
                                # - size field (2 bits)
                                # For INS: element size comes from imm5 (lowest set bit)

                                # Create indexed element operand
                                # field1 = register number
                                # field2 = index field (imm5, imm4, or index)
                                # subtype = element size encoding method:
                                #   0 = imm5-based (lowest set bit)
                                #   1 = size field (2-bit)
                                #   2 = other

                                if indexField == 'imm5' or 'imm5' in index_link:
                                    subtype = 0  # imm5-based encoding
                                elif elem_size_field and 'size' in elem_size_field:
                                    subtype = 1  # size field
                                else:
                                    subtype = 0  # default to imm5

                                operand = Operand('REG_SIMD_ELEMENT', subtype, registerField,
                                                indexField, False, f"SIMD {registerField} indexed by {indexField}")
                                operands.append(operand)

                                # Skip past: ".", element_size, "[", index, "]"
                                # Find the closing bracket
                                skip_to = i + 5
                                while skip_to < len(parts):
                                    if parts[skip_to][0] == 'text' and ']' in parts[skip_to][1]:
                                        i = skip_to + 1
                                        break
                                    skip_to += 1
                                else:
                                    i += 5
                                continue

                            elif '_option' in arrangement_link:
                                # Non-indexed arrangement specifier
                                # This is a SIMD register with arrangement
                                # Get field encoding from explanation
                                arrangementField = get_field_encoding(arrangement_link)

                                # If no explanation found, try to extract from hover
                                if not arrangementField:
                                    arr_fields = re.findall(r'"([A-Za-z0-9_:]+)"', arrangement_hover)
                                    arrangementField = arr_fields[0] if arr_fields else None

                                # If still no field found, use heuristics based on link pattern
                                if not arrangementField:
                                    if arrangement_link.startswith('ta_'):
                                        # Destination arrangement - typically immh for shift ops
                                        arrangementField = 'immh'
                                    elif arrangement_link.startswith('tb_'):
                                        # Source arrangement - typically immh:Q for shift ops
                                        arrangementField = 'immh:Q'
                                    elif 't' in arrangement_link and '_option' in arrangement_link:
                                        # Generic arrangement - try size:Q for arithmetic ops, or imm5:Q for DUP-style ops
                                        # Check which field exists in this instruction

                                        # Special case: SIMD logical operations (AND, ORR, EOR, BIC, ORN, BIT, BIF, BSL)
                                        # always use Q-only encoding (bytes only), even though they have a size field.
                                        # The size field distinguishes between operations, not element sizes.
                                        logical_ops = {'and', 'orr', 'eor', 'bic', 'orn', 'bit', 'bif', 'bsl', 'eon'}
                                        if mnemonic.lower() in logical_ops and 'advsimd' in instruction_id.lower():
                                            arrangementField = 'Q'
                                        elif 'size' in field_map:
                                            arrangementField = 'size:Q'
                                        elif 'imm5' in field_map:
                                            arrangementField = 'imm5:Q'
                                        else:
                                            arrangementField = 'Q'  # fallback to just Q
                                    else:
                                        # Fallback
                                        arrangementField = 'Q'

                                # Get register field from current operand hover
                                reg_fields = re.findall(r'"([A-Za-z0-9_]+)"', hover)
                                registerField = reg_fields[0] if reg_fields else 'Rd'

                                # Determine if compound field (has ':')
                                is_compound = ':' in arrangementField if arrangementField else False
                                subtype = 1 if is_compound else 0

                                # Create arranged SIMD register operand
                                # field1 = register number, field2 = arrangement field
                                # subtype: 0 = simple (Ta), 1 = compound (Ta:Q or Tb:Q)
                                operand = Operand('REG_SIMD_ARRANGED', subtype, registerField,
                                                arrangementField, False, f"SIMD {registerField} with arrangement {arrangementField}")
                                operands.append(operand)

                                # Skip the "." and arrangement operand
                                i += 3
                                continue

                # Check for UMOV-style indexed element: Vn followed by ".D[" or ".B[" etc. in text
                # Pattern: Vn operand + text with ".[BHSD][" + index operand + "]"
                link_base = re.sub(r'__\d+$', '', link_lower)
                if link_base in ['vd', 'vn', 'vm', 'vt', 'va'] and i + 1 < len(parts):
                    if parts[i + 1][0] == 'text':
                        next_text = parts[i + 1][1]
                        # Check if text contains element size + bracket: .B[, .H[, .S[, .D[
                        elem_match = re.search(r'\.([BHSD])\[', next_text)
                        if elem_match:
                            # This is UMOV-style indexed element with hardcoded size
                            # Look for index operand after the bracket
                            if i + 2 < len(parts) and parts[i + 2][0] == 'operand':
                                index_link = parts[i + 2][1].lower()
                                index_hover = parts[i + 2][2]

                                # Extract register field
                                reg_fields = re.findall(r'"([A-Za-z0-9_]+)"', hover)
                                registerField = reg_fields[0] if reg_fields else 'Rn'

                                # Extract index field
                                index_fields = re.findall(r'"([A-Za-z0-9_:]+)(?:<[^>]+>)?"', index_hover)
                                if not index_fields:
                                    # Try simpler pattern without angle brackets
                                    index_fields = re.findall(r'"([A-Za-z0-9_:]+)"', index_hover)
                                # Extract just the field name (before any <bits> specifier)
                                index_field_raw = index_fields[0] if index_fields else 'imm5'
                                # Remove any bit specifiers like <4> or <4:0>
                                indexField = re.sub(r'<[^>]+>', '', index_field_raw)

                                # Element size from the matched text
                                elem_size_char = elem_match.group(1)
                                # Map to subtype: B=0, H=1, S=2, D=3 for hardcoded size
                                elemSizeMap = {'B': 0, 'H': 1, 'S': 2, 'D': 3}
                                subtype = elemSizeMap.get(elem_size_char, 3)  # subtype encodes hardcoded size

                                # Create indexed element operand
                                # subtype >= 10 means hardcoded element size (not from imm5)
                                # subtype = 10+size where size: B=0, H=1, S=2, D=3
                                operand = Operand('REG_SIMD_ELEMENT', 10 + subtype, registerField,
                                                indexField, False, f"SIMD {registerField} indexed with hardcoded size")
                                operands.append(operand)

                                # Skip past: text with ".[X][", index operand, and find "]"
                                skip_to = i + 3
                                while skip_to < len(parts):
                                    if parts[skip_to][0] == 'text' and ']' in parts[skip_to][1]:
                                        i = skip_to + 1
                                        break
                                    skip_to += 1
                                else:
                                    i = skip_to
                                continue

                # Check if we're inside a register list (previous was "{ ")
                if i > 0:
                    prev_text = None
                    for j in range(i - 1, -1, -1):
                        if parts[j][0] == 'text':
                            prev_text = parts[j][1]
                            break

                    if prev_text and (', { ' in prev_text or ',{ ' in prev_text or '{ ' in prev_text):
                        # We're building a register list
                        # Collect all operands until we hit "}", "], " or end
                        reglist_parts = self._collect_register_list(parts, i)
                        if reglist_parts:
                            reglist_operand = self._build_register_list_operand(reglist_parts, field_map, encoding_elem)
                            if reglist_operand:
                                operands.append(reglist_operand)
                            i = reglist_parts['end_index']
                            continue

                # Look ahead for memory operand pattern: operand followed by ", ["
                if i + 1 < len(parts) and parts[i + 1][0] == 'text':
                    next_text = parts[i + 1][1]

                    # Check if this starts a memory operand
                    if ', [' in next_text or ',[' in next_text:
                        # This operand is not part of memory, add it normally
                        operand = self._infer_operand(link, hover, field_map, is_64bit, last_r_option, last_v_option, last_t_option)
                        if operand:
                            operands.append(operand)
                            last_r_option = None  # Reset after use
                            last_v_option = None
                            last_t_option = None
                        i += 1
                        continue

                    # Check if we're inside a memory operand (previous was "[" or ", #")
                    if i > 0:
                        prev_text = None
                        for j in range(i - 1, -1, -1):
                            if parts[j][0] == 'text':
                                prev_text = parts[j][1]
                                break

                        if prev_text and ('[' in prev_text or ', #' in prev_text or ',#' in prev_text):
                            # We're building a memory operand
                            # Collect all operands until we hit "]", "]!" or end
                            memory_parts = self._collect_memory_operand(parts, i)
                            if memory_parts:
                                mem_operand = self._build_memory_operand(memory_parts, field_map, is_64bit)
                                if mem_operand:
                                    operands.append(mem_operand)
                                i = memory_parts['end_index']
                                continue

                # Regular operand (not part of memory)
                operand = self._infer_operand(link, hover, field_map, is_64bit, last_r_option, last_v_option, last_t_option)
                if operand:
                    operands.append(operand)
                    last_r_option = None  # Reset after use
                    last_v_option = None
                    last_t_option = None

            i += 1

        return mnemonic, operands, has_q_suffix

    def _collect_memory_operand(self, parts: List, start_index: int) -> Optional[Dict]:
        """Collect all parts of a memory operand"""
        # Find the opening bracket before this operand
        bracket_start = None
        for i in range(start_index - 1, -1, -1):
            if parts[i][0] == 'text' and '[' in parts[i][1]:
                bracket_start = i
                break

        if bracket_start is None:
            return None

        # Find the closing bracket
        bracket_end = None
        has_writeback = False
        for i in range(start_index, len(parts)):
            if parts[i][0] == 'text':
                text = parts[i][1]
                if ']!' in text:
                    bracket_end = i
                    has_writeback = True
                    break
                elif ']' in text:
                    bracket_end = i
                    break

        if bracket_end is None:
            return None

        # Collect operands within brackets
        operands_in_brackets = []
        for i in range(bracket_start + 1, bracket_end + 1):
            if parts[i][0] == 'operand':
                operands_in_brackets.append((i, parts[i][1], parts[i][2]))

        # Determine memory operand type
        mem_type = 'MEMORY_BASE'

        # Check for post-indexed: ], #imm (bracket closes, then comma and immediate)
        if bracket_end + 1 < len(parts):
            text_after = parts[bracket_end][1] if parts[bracket_end][0] == 'text' else ''
            if ']' in text_after and not ']!' in text_after:
                # Check if there's a ", #" after the bracket
                for i in range(bracket_end + 1, len(parts)):
                    if parts[i][0] == 'text' and (', #' in parts[i][1] or ',#' in parts[i][1]):
                        mem_type = 'MEMORY_POSTIDX'
                        # Include the operand after "],"
                        if i + 1 < len(parts) and parts[i + 1][0] == 'operand':
                            operands_in_brackets.append((i + 1, parts[i + 1][1], parts[i + 1][2]))
                            bracket_end = i + 1
                        break

        # Check for pre-indexed: [Xn, #imm]!
        if has_writeback:
            mem_type = 'MEMORY_PREIDX'
        elif len(operands_in_brackets) > 1:
            # Check if second operand is immediate (offset)
            mem_type = 'MEMORY_OFFSET'

        return {
            'type': mem_type,
            'operands': operands_in_brackets,
            'start_index': bracket_start,
            'end_index': bracket_end + 1,
            'has_writeback': has_writeback
        }

    def _build_memory_operand(self, memory_parts: Dict, field_map: Dict, is_64bit: bool) -> Optional[Operand]:
        """Build a memory operand from collected parts"""
        operands = memory_parts['operands']
        mem_type = memory_parts['type']

        if not operands:
            return None

        # First operand is always the base register
        base_link, base_hover = operands[0][1], operands[0][2]
        base_fields = re.findall(r'"([A-Za-z0-9_]+)"', base_hover)
        base_field = base_fields[0] if base_fields else 'Rn'

        # Second operand (if exists) is offset/index register
        offset_field = None
        if len(operands) > 1:
            offset_link, offset_hover = operands[1][1], operands[1][2]
            offset_fields = re.findall(r'"([A-Za-z0-9_]+)"', offset_hover)
            offset_field = offset_fields[0] if offset_fields else 'imm'

        return Operand(mem_type, None, base_field, offset_field, False,
                      f"Memory: {mem_type}")

    def _collect_register_list(self, parts: List, start_index: int) -> Optional[Dict]:
        """Collect all parts of a register list with hardcoded or variable arrangements"""
        # Find the opening brace before this operand
        brace_start = None
        for i in range(start_index - 1, -1, -1):
            if parts[i][0] == 'text' and '{' in parts[i][1]:
                brace_start = i
                break

        if brace_start is None:
            return None

        # Find the closing brace
        brace_end = None
        hardcoded_arrangement = None
        for i in range(start_index, len(parts)):
            if parts[i][0] == 'text':
                text = parts[i][1]
                if '}' in text:
                    brace_end = i
                    # Extract hardcoded arrangement like ".16B" from ".16B }, "
                    match = re.search(r'\.(\d+[BHSDQ])', text)
                    if match:
                        hardcoded_arrangement = match.group(1)
                    break

        if brace_end is None:
            return None

        # Collect all register and arrangement operands within braces
        # Pattern 1 (TBL): { <Vn> TEXT(.16B) }
        # Pattern 2 (LD1): { <Vt> TEXT(.) <T_option> }
        registers = []
        arrangement_operand = None  # For variable arrangements (LD1 style)

        i = brace_start + 1
        while i < brace_end + 1:
            if parts[i][0] == 'operand':
                link, hover = parts[i][1], parts[i][2]
                # Check if this is a register operand (Vn, Vt, VnPlus1, etc.)
                if any(x in link.lower() for x in ['vn', 'vt', 'vnplus', 'vtplus']):
                    registers.append((i, link, hover))

                    # Check if next is TEXT with just "." followed by arrangement operand
                    if i + 2 < len(parts):
                        if parts[i + 1][0] == 'text' and '.' in parts[i + 1][1]:
                            # Check if following part is an arrangement operand
                            if parts[i + 2][0] == 'operand':
                                arr_link = parts[i + 2][1].lower()
                                if '_option' in arr_link and ('t_' in arr_link or 'ta_' in arr_link or 'tb_' in arr_link):
                                    # This is a variable arrangement operand (like T_option)
                                    arr_hover = parts[i + 2][2]
                                    arrangement_operand = (parts[i + 2][1], arr_hover)
                                    # Skip past the dot and arrangement operand
                                    i += 2
            i += 1

        return {
            'registers': registers,
            'hardcoded_arrangement': hardcoded_arrangement,
            'arrangement_operand': arrangement_operand,  # (link, hover) or None
            'start_index': brace_start,
            'end_index': brace_end + 1
        }

    def _build_register_list_operand(self, reglist_parts: Dict, field_map: Dict, encoding_elem) -> Optional[Operand]:
        """Build a register list operand from collected parts (hardcoded or variable arrangements)"""
        registers = reglist_parts['registers']
        hardcoded_arrangement = reglist_parts['hardcoded_arrangement']
        arrangement_operand = reglist_parts.get('arrangement_operand')  # (link, hover) or None

        if not registers:
            return None

        # First register is the base
        base_link, base_hover = registers[0][1], registers[0][2]
        base_fields = re.findall(r'"([A-Za-z0-9_]+)"', base_hover)
        base_field = base_fields[0] if base_fields else 'Rn'

        # Store the actual count of registers from the template
        num_registers = len(registers)

        # Determine arrangement encoding
        if arrangement_operand:
            # Variable arrangement (LD1 style): arrangement comes from a field at runtime
            arr_link, arr_hover = arrangement_operand

            # Extract field name from hover or use explanation data
            arr_fields = re.findall(r'"([A-Za-z0-9_:]+)"', arr_hover)
            arrangementField = arr_fields[0] if arr_fields else None

            # If not found, try to get from explanations
            if not arrangementField:
                # Try lowercase link
                arr_link_lower = arr_link.lower()
                # Check common patterns
                if 't_option' in arr_link_lower:
                    # Default patterns for T_option
                    if 'size' in field_map and 'Q' in field_map:
                        arrangementField = 'size:Q'
                    elif 'size' in field_map:
                        arrangementField = 'size'
                    else:
                        arrangementField = 'Q'

            # Use special marker (15) in low bits to indicate variable arrangement
            # subtype = (numRegs << 4) | 15 (variable marker)
            combined_subtype = (num_registers << 4) | 15

            return Operand('REG_LIST', combined_subtype, base_field, arrangementField, False,
                          f"Register list: {num_registers} registers with variable arrangement {arrangementField}")
        else:
            # Hardcoded arrangement (TBL style): arrangement is fixed in the template
            arrangement_map = {
                '16B': 0, '8B': 1, '4S': 2, '2S': 3, '8H': 4, '4H': 5,
                '2D': 6, '1D': 7, '16b': 0, '8b': 1, '4s': 2, '2s': 3,
                '8h': 4, '4h': 5, '2d': 6, '1d': 7
            }
            subtype = arrangement_map.get(hardcoded_arrangement, 0) if hardcoded_arrangement else 0

            # subtype = (numRegs << 4) | arrangementType
            combined_subtype = (num_registers << 4) | subtype

            return Operand('REG_LIST', combined_subtype, base_field, None, False,
                          f"Register list: {num_registers} registers with {hardcoded_arrangement}")

    def _infer_operand(self, link: str, hover: str, field_map: Dict,
                      is_64bit: bool, r_option_hover: Optional[str] = None,
                      v_option_data: Optional[Tuple[str, str]] = None,
                      t_option_data: Optional[Tuple[str, str]] = None) -> Optional[Operand]:
        """Infer operand type with field mapping

        Args:
            link: Link name from XML
            hover: Hover text from XML
            field_map: Map of field names to BitField objects
            is_64bit: Whether instruction is 64-bit
            r_option_hover: R_option hover text for GP register width
            v_option_data: Tuple of (hover, link) for V_option SIMD width specifier
            t_option_data: Tuple of (hover, link) for T_option element size specifier
        """
        # Extract field names from quotes, handling compound fields like "imms:immr" or "N:imms:immr"
        quoted_strings = re.findall(r'"([A-Za-z0-9_:]+)"', hover)
        field_names = []
        for qs in quoted_strings:
            # Split on colon to handle compound field names
            field_names.extend(qs.split(':'))

        primary_field = field_names[0] if field_names else None
        secondary_field = field_names[1] if len(field_names) > 1 else None
        is_optional = '{' in link or 'optional' in hover.lower()

        link_lower = link.lower()
        hover_lower = hover.lower()

        # Strip suffix numbers from link patterns (__2, __3, etc.) for better matching
        link_base = re.sub(r'__\d+$', '', link_lower)

        # Check for single-letter register numbers with V_option or T_option context
        # These are register numbers (d, n, m, t, a) that depend on a preceding size specifier
        if re.match(r'^[dnmta](__\d+)?$', link_lower):
            # This is a single-letter register number
            if v_option_data:
                # V_option specifies the register size (B/H/S/D/Q)
                # Extract the field name from V_option hover (e.g., "sz", "size", "Q")
                v_hover, v_link = v_option_data
                # Get the field name for the size specifier
                v_field_names = re.findall(r'"([A-Za-z0-9_:]+)"', v_hover)
                size_field = v_field_names[0] if v_field_names else 'size'
                # Return a sized SIMD register operand
                # The size_field determines B/H/S/D/Q at runtime
                return Operand('REG_SIMD_SIZED', None, primary_field or 'Rd', size_field, is_optional,
                              f"SIMD register with size from {size_field}")
            elif t_option_data:
                # T_option specifies element size
                t_hover, t_link = t_option_data
                t_field_names = re.findall(r'"([A-Za-z0-9_:]+)"', t_hover)
                size_field = t_field_names[0] if t_field_names else 'size'
                return Operand('REG_SIMD_SIZED', None, primary_field or 'Rd', size_field, is_optional,
                              f"SIMD register with size from {size_field}")
            elif 'simd' in hover_lower or 'fp' in hover_lower or 'vector' in hover_lower:
                # Single-letter SIMD/FP register without size specifier
                # Default to generic SIMD register
                return Operand('REG_SIMD_V', None, primary_field or 'Rd', None, is_optional, hover)

        # GP registers - check for specific register patterns at word boundaries
        # Use more specific checks to avoid false matches like 'xn' in 'extend_option'
        if link_lower.startswith(('xd', 'xn', 'xm', 'xa', 'xt', 'xs')) or \
           any(link_lower.startswith(p + '_') or link_lower.startswith(p + 'or') for p in ['xd', 'xn', 'xm', 'xa', 'xt', 'xs']):
            if 'sp' in link_lower:
                return Operand('REG_GPR_XSP', None, primary_field or 'Rd', secondary_field, is_optional, hover)
            elif 'zr' in link_lower:
                return Operand('REG_GPR_XZR', None, primary_field or 'Rd', secondary_field, is_optional, hover)
            return Operand('REG_GPR_X', None, primary_field or 'Rd', secondary_field, is_optional, hover)

        if link_lower.startswith(('wd', 'wn', 'wm', 'wa', 'wt', 'ws')) or \
           any(link_lower.startswith(p + '_') or link_lower.startswith(p + 'or') for p in ['wd', 'wn', 'wm', 'wa', 'wt', 'ws']):
            if 'sp' in link_lower:
                return Operand('REG_GPR_WSP', None, primary_field or 'Rd', secondary_field, is_optional, hover)
            elif 'zr' in link_lower:
                return Operand('REG_GPR_WZR', None, primary_field or 'Rd', secondary_field, is_optional, hover)
            return Operand('REG_GPR_W', None, primary_field or 'Rd', secondary_field, is_optional, hover)

        # Register number with width specifier (Rm_option, Rn_option, etc.)
        # These come after R_option (width specifier) and specify the register number
        # Check if there's R_option context, or look ahead for extend operand
        if link_lower in ['rm_option', 'rn_option', 'rd_option', 'ra_option', 'rs_option', 'rt_option']:
            # For extended register addressing (like ADD with extend),
            # the register is typically W when followed by an extend operand
            # Check if R_option indicates W register
            is_w_register = False
            if r_option_hover and ('<w' in r_option_hover.lower() or '32-bit' in r_option_hover.lower()):
                is_w_register = True

            # Return the appropriate register type
            if is_w_register:
                return Operand('REG_GPR_W', None, primary_field or 'Rm', secondary_field, is_optional, hover)
            else:
                # For Rm_option specifically, default to W for extended register instructions
                # ARM64 extended register instructions use W registers (Wm) not X registers
                if link_lower == 'rm_option' and r_option_hover:
                    # If we have R_option context, default to W for Rm
                    return Operand('REG_GPR_W', None, primary_field or 'Rm', secondary_field, is_optional, hover)
                return Operand('REG_GPR_X', None, primary_field or 'Rm', secondary_field, is_optional, hover)

        # SVE registers - Check BEFORE FP registers to avoid misclassification
        # SVE Z registers: links like "sa_zdn", "sa_zm", or hover containing "scalable vector"
        # SVE P registers: links like "sa_pg", "sa_pn", or hover containing "scalable predicate"
        if 'scalable vector' in hover_lower or any(p in link_lower for p in ['sa_zdn', 'sa_zm', 'sa_zn', 'sa_zd', 'sa_za']):
            return Operand('REG_SVE_Z', None, primary_field or 'Zd', None, is_optional, hover)
        if 'scalable predicate' in hover_lower or any(p in link_lower for p in ['sa_pg', 'sa_pn', 'sa_pm', 'sa_pd']):
            return Operand('REG_SVE_P', None, primary_field or 'Pd', None, is_optional, hover)

        # Also check for traditional SVE link patterns (zd, zn, zm, za, pd, pn, pm, pg)
        # These come after scalable text check to prioritize hover-based detection
        if any(p in link_lower for p in ['zd', 'zn', 'zm', 'za']) and not any(p in link_lower for p in ['wzr', 'xzr']):
            return Operand('REG_SVE_Z', None, primary_field or 'Zd', None, is_optional, hover)
        if any(p in link_lower for p in ['pd', 'pn', 'pm', 'pg']) and 'predicate' in hover_lower:
            return Operand('REG_SVE_P', None, primary_field or 'Pd', None, is_optional, hover)

        # FP registers
        # Support d/n/m/t/a suffixes (destination/source1/source2/transfer/accumulator)
        # 't' suffix is used for load/store instructions (Rt = transfer register)
        # For 't' patterns, field is typically 'Rt'; for 'd' patterns, typically 'Rd'
        if any(p in link_lower for p in ['bd', 'bn']):
            return Operand('REG_FP_B', None, primary_field or 'Rd', None, is_optional, hover)
        if any(p in link_lower for p in ['bt']):
            return Operand('REG_FP_B', None, primary_field or 'Rt', None, is_optional, hover)
        if any(p in link_lower for p in ['hd', 'hn']):
            return Operand('REG_FP_H', None, primary_field or 'Rd', None, is_optional, hover)
        if any(p in link_lower for p in ['ht']):
            return Operand('REG_FP_H', None, primary_field or 'Rt', None, is_optional, hover)
        if any(p in link_lower for p in ['sd', 'sn']):
            return Operand('REG_FP_S', None, primary_field or 'Rd', None, is_optional, hover)
        if any(p in link_lower for p in ['st']):
            return Operand('REG_FP_S', None, primary_field or 'Rt', None, is_optional, hover)
        if any(p in link_lower for p in ['dd', 'dn']):
            return Operand('REG_FP_D', None, primary_field or 'Rd', None, is_optional, hover)
        if any(p in link_lower for p in ['dt']):
            return Operand('REG_FP_D', None, primary_field or 'Rt', None, is_optional, hover)
        if any(p in link_lower for p in ['qd', 'qn']):
            return Operand('REG_FP_Q', None, primary_field or 'Rd', None, is_optional, hover)
        if any(p in link_lower for p in ['qt']):
            return Operand('REG_FP_Q', None, primary_field or 'Rt', None, is_optional, hover)
        if any(p in link_lower for p in ['vd', 'vn', 'vm']):
            return Operand('REG_SIMD_V', None, primary_field or 'Rd', None, is_optional, hover)
        if any(p in link_lower for p in ['vt']):
            return Operand('REG_SIMD_V', None, primary_field or 'Rt', None, is_optional, hover)

        # Bit position (for TBNZ/TBZ)
        if 'bit' in hover_lower and ('number' in hover_lower or 'position' in hover_lower):
            # Bit position operand like b40_b5 in TBNZ
            # Encoded as b5 (high bit) and b40 (low 5 bits): position = b5 * 32 + b40
            # After colon split, fields are in order: b5, b40
            # We want field1=b40 (base), field2=b5 (multiplier), so swap them
            return Operand('IMM_UINT', None, secondary_field or 'b40', primary_field or 'b5', is_optional, hover)

        # Labels (check before immediates since links like "imm14_offset" contain "imm")
        if 'label' in link_lower or 'label' in hover_lower or ('offset' in hover_lower and 'pc' in hover_lower):
            return Operand('LABEL_PCREL', None, primary_field or 'imm', None, is_optional, hover)

        # Immediates
        if 'imm' in link_lower or 'hw_imm' in link_lower or (link_lower.startswith('shift__')):
            # Special handling for composite immediates like hw_imm16 (MOV/MOVZ)
            # These have link patterns like "hw_imm16__4" and hover like "imm16:hw"
            if 'hw_imm' in link_lower or ('imm16' in hover_lower and 'hw' in hover_lower):
                # This is a MOV-style immediate combining imm16 and hw (shift) fields
                # imm16 is the main immediate, hw is the shift amount
                return Operand('IMM_UINT', None, 'imm16', 'hw', is_optional, hover)

            # Handle shift amounts (like shift__8 in MOVK) as immediates
            # These are encoded in the hw field and represent shift amounts
            if link_lower.startswith('shift__'):
                # This is a shift amount, typically encoded in hw field
                # Check hover for field name
                if 'hw' in hover_lower:
                    return Operand('IMM_UINT', None, 'hw', None, is_optional, hover)
                return Operand('IMM_UINT', None, primary_field or 'shift', None, is_optional, hover)

            if 'logical' in hover_lower or 'bitmask' in hover_lower:
                return Operand('IMM_LOGICAL', None, primary_field or 'imms', 'N', is_optional, hover)
            elif 'shift' in hover_lower:
                return Operand('IMM_SHIFTED', None, primary_field or 'imm', 'sh', is_optional, hover)
            elif 'float' in hover_lower or 'fp' in hover_lower:
                return Operand('IMM_FLOAT', None, primary_field or 'imm', None, is_optional, hover)
            elif 'unsigned' in hover_lower:
                return Operand('IMM_UINT', None, primary_field or 'imm', None, is_optional, hover)
            elif 'signed' in hover_lower or 'offset' in hover_lower:
                return Operand('IMM_SINT', None, primary_field or 'imm', None, is_optional, hover)
            return Operand('IMM_UINT', None, primary_field or 'imm', None, is_optional, hover)

        # Condition
        if 'cond' in link_lower:
            return Operand('CONDITION', None, 'cond', None, is_optional, hover)

        # Shift/extend
        # Be specific: shift_option/shift_type are shift types, plain shift__N are shift amounts
        if 'shift_option' in link_lower or 'shift_type' in link_lower:
            # The shift amount field is typically 'imm6' for shifted register operations
            return Operand('SHIFT_TYPE', None, 'shift', 'imm6', is_optional, hover)
        if 'extend' in link_lower or 'extend_option' in link_lower:
            return Operand('EXTEND_TYPE', None, 'option', 'imm3', is_optional, hover)

        # Memory
        if '[' in link or 'memory' in hover_lower:
            if 'post-indexed' in hover_lower or 'post indexed' in hover_lower:
                return Operand('MEMORY_POSTIDX', None, 'Rn', 'imm', False, hover)
            elif 'pre-indexed' in hover_lower or 'pre indexed' in hover_lower:
                return Operand('MEMORY_PREIDX', None, 'Rn', 'imm', False, hover)
            elif 'register offset' in hover_lower:
                return Operand('MEMORY_REG', None, 'Rn', 'Rm', False, hover)
            return Operand('MEMORY_BASE', None, 'Rn', 'imm', False, hover)

        return None

    def _parse_boxes(self, boxes) -> List[BitField]:
        """Parse box elements"""
        fields = []
        for box in boxes:
            hibit = int(box.get('hibit', '0'))
            width = int(box.get('width', '1')) if box.get('width') else 1
            bit_start = hibit - width + 1

            name = box.get('name')
            usename = box.get('usename') == '1'
            settings = box.get('settings')
            psbits = box.get('psbits', '')
            is_fixed = settings is not None
            fixed_value = 0

            if is_fixed and settings:
                fixed_bits = []
                for child in box:
                    if child.tag == 'c' and child.text and child.text.strip():
                        fixed_bits.append(child.text.strip())

                if fixed_bits:
                    pattern = ''.join(fixed_bits)

                    # Check if pattern contains 'x' - means partially fixed
                    if 'x' in pattern.lower():
                        # Partially fixed field (e.g., "1x" means bit[23]=1, bit[22]=variable)
                        # We need to create multiple BitField entries - one for each actually-fixed bit
                        for i, bit_char in enumerate(pattern):
                            bit_pos = hibit - i
                            if bit_char in ('0', '1'):
                                # This bit is fixed
                                fields.append(BitField(
                                    f"_fixed_{bit_pos}",
                                    bit_pos,
                                    1,
                                    True,
                                    int(bit_char)
                                ))
                        # Also add the field as a whole for operand extraction (not fixed)
                        if name and usename:
                            fields.append(BitField(name, bit_start, width, False, 0))
                        continue  # Don't process further

                    # Fully fixed field - all bits are 0 or 1
                    binary_str = pattern
                    if all(c in '01' for c in binary_str):
                        fixed_value = int(binary_str, 2)
                    else:
                        # Non-binary content (like "!= 0000") means variable field
                        is_fixed = False

            if name and usename:
                fields.append(BitField(name, bit_start, width, is_fixed, fixed_value if is_fixed else 0))
            elif is_fixed:
                fields.append(BitField(f"_fixed_{bit_start}", bit_start, width, True, fixed_value))

        return fields

    def _parse_regdiagram(self, regdiagram) -> List[BitField]:
        """Parse regdiagram"""
        return self._parse_boxes(regdiagram.findall('box'))

    def _calculate_mask_and_pattern(self, fields: List[BitField]) -> Tuple[int, int]:
        """Calculate mask and pattern"""
        mask = 0
        pattern = 0
        for field in fields:
            if field.is_fixed:
                field_mask = ((1 << field.bit_width) - 1) << field.bit_start
                mask |= field_mask
                pattern |= (field.fixed_value << field.bit_start)
        return mask, pattern


class CodeGenerator:
    """Complete code generator with all formatters"""

    OP_TYPES = {
        'REG_GPR_X': 0, 'REG_GPR_W': 1, 'REG_GPR_SP': 2,
        'REG_GPR_XSP': 3, 'REG_GPR_WSP': 4,
        'REG_GPR_XZR': 5, 'REG_GPR_WZR': 6,
        'REG_GPR_SIZED': 7,  # GP register with width determined by field (like imm5 in INS)
        'REG_FP_B': 10, 'REG_FP_H': 11, 'REG_FP_S': 12,
        'REG_FP_D': 13, 'REG_FP_Q': 14, 'REG_SIMD_V': 15,
        'REG_SIMD_SIZED': 16,  # SIMD register with size determined by field
        'REG_SIMD_ARRANGED': 17,  # SIMD register with arrangement specifier
        'REG_SIMD_ELEMENT': 18,  # SIMD register with indexed element (v1.b[0])
        'REG_LIST': 19,  # Register list with hardcoded arrangement (e.g., { v0.16b, v1.16b })
        'REG_SVE_Z': 20, 'REG_SVE_P': 21,
        'IMM_UINT': 30, 'IMM_SINT': 31, 'IMM_HEX': 32,
        'IMM_FLOAT': 33, 'IMM_LOGICAL': 34, 'IMM_SHIFTED': 35,
        'LABEL_PCREL': 40,
        'CONDITION': 50, 'SHIFT_TYPE': 51, 'EXTEND_TYPE': 52,
        'MEMORY_BASE': 60, 'MEMORY_OFFSET': 61, 'MEMORY_REG': 62,
        'MEMORY_PREIDX': 63, 'MEMORY_POSTIDX': 64,
        'UNKNOWN': 127,
    }

    def __init__(self, instructions: List[InstructionEncoding], output_dir: str):
        self.instructions = instructions
        self.output_dir = output_dir

        # Collect unique mnemonics for enum generation
        unique_mnemonics = sorted(set(instr.mnemonic.lower() for instr in instructions))

        # Build mnemonic enum mapping (mnemonic_string -> enum_value)
        self.mnemonic_enum = {mnemonic: idx for idx, mnemonic in enumerate(unique_mnemonics)}
        self.mnemonic_names = unique_mnemonics  # For enum generation

        print(f"Found {len(unique_mnemonics)} unique mnemonics")

        # Define systematic alias precedence based on condition specificity
        # Analysis of 187 alias relationships from ARM64 XML files shows these patterns:
        #
        # Precedence principles (highest to lowest):
        # 1. Arithmetic equality (most specific): field1 + constant == field2
        # 2. Function-based checks: BFXPreferred(), etc.
        # 3. Simple equality: field1 == field2
        # 4. Inequality with negation: !(condition)
        # 5. Comparison: <, >=, <=, >
        # 6. Unconditional (least specific, implicit constraints in encoding)
        # 7. Base instructions (non-aliases)
        #
        # For bitfield instructions (UBFM, SBFM, BFM):
        #   - LSL: imms + 1 == immr (arithmetic equality - most specific)
        #   - LSR/ASR: unconditional (but constrained by imms field values)
        #   - UBFX/SBFX: BFXPreferred() function check
        #   - UBFIZ/SBFIZ: imms < immr (comparison)
        #   - UXTB/UXTH/SXTB/SXTH/SXTW: unconditional (specific field values)
        #   - BFI/BFC: imms < immr (comparison)
        #   - BFXIL: imms >= immr (comparison)

        self.alias_priority = {
            # Bitfield shift aliases (arithmetic equality - highest priority)
            'lsl': 0,      # imms + 1 == immr

            # Function-based checks
            'ubfx': 10,    # BFXPreferred()
            'sbfx': 10,    # BFXPreferred()

            # Simple shifts (unconditional but specific encoding)
            'lsr': 20,     # Unconditional (imms == 31/63)
            'asr': 20,     # Unconditional (imms == 31/63)

            # Comparison-based (less specific)
            'ubfiz': 40,   # imms < immr
            'sbfiz': 40,   # imms < immr
            'bfi': 40,     # imms < immr
            'bfc': 40,     # imms < immr (with Rn==11111)
            'bfxil': 45,   # imms >= immr

            # Extension aliases (unconditional, specific field values)
            'uxtb': 50,    # Unconditional (specific imms/immr)
            'uxth': 50,    # Unconditional (specific imms/immr)
            'sxtb': 50,    # Unconditional (specific imms/immr)
            'sxth': 50,    # Unconditional (specific imms/immr)
            'sxtw': 50,    # Unconditional (specific imms/immr)

            # SIMD extension aliases (Q-bit controlled)
            'sxtl': 0,     # High priority for Q-bit controlled variant
            'uxtl': 0,     # High priority for Q-bit controlled variant

            # SIMD logical operations and aliases
            'orr': 30,     # Base instruction
            'mov': 60,     # Alias of ORR with Rm==Rn condition (lower priority)

            # Base instructions (lowest priority)
            'ubfm': 100,
            'sbfm': 100,
            'bfm': 100,
        }

        # Sort by:
        # 1. Top 4 bits (bits 25-28) for hash table grouping
        # 2. Specificity (bit count) within each group
        # 3. Alias priority within same specificity
        def sort_key(instr):
            # Extract top 4 bits if they're fixed
            top_bits_mask = (instr.mask >> 25) & 0xF
            if top_bits_mask == 0xF:
                top_bits = (instr.pattern >> 25) & 0xF
            else:
                # Variable top bits go to fallback bucket (16)
                top_bits = 16

            mask_bits = bin(instr.mask).count('1')
            priority = self.alias_priority.get(instr.mnemonic.lower(), 50)
            return (top_bits, -mask_bits, priority)

        self.instructions.sort(key=sort_key)

        # Build bucket information for hash table (bits 25-28)
        # 17 buckets: 0-15 for fixed top bits, 16 for variable
        self.buckets = []
        current_bucket = -1
        bucket_start = 0

        for i, instr in enumerate(self.instructions):
            top_bits_mask = (instr.mask >> 25) & 0xF
            if top_bits_mask == 0xF:
                bucket_id = (instr.pattern >> 25) & 0xF
            else:
                bucket_id = 16

            if bucket_id != current_bucket:
                # Save previous bucket if exists
                if current_bucket >= 0:
                    self.buckets.append((current_bucket, bucket_start, i - bucket_start))

                current_bucket = bucket_id
                bucket_start = i

        # Save last bucket
        if current_bucket >= 0:
            self.buckets.append((current_bucket, bucket_start, len(self.instructions) - bucket_start))

    def generate_all(self):
        """Generate all files"""
        print("Generating complete disassembler with formatters...")

        self._generate_header()
        self._generate_implementation()

        print("Generation complete!")

    def _generate_header(self):
        """Generate header"""
        with open(os.path.join(self.output_dir, 'A64InstructionTable.h'), 'w') as f:
            f.write(self._copyright())
            f.write("""
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace JSC::ARM64Disassembler {

enum class OperandType : uint8_t;

enum class Mnemonic : uint16_t {
""")
            # Generate enum values
            for i, mnemonic in enumerate(self.mnemonic_names):
                # Convert to valid C++ identifier (replace dots, etc.)
                enum_name = mnemonic.upper().replace('.', '_')
                f.write(f"    ARM64_{enum_name} = {i},\n")

            f.write("""};

// Instruction entry
struct InstructionEntry {
    const char* mnemonic;       // Lowercase mnemonic string
    uint32_t mask;              // Instruction mask
    uint32_t pattern;           // Instruction pattern
    uint16_t operandOffset;     // Offset into operand table
    Mnemonic mnemonicEnum;      // Mnemonic enum value (for fast comparisons)
    uint8_t operandCount;       // Number of operands
    uint8_t flags;              // Instruction flags
};

// Operand descriptor
struct OperandDesc {
    OperandType type;
    uint8_t subtype;
    uint8_t field1Start;
    uint8_t field1Width;
    uint8_t field2Start;
    uint8_t field2Width;
};

// Hash bucket for fast instruction lookup
struct InstructionBucket {
    const InstructionEntry* start;
    uint16_t count;
};

// Tables
extern const InstructionEntry g_instructionTable[];
extern const size_t g_instructionTableSize;
extern const InstructionBucket g_instructionBuckets[17];  // 16 buckets + 1 fallback
extern const OperandDesc g_operandTable[];
extern const uint8_t g_operandIndices[];  // Indices into g_operandTable

// API
const InstructionEntry* findInstruction(uint32_t opcode);
void formatInstruction(const InstructionEntry* entry, uint32_t opcode, uint32_t* pc, uint32_t* startPC, uint32_t* endPC, char* buffer, size_t bufferSize);

} // namespace JSC::ARM64Disassembler
""")

    def _generate_implementation(self):
        """Generate complete implementation"""
        with open(os.path.join(self.output_dir, 'A64InstructionTable.cpp'), 'w') as f:
            f.write(self._copyright())
            f.write("""
#include "config.h"

#if ENABLE(ARM64_DISASSEMBLER)

#include "A64InstructionTable.h"
#include <array>
#include <stdio.h>
#include <string.h>

namespace JSC::ARM64Disassembler {

// Helper functions
static inline uint32_t extractBits(uint32_t value, unsigned start, unsigned width)
{
    return (value >> start) & ((1U << width) - 1);
}

static inline int32_t signExtend(uint32_t value, unsigned bits)
{
    if (bits == 0 || bits >= 32) return (int32_t)value;
    uint32_t signBit = 1U << (bits - 1);
    if (value & signBit)
        return (int32_t)(value | (~0U << bits));
    return (int32_t)value;
}

static inline int64_t signExtend64(uint64_t value, unsigned bits)
{
    if (bits == 0 || bits >= 64) return (int64_t)value;
    uint64_t signBit = 1ULL << (bits - 1);
    if (value & signBit)
        return (int64_t)(value | (~0ULL << bits));
    return (int64_t)value;
}

// Condition names
static const char* const g_conditionNames[16] = {
    "eq", "ne", "hs", "lo", "mi", "pl", "vs", "vc",
    "hi", "ls", "ge", "lt", "gt", "le", "al", "nv"
};

// Shift names
static const char* const g_shiftNames[4] = {
    "lsl", "lsr", "asr", "ror"
};

// Extend names
static const char* const g_extendNames[8] = {
    "uxtb", "uxth", "uxtw", "uxtx", "sxtb", "sxth", "sxtw", "sxtx"
};

""")

            # Generate opcode type enums
            f.write(self._generate_op_type_enum())

            # Generate operand table
            f.write(self._generate_operand_table())

            # Generate instruction table
            f.write(self._generate_instruction_table())

            # Generate bucket table for hash lookup
            f.write(self._generate_bucket_table())

            # Generate logical immediate decoder
            f.write(self._generate_logical_immediate_decoder())

            # Generate instruction finder
            f.write(self._generate_finder())

            # Generate complete formatter
            f.write(self._generate_complete_formatter())

            f.write("\n} // namespace JSC::ARM64Disassembler\n\n")
            f.write("#endif // ENABLE(ARM64_DISASSEMBLER)\n")

    def _generate_op_type_enum(self) -> str:
        """Generate opcode type enum"""
        code = "enum class OperandType : uint8_t {\n"

        for key, value in self.OP_TYPES.items():
            code += f'    {key} = {value},\n'

        code += "};\n\n"
        code += "using enum OperandType;\n\n"
        return code

    def _generate_operand_table(self) -> str:
        """Generate compressed operand table with sequence deduplication and subspan detection"""

        # First pass: collect all operand descriptors and build unique operand table
        unique_operands = []
        operand_to_index = {}

        # Collect operand sequences for each instruction
        instruction_sequences = []

        for instr in self.instructions:
            sequence = []
            if instr.operands:
                for op in instr.operands:
                    # Get bit positions from instruction's field dict
                    field1Start, field1Width = instr.fields.get(op.field_name, (255, 0))

                    # Handle compound field names (e.g., "immh:Q")
                    if op.field_name2 and ':' in op.field_name2:
                        # Compound field - use first field for positioning
                        first_field = op.field_name2.split(':')[0]
                        field2Start, field2Width = instr.fields.get(first_field, (255, 0))
                    else:
                        field2Start, field2Width = instr.fields.get(op.field_name2, (255, 0)) if op.field_name2 else (255, 0)

                    op_type_val = op.type
                    if op_type_val not in self.OP_TYPES:
                        raise ValueError(f"{op_type_val} is not registered")
                    op_subtype = op.subtype if op.subtype is not None else 0

                    # Create operand descriptor tuple
                    op_desc = (op_type_val, op_subtype, field1Start, field1Width, field2Start, field2Width)

                    # Add to unique table if not seen before
                    if op_desc not in operand_to_index:
                        operand_to_index[op_desc] = len(unique_operands)
                        unique_operands.append(op_desc)

                    sequence.append(operand_to_index[op_desc])

            instruction_sequences.append(tuple(sequence))

        # Second pass: deduplicate sequences with subspan detection
        # Build unique sequence table and find optimal sharing
        unique_sequences = []
        sequence_to_offset = {}

        def find_subspan(target_seq):
            """Find if target_seq exists as a subspan in any existing sequence"""
            if not target_seq:
                return None

            for offset, existing_seq in enumerate(unique_sequences):
                # Try to find target as a contiguous subspan
                target_len = len(target_seq)
                existing_len = len(existing_seq)

                if target_len > existing_len:
                    continue

                for start_pos in range(existing_len - target_len + 1):
                    if existing_seq[start_pos:start_pos + target_len] == target_seq:
                        # Found as subspan at position start_pos in sequence at offset
                        return (offset, start_pos)

            return None

        # Track where each sequence is stored (offset into g_operandIndices)
        sequence_storage = {}
        current_offset = 0

        for seq in instruction_sequences:
            if not seq:
                # Empty sequence
                sequence_storage[seq] = (0, 0)  # offset=0, count=0
                continue

            if seq in sequence_to_offset:
                # Already stored this exact sequence
                sequence_storage[seq] = sequence_to_offset[seq]
                continue

            # Check if this sequence is a subspan of an existing sequence
            subspan = find_subspan(seq)
            if subspan is not None:
                # Found as subspan - calculate offset in flattened array
                base_seq_idx, position = subspan
                # Find the offset of the base sequence in the flattened array
                base_offset = 0
                for i in range(base_seq_idx):
                    base_offset += len(unique_sequences[i])
                actual_offset = base_offset + position
                sequence_storage[seq] = (actual_offset, len(seq))
                sequence_to_offset[seq] = (actual_offset, len(seq))
            else:
                # New unique sequence - add to table
                sequence_to_offset[seq] = (current_offset, len(seq))
                sequence_storage[seq] = (current_offset, len(seq))
                unique_sequences.append(seq)
                current_offset += len(seq)

        # Generate unique operand table
        code = f"// Unique operand descriptors (deduplicated)\n"
        code += f"// Total unique patterns: {len(unique_operands)}\n"
        code += "const OperandDesc g_operandTable[] = {\n"

        for op_desc in unique_operands:
            code += f"    {{ {op_desc[0]}, {op_desc[1]}, {op_desc[2]}, {op_desc[3]}, {op_desc[4]}, {op_desc[5]} }},\n"

        if not unique_operands:
            code += "    { UNKNOWN, 0, 255, 0, 255, 0 }\n"

        code += "};\n\n"

        # Generate deduplicated sequence table
        total_sequences = len(instruction_sequences)
        unique_seq_count = len(unique_sequences)
        total_indices = sum(len(seq) for seq in unique_sequences)

        code += f"// Operand sequences (deduplicated with subspan sharing)\n"
        code += f"// Total instructions: {total_sequences}, Unique sequences: {unique_seq_count}\n"
        code += f"// Compression: {total_indices} indices vs {sum(len(seq) for seq in instruction_sequences)} original "
        code += f"({(1 - total_indices/max(sum(len(seq) for seq in instruction_sequences), 1))*100:.1f}% reduction)\n"
        code += "const uint8_t g_operandIndices[] = {\n"

        if unique_sequences:
            idx_count = 0
            for seq in unique_sequences:
                if idx_count > 0 and idx_count % 16 == 0:
                    code += "\n"
                for idx in seq:
                    code += f"{idx}, "
                    idx_count += 1
            code += "\n"
        else:
            code += "    0\n"

        code += "};\n\n"

        # Build instruction operand info (offset and count into deduplicated sequence table)
        self.instruction_operand_info = []

        for seq in instruction_sequences:
            offset, count = sequence_storage[seq]
            self.instruction_operand_info.append((offset, count))

        return code

    def _generate_instruction_table(self) -> str:
        """Generate instruction table"""
        code = "// Instruction table\n"
        code += "const InstructionEntry g_instructionTable[] = {\n"

        for i, instr in enumerate(self.instructions):
            start_offset, operand_count = self.instruction_operand_info[i]
            # Flags: bit 0 = is_64bit_variant, bit 1 = has_q_suffix
            flags = (1 if instr.is_64bit_variant else 0) | (2 if instr.has_q_suffix else 0)

            # Convert mnemonic to lowercase for table
            lowercase_mnemonic = instr.mnemonic.lower()

            # Get mnemonic enum value
            mnemonic_enum_value = self.mnemonic_enum[lowercase_mnemonic]
            mnemonic_enum_name = lowercase_mnemonic.upper().replace('.', '_')

            code += f"    {{ \"{lowercase_mnemonic}\", 0x{instr.mask:08x}U, 0x{instr.pattern:08x}U, {start_offset}, Mnemonic::ARM64_{mnemonic_enum_name}, {operand_count}, {flags} }}, // {instr.name}\n"

        code += "};\n\n"
        code += f"const size_t g_instructionTableSize = {len(self.instructions)};\n\n"
        return code

    def _generate_bucket_table(self) -> str:
        """Generate hash bucket table for fast lookup"""
        code = "// Hash bucket table for fast instruction lookup\n"
        code += "// Indexed by bits 25-28 of the opcode (16 buckets + 1 fallback)\n"
        code += "const InstructionBucket g_instructionBuckets[17] = {\n"

        # Create array with all 17 buckets (0-15 for fixed bits, 16 for fallback)
        bucket_map = {bucket_id: (start, count) for bucket_id, start, count in self.buckets}

        for i in range(17):
            if i in bucket_map:
                start, count = bucket_map[i]
                if i < 16:
                    code += f"    {{ &g_instructionTable[{start}], {count} }},  // Bucket 0x{i:X}: bits[28:25] = {i:04b}\n"
                else:
                    code += f"    {{ &g_instructionTable[{start}], {count} }},  // Fallback bucket: variable top bits\n"
            else:
                code += f"    {{ nullptr, 0 }},  // Bucket 0x{i:X}: empty\n"

        code += "};\n\n"
        return code

    def _generate_logical_immediate_decoder(self) -> str:
        """Generate ARM64 logical immediate decoding algorithm"""
        return """
// Decode ARM64 logical immediate
// Based on ARM Architecture Reference Manual, pseudocode for DecodeBitMasks
static bool decodeLogicalImmediate(uint32_t n, uint32_t immr, uint32_t imms, bool is64bit, uint64_t* result)
{
    // Determine element size
    unsigned len = 31 - __builtin_clz((n << 6) | (~imms & 0x3f));
    if (len < 1)
        return false;

    unsigned levels = (1U << len) - 1;

    // Check for reserved encoding
    if ((imms & levels) == levels)
        return false;

    unsigned s = imms & levels;
    unsigned r = immr & levels;
    unsigned esize = 1U << len;

    // Calculate the element
    uint64_t welem = (1ULL << (s + 1)) - 1;

    // Rotate right
    if (r) {
        welem = (welem >> r) | (welem << (esize - r));
        welem &= ((1ULL << esize) - 1);
    }

    // Replicate
    uint64_t wmask = 0;
    for (unsigned i = 0; i < (is64bit ? 64 : 32); i += esize) {
        wmask |= welem << i;
    }

    if (!is64bit)
        wmask &= 0xFFFFFFFFULL;

    *result = wmask;
    return true;
}

"""

    def _generate_finder(self) -> str:
        """Generate instruction finder with hash table lookup"""
        return """
const InstructionEntry* findInstruction(uint32_t opcode)
{
    // Two-level hash table lookup using bits 25-28
    // Average case: ~16x faster than linear search

    // Extract bits 25-28 (major instruction class)
    unsigned bucketIndex = (opcode >> 25) & 0xF;

    // Search primary bucket
    const auto& bucket = g_instructionBuckets[bucketIndex];
    for (unsigned i = 0; i < bucket.count; i++) {
        const auto& entry = bucket.start[i];
        if ((opcode & entry.mask) == entry.pattern)
            return &entry;
    }

    // Fallback: Check variable bucket (for instructions with variable top bits)
    const auto& fallback = g_instructionBuckets[16];
    for (unsigned i = 0; i < fallback.count; i++) {
        const auto& entry = fallback.start[i];
        if ((opcode & entry.mask) == entry.pattern)
            return &entry;
    }

    return nullptr;
}

"""

    def _generate_complete_formatter(self) -> str:
        """Generate complete formatter with all operand types"""
        return """
void formatInstruction(const InstructionEntry* entry, uint32_t opcode, uint32_t* pc, uint32_t* startPC, uint32_t* endPC, char* buffer, size_t bufferSize)
{
    if (!entry) {
        snprintf(buffer, bufferSize, "   .long      0x%08x", opcode);
        return;
    }

    // Check if first operand is a condition code for conditional branches
    bool hasConditionSuffix = false;
    const char* conditionCode = nullptr;
    if (entry->operandCount > 0) {
        uint8_t firstOpIdx = g_operandIndices[entry->operandOffset];
        const auto& firstOp = g_operandTable[firstOpIdx];
        if (firstOp.type == CONDITION) {
            hasConditionSuffix = true;
            if (firstOp.field1Width > 0 && firstOp.field1Start < 32) {
                uint32_t cond = extractBits(opcode, firstOp.field1Start, firstOp.field1Width);
                conditionCode = g_conditionNames[cond & 0xf];
            }
        }
    }

    // Check if this instruction has Q-bit controlled "2" suffix (like SXTL2/UXTL2)
    bool hasQSuffix = (entry->flags & 2) != 0;
    bool appendTwo = false;
    if (hasQSuffix) {
        // Check Q bit (bit 30)
        uint32_t q = extractBits(opcode, 30, 1);
        appendTwo = (q == 1);
    }

    // Format mnemonic (with optional "2" suffix and optional condition suffix)
    // Mnemonic is already lowercase in table
    // Use stack-allocated buffer to avoid heap allocation
    int offset = 0;
    std::array<char, 32> mnemonicBuffer;
    size_t mnemonicLength = strlen(entry->mnemonic);

    // Build mnemonic string with optional suffixes
    size_t pos = 0;

    // Copy base mnemonic (buffer is 32 bytes, max mnemonic is ~14 bytes with suffixes)
    memcpy(mnemonicBuffer.data(), entry->mnemonic, mnemonicLength);
    pos = mnemonicLength;

    // Add "2" suffix if needed (before condition suffix)
    // Example: smull2
    if (appendTwo)
        mnemonicBuffer[pos++] = '2';

    // Add condition suffix if needed
    if (hasConditionSuffix && conditionCode) {
        // Check if mnemonic already ends with a dot (like "b.")
        bool hasDot = (mnemonicLength > 0 && entry->mnemonic[mnemonicLength - 1] == '.');

        // Add dot before condition if not already present
        if (!hasDot)
            mnemonicBuffer[pos++] = '.';

        // Add condition code
        const char* cc = conditionCode;
        while (*cc)
            mnemonicBuffer[pos++] = *cc++;
    }

    mnemonicBuffer[pos] = 0;

    offset = snprintf(buffer, bufferSize, "   %-9s", mnemonicBuffer.data());

    if (offset < 0 || (size_t)offset >= bufferSize)
        return;

    // Special handling for bitfield shift aliases (LSL, LSR, ASR)
    // These compute shift amount from immr field: shift = (width - immr) % width
    bool isLslLsrAsr = false;
    uint32_t computedShift = 0;
    if (entry->mnemonicEnum == Mnemonic::ARM64_LSL ||
        entry->mnemonicEnum == Mnemonic::ARM64_LSR ||
        entry->mnemonicEnum == Mnemonic::ARM64_ASR) {
        // Extract immr field (bits 21-16 for UBFM/SBFM)
        uint32_t immr = extractBits(opcode, 16, 6);
        // Determine width from sf bit (bit 31)
        uint32_t sf = extractBits(opcode, 31, 1);
        uint32_t width = sf ? 64 : 32;
        // Compute shift: for LSL: width - immr, for LSR/ASR: immr
        if (entry->mnemonicEnum == Mnemonic::ARM64_LSL) {
            computedShift = (width - immr) % width;
        } else {
            computedShift = immr;
        }
        isLslLsrAsr = true;
    }

    // Format each operand (skip first if it was a condition code)
    unsigned startOperand = hasConditionSuffix ? 1 : 0;
    for (unsigned i = startOperand; i < entry->operandCount; i++) {
        uint8_t opIdx = g_operandIndices[entry->operandOffset + i];
        const auto& op = g_operandTable[opIdx];

        // Pre-check: Skip SHIFT_TYPE operands when shift amount is 0
        // Two cases:
        // 1. ADD/SUB immediate (no field bindings): Check sh bit (bit 22)
        // 2. Shifted register (has field bindings): Check imm6 field (field2)
        if (op.type == SHIFT_TYPE) {
            bool skip = false;

            if (op.field1Width == 0 && op.field2Width == 0) {
                // Case 1: ADD/SUB immediate with sh bit
                uint32_t sh = extractBits(opcode, 22, 1);
                if (sh == 0)
                    skip = true;  // No shift, skip operand
            } else if (op.field2Width > 0 && op.field2Start < 32) {
                // Case 2: Shifted register with imm6 field
                uint32_t imm6 = extractBits(opcode, op.field2Start, op.field2Width);
                if (imm6 == 0)
                    skip = true;  // Shift amount is 0, skip operand
            }

            if (skip)
                continue;  // Skip this operand entirely
        }

        // Add separator
        if (i > startOperand && offset > 0 && (size_t)offset < bufferSize)
            offset += snprintf(buffer + offset, bufferSize - offset, ", ");

        if (offset < 0 || (size_t)offset >= bufferSize)
            return;

        // Extract field values
        uint32_t field1Val = 0;
        uint32_t field2Val = 0;

        if (op.field1Width > 0 && op.field1Start < 32)
            field1Val = extractBits(opcode, op.field1Start, op.field1Width);

        if (op.field2Width > 0 && op.field2Start < 32)
            field2Val = extractBits(opcode, op.field2Start, op.field2Width);

        // Format based on operand type
        switch (op.type) {
        // GP registers
        case REG_GPR_X:
            if (field1Val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1Val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1Val);
            break;

        case REG_GPR_W:
            offset += snprintf(buffer + offset, bufferSize - offset, "w%u", field1Val);
            break;

        case REG_GPR_SP:
        case REG_GPR_XSP:
            if (field1Val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "sp");
            else if (field1Val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1Val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1Val);
            break;

        case REG_GPR_WSP:
            if (field1Val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "wsp");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "w%u", field1Val);
            break;

        case REG_GPR_XZR:
            if (field1Val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "xzr");
            else if (field1Val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1Val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1Val);
            break;

        case REG_GPR_WZR:
            if (field1Val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "wzr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "w%u", field1Val);
            break;

        case REG_GPR_SIZED:
            // field1 = register number, field2 = size field (typically imm5)
            // For INS: imm5 bit 3 determines W (0) vs X (1)
            // Pattern: x1000 = D element = X register, else = W register
            {
                bool use_x_reg = false;
                if (op.field2Width > 0 && op.field2Start < 32) {
                    // Check if bit 3 of the size field is set (D element)
                    use_x_reg = (field2Val & 0x8) != 0;
                }

                if (use_x_reg) {
                    // X register
                    if (field1Val == 29)
                        offset += snprintf(buffer + offset, bufferSize - offset, "fp");
                    else if (field1Val == 30)
                        offset += snprintf(buffer + offset, bufferSize - offset, "lr");
                    else
                        offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1Val);
                } else {
                    // W register
                    offset += snprintf(buffer + offset, bufferSize - offset, "w%u", field1Val);
                }
            }
            break;

        // FP Registers
        case REG_FP_B:
            offset += snprintf(buffer + offset, bufferSize - offset, "b%u", field1Val);
            break;
        case REG_FP_H:
            offset += snprintf(buffer + offset, bufferSize - offset, "h%u", field1Val);
            break;
        case REG_FP_S:
            offset += snprintf(buffer + offset, bufferSize - offset, "s%u", field1Val);
            break;
        case REG_FP_D:
            offset += snprintf(buffer + offset, bufferSize - offset, "d%u", field1Val);
            break;
        case REG_FP_Q:
            offset += snprintf(buffer + offset, bufferSize - offset, "q%u", field1Val);
            break;
        case REG_SIMD_V:
            offset += snprintf(buffer + offset, bufferSize - offset, "v%u", field1Val);
            break;
        case REG_SIMD_SIZED: // size determined by field2
            // field1 = register number (Rd, Rn, etc.)
            // field2 = size field (sz, size, Q, etc.)
            // Map size field value to register prefix OR full arrangement
            {
                // Check if this is DUP-style (needs full arrangement, not just register prefix)
                // DUP uses size field + Q bit to determine arrangement
                // For arrangement output: check if Q bit exists and output like "8b", "16b"
                // For register prefix output: output like "b0", "d0"

                // Try to determine from context: if field2 is "size" at bits 22-23 (width 2),
                // and Q bit is present, output full arrangement
                bool output_arrangement = (op.field2Width == 2 && op.field2Start == 22);

                if (output_arrangement) {
                    // Output full arrangement (like DUP destination)
                    uint32_t Q = extractBits(opcode, 30, 1);
                    uint32_t size = field2Val & 0x3;
                    const char* arrangement = "???";

                    if (size == 0) arrangement = Q ? "16b" : "8b";
                    else if (size == 1) arrangement = Q ? "8h" : "4h";
                    else if (size == 2) arrangement = Q ? "4s" : "2s";
                    else if (size == 3) arrangement = Q ? "2d" : "1d";

                    offset += snprintf(buffer + offset, bufferSize - offset, "v%u.%s", field1Val, arrangement);
                } else {
                    // Output register with size prefix (original behavior)
                    char prefix;
                    // Common mappings:
                    // 1-bit sz: 0=S, 1=D
                    // 2-bit size: 00=B, 01=H, 10=S, 11=D
                    // 1-bit Q: 0=D, 1=Q
                    if (op.field2Width == 1) {
                        // 1-bit field: sz or Q
                        if (op.field2Start == 30) {
                            // Q bit (bit 30) determines scalar size
                            prefix = field2Val ? 'q' : 'd';
                        } else {
                            // sz bit determines FP size
                            prefix = field2Val ? 'd' : 's';
                        }
                    } else if (op.field2Width == 2) {
                        // 2-bit size field
                        const char sizeMap[] = {'b', 'h', 's', 'd'};
                        prefix = sizeMap[field2Val & 3];
                    } else {
                        // Default to 's' if unknown
                        prefix = 's';
                    }
                    offset += snprintf(buffer + offset, bufferSize - offset, "%c%u", prefix, field1Val);
                }
            }
            break;

        case REG_SIMD_ARRANGED: // SIMD register with arrangement specifier
            // field1 = register number (Rd, Rn, etc.)
            // field2 = arrangement field (immh, size, etc.) - determines element size and count
            // subtype: 0 = simple arrangement (Ta), 1 = compound arrangement (Ta:Q or Tb:Q or size:Q)
            {
                // Common patterns for ARM64 SIMD instructions:
                //
                // 1. immh-based (SXTL/SSHLL type, bits 22-19):
                //    - immh[2:0] with bit 0 set (0001) → 8-bit elements
                //    - immh[2:0] with bit 1 set (001x) → 16-bit elements
                //    - immh[2:0] with bit 2 set (01xx) → 32-bit elements
                //    Simple (Ta): 8H/4S/2D, Compound (Tb): (Q?16B:8B)/(Q?8H:4H)/(Q?4S:2S)
                //
                // 2. size-based (ADD/MUL type, bits 23-22):
                //    size=00 → 8-bit, size=01 → 16-bit, size=10 → 32-bit, size=11 → 64-bit
                //    With Q bit: Q=0 → 64-bit vector, Q=1 → 128-bit vector
                //    Arrangements: (Q?16B:8B)/(Q?8H:4H)/(Q?4S:2S)/(Q?2D:1D)
                //
                // 3. imm5-based (DUP type, bits 20-16):
                //    Similar to immh but at different bit position

                const char* arrangement = "???";

                // Check pattern based on field position and width
                if (op.field2Start == 19 && op.field2Width == 4) {
                    // immh field (bits 22-19) - SXTL/SSHLL type
                    uint32_t immhLow = field2Val & 0x7;

                    if (op.subtype == 0) {
                        // Simple: destination with larger elements
                        if (immhLow & 0x4) arrangement = "2d";
                        else if (immhLow & 0x2) arrangement = "4s";
                        else if (immhLow & 0x1) arrangement = "8h";
                    } else {
                        // Compound: source with Q bit
                        uint32_t Q = extractBits(opcode, 30, 1);
                        if (immhLow & 0x4) arrangement = Q ? "4s" : "2s";
                        else if (immhLow & 0x2) arrangement = Q ? "8h" : "4h";
                        else if (immhLow & 0x1) arrangement = Q ? "16b" : "8b";
                    }
                } else if (op.field2Start == 22 && op.field2Width == 2) {
                    // size field (bits 23-22) - ADD/MUL/SUB type
                    uint32_t Q = extractBits(opcode, 30, 1);
                    uint32_t size = field2Val & 0x3;

                    // Element count based on Q and size
                    if (size == 0) arrangement = Q ? "16b" : "8b";   // 8-bit
                    else if (size == 1) arrangement = Q ? "8h" : "4h";    // 16-bit
                    else if (size == 2) arrangement = Q ? "4s" : "2s";    // 32-bit
                    else if (size == 3) arrangement = Q ? "2d" : "1d";    // 64-bit
                } else if (op.field2Start == 16 && op.field2Width == 5) {
                    // imm5 field (bits 20-16) - DUP type
                    // Element size from lowest set bit, arrangement from Q bit
                    uint32_t Q = extractBits(opcode, 30, 1);
                    uint32_t imm5Low = field2Val & 0xF;

                    if (imm5Low & 0x1) arrangement = Q ? "16b" : "8b";       // byte
                    else if (imm5Low & 0x2) arrangement = Q ? "8h" : "4h";   // half
                    else if (imm5Low & 0x4) arrangement = Q ? "4s" : "2s";   // single
                    else if (imm5Low & 0x8) arrangement = Q ? "2d" : "1d";   // double
                } else if (op.field2Start == 30 && op.field2Width == 1) {
                    // Q bit only - used by 107+ instruction families
                    // Comprehensive element size inference from opcode bits
                    uint32_t Q = field2Val & 0x1;

                    // First check for logical operations (AND, ORR, EOR, BIC, ORN, BIT, BIF, BSL)
                    // Pattern: bits[28:21] = 0111_0xx1 (where xx varies), bits[15:11] = 00011, bit[10] = 1
                    // These always use byte arrangements regardless of size field value
                    uint32_t bits2821 = extractBits(opcode, 21, 8);  // bits[28:21]
                    uint32_t bits1510 = extractBits(opcode, 10, 6);   // bits[15:10]
                    bool isLogicalOp = ((bits2821 & 0xF1) == 0x71) && (bits1510 == 0x07);  // 0111_0xx1 and 000111

                    if (isLogicalOp) {
                        // Logical operations always use byte arrangements
                        arrangement = Q ? "16b" : "8b";
                    }
                    // Detect FP SIMD instructions that use size:Q encoding (FAMAX, FAMIN, etc.)
                    // Pattern: bits[28:24]=01110 (SIMD FP class), bit[23]=1 (FP with size field)
                    // This matches FAMAX, FAMIN, and other FP SIMD instructions
                    else {
                        uint32_t bits2824 = extractBits(opcode, 24, 5);
                        uint32_t bit23 = extractBits(opcode, 23, 1);
                        bool isFPSIMDWithSizeQ = (bits2824 == 0x0E && bit23 == 1);

                        if (isFPSIMDWithSizeQ) {
                            // FP SIMD instructions (FAMAX, FAMIN, etc.) use size field + Q
                            // size at bits[23:22] determines precision:
                            // - 10 (bit[22]=0) → half-precision for FP16 ops
                            // - 11 (bit[22]=1) → single-precision or half-precision depending on operation
                            // Extract full size field
                            uint32_t size = extractBits(opcode, 22, 2);

                            // For half-precision FP16 ops (size[1:0]=10 with bits[22:21]=10)
                            // or size[1:0]=11 (with various interpretations)
                            if (size == 2) {
                                // size=10 → half-precision FP16 operations
                                arrangement = Q ? "8h" : "4h";
                            } else if (size == 3) {
                                // size=11 → need to distinguish between half, single, and double
                                // Check bits[15:10] to distinguish operation class
                                uint32_t bits1510 = extractBits(opcode, 10, 6);
                                if (bits1510 == 0x07) {
                                    // FAMAX/FAMIN half-precision (bits[15:10]=000111)
                                    arrangement = Q ? "8h" : "4h";
                                } else if (bits1510 == 0x37) {
                                    // FAMAX/FAMIN double-precision (bits[15:10]=110111=0x37)
                                    // Q=0 is UNDEFINED per ARM spec, but handle Q=1
                                    arrangement = Q ? "2d" : "1d";
                                } else {
                                    // Other operations - typically single-precision
                                    arrangement = Q ? "4s" : "2s";
                                }
                            } else {
                                // size=00 or 01 → single/double precision (handled in field2Width=0)
                                // Fallback
                                arrangement = Q ? "16b" : "8b";
                            }
                        } else {
                            // Non-FP-SIMD instructions - use comprehensive inference
                            // Key bit ranges for classification:
                            uint32_t bits3129 = extractBits(opcode, 29, 3);  // Top-level class
                            uint32_t bits2321 = extractBits(opcode, 21, 3);  // Type/size indicator
                            uint32_t bits1512 = extractBits(opcode, 12, 4);  // Opcode
                            uint32_t bits1110 = extractBits(opcode, 10, 2);  // Sometimes size

                            // Category 1: FP16 (half-precision) instructions
                            // Pattern: bits[23:21] = 010 OR bits[23:21] = x11 (FP16 suffix)
                            if (bits2321 == 0b010 || (bits2321 & 0b011) == 0b011) {
                                // FP16 operations: .4H/.8H
                                arrangement = Q ? "8h" : "4h";
                            }
                            // Category 2: Table lookup (TBL/TBX) - always bytes
                            // Pattern: bits[31:29] = 001 or 000, bits[15:12] = 0000 or 0001, bits[23:21] = 000
                            else if (bits2321 == 0b000 && (bits1512 == 0b0000 || bits1512 == 0b0001)) {
                                arrangement = Q ? "16b" : "8b";
                            }
                            // Category 3: Logical operations - always bytes
                            // Pattern: bits[15:12] indicates logical ops (various values)
                            // AND, ORR, EOR, BIC, BIT, BIF, BSL, NOT, MVN, ORN
                            // Check bits[23:21] for common logical patterns
                            else if (bits2321 == 0b001 || bits2321 == 0b011 || bits2321 == 0b101 || bits2321 == 0b111) {
                                // Check if it's a logical op (bits[15:12] typically 0001 for 3-reg same)
                                if (bits1512 == 0b0001 || bits1512 == 0b0101 || bits1512 == 0b0110 || bits1512 == 0b0111) {
                                    arrangement = Q ? "16b" : "8b";
                                }
                                // FP conversion/rounding with FP16 (bits[23:21] = x11)
                                else if ((bits2321 & 0b011) == 0b011 && bits1512 >= 0b1000) {
                                    arrangement = Q ? "8h" : "4h";
                                }
                                // Default byte
                                else {
                                    arrangement = Q ? "16b" : "8b";
                                }
                            }
                            // Category 4: Immediate value instructions (MOVI, MVNI, ORR, BIC, FMOV)
                            // Pattern: bits[31:29] = 000, 001, or 010, bits[15:12] = 0000 (MOVI), 0001 (ORR/BIC), 1111 (FMOV)
                            else if ((bits3129 == 0b000 || bits3129 == 0b001 || bits3129 == 0b010) &&
                                     (bits1512 == 0b0000 || bits1512 == 0b0001 || bits1512 == 0b1111)) {
                                // MOVI/MVNI/ORR/BIC typically use bytes
                                // FMOV uses H or S based on bits[11:10]
                                if (bits1512 == 0b1111) {
                                    // FMOV - check bits[11:10] for size
                                    if (bits1110 == 0b11) {
                                        arrangement = Q ? "8h" : "4h";  // FP16
                                    } else {
                                        arrangement = Q ? "4s" : "2s";  // FP32
                                    }
                                } else {
                                    arrangement = Q ? "16b" : "8b";
                                }
                            }
                            // Category 5: DOT products and special ops
                            // Pattern: bits[15:12] = 1001 (DOT), 1100/1110 (FMLAL/FMLSL), etc.
                            else if (bits1512 == 0b1001 || bits1512 == 0b1100 || bits1512 == 0b1110 ||
                                     bits1512 == 0b1111 || bits1512 == 0b0000) {
                                // DOT products typically use bytes or specific element sizes
                                // FDOT, SDOT, UDOT → bytes
                                // FMLAL/FMLSL → half-words
                                // Check bits[23:21] to distinguish
                                if (bits2321 == 0b000 || bits2321 == 0b100) {
                                    // FDOT/DOT variants - bytes
                                    arrangement = Q ? "16b" : "8b";
                                } else if (bits2321 == 0b001) {
                                    // FMLAL variants - half-words
                                    arrangement = Q ? "8h" : "4h";
                                } else if (bits2321 == 0b010) {
                                    // BFDOT, FCVTN - half-words
                                    arrangement = Q ? "8h" : "4h";
                                } else {
                                    arrangement = Q ? "16b" : "8b";
                                }
                            }
                            // Category 6: EXT (extract) - always bytes
                            else if (bits1512 == 0b0000 && bits2321 == 0b000 && bits3129 == 0b010) {
                                arrangement = Q ? "16b" : "8b";
                            }
                            // Default: byte arrangement (safest default)
                            else {
                                arrangement = Q ? "16b" : "8b";
                            }
                        }
                    }
                } else if (op.field2Start == 22 && op.field2Width == 1) {
                    // sz field (bit 22) - FMUL/FABS/etc. floating-point type
                    // Could be standalone or with Q bit - check if Q varies
                    uint32_t Q = extractBits(opcode, 30, 1);
                    uint32_t sz = field2Val & 0x1;

                    // For sz:Q compound field:
                    // sz=0, Q=0 → 2S  (32-bit, 64-bit vector)
                    // sz=0, Q=1 → 4S  (32-bit, 128-bit vector)
                    // sz=1, Q=0 → RESERVED
                    // sz=1, Q=1 → 2D  (64-bit, 128-bit vector)
                    if (sz == 0) {
                        arrangement = Q ? "4s" : "2s";
                    } else {
                        // sz == 1
                        arrangement = Q ? "2d" : "1d";  // Q=0 would be reserved but handle gracefully
                    }
                } else if (op.field2Start == 11 && op.field2Width == 2) {
                    // size field at different position (bits 11-12) - LD1R/ST1R type
                    // Can be standalone or with Q
                    uint32_t Q = extractBits(opcode, 30, 1);
                    uint32_t size = field2Val & 0x3;

                    if (size == 0) arrangement = Q ? "16b" : "8b";   // 8-bit
                    else if (size == 1) arrangement = Q ? "8h" : "4h";    // 16-bit
                    else if (size == 2) arrangement = Q ? "4s" : "2s";    // 32-bit
                    else if (size == 3) arrangement = Q ? "2d" : "1d";    // 64-bit
                } else if (op.field2Width == 0) {
                    // No arrangement field encoded
                    uint32_t Q = extractBits(opcode, 30, 1);

                    // Detect FP SIMD instructions that use size:Q encoding (FAMAX, FAMIN, etc.)
                    // Pattern: bits[28:24]=01110 (SIMD FP class), bit[23]=1 (FP with size field)
                    uint32_t bits2824 = extractBits(opcode, 24, 5);
                    uint32_t bit23 = extractBits(opcode, 23, 1);
                    bool isFPSIMDWithSizeQ = (bits2824 == 0x0E && bit23 == 1);

                    if (isFPSIMDWithSizeQ) {
                        // FP SIMD instructions (FAMAX, FAMIN, etc.) use size<0>:Q encoding
                        // size[1:0] at bits[23:22]:
                        // 10 (bit23=1, bit22=0) → single-precision
                        // 11 (bit23=1, bit22=1) → half-precision or other
                        uint32_t full_size = extractBits(opcode, 22, 2);

                        if (full_size == 2) {
                            // size=10 → single-precision
                            arrangement = Q ? "4s" : "2s";
                        } else if (full_size == 3) {
                            // size=11 → distinguish based on bits[15:10]
                            uint32_t bits1510 = extractBits(opcode, 10, 6);
                            if (bits1510 == 0x07) {
                                // FAMAX/FAMIN half-precision (opcode=000111)
                                arrangement = Q ? "8h" : "4h";
                            } else if (bits1510 == 0x37) {
                                // FAMAX/FAMIN double-precision (opcode=110111=0x37)
                                // Q=0 is UNDEFINED per ARM spec, but handle Q=1
                                arrangement = Q ? "2d" : "1d";
                            } else {
                                // Other FP ops - default to half-precision
                                arrangement = Q ? "8h" : "4h";
                            }
                        } else {
                            // size=00 or 01 - shouldn't happen for typical FP SIMD
                            arrangement = Q ? "16b" : "8b";
                        }
                    } else {
                        // Default to Q bit if present
                        arrangement = Q ? "16b" : "8b";
                    }
                }
                // Additional fallback: try to infer from Q bit alone if field metadata is missing
                else {
                    uint32_t Q = extractBits(opcode, 30, 1);

                    // If we have a field but don't recognize the pattern, try heuristics
                    // based on field position
                    if (op.field2Start >= 10 && op.field2Start <= 12 && op.field2Width == 2) {
                        // Likely a size field at unusual position
                        uint32_t size = field2Val & 0x3;
                        if (size == 0) arrangement = Q ? "16b" : "8b";
                        else if (size == 1) arrangement = Q ? "8h" : "4h";
                        else if (size == 2) arrangement = Q ? "4s" : "2s";
                        else if (size == 3) arrangement = Q ? "2d" : "1d";
                    } else {
                        // Last resort: default based on Q
                        arrangement = Q ? "16b" : "8b";
                    }
                }

                offset += snprintf(buffer + offset, bufferSize - offset, "v%u.%s", field1Val, arrangement);
            }
            break;

        case REG_SIMD_ELEMENT: // indexed element like v1.b[0]
            // field1 = register number (Rd, Rn, etc.)
            // field2 = index field (usually imm5 or imm4)
            // subtype: 0 = imm5-based (lowest set bit determines size)
            //          1 = size field-based
            //          10+ = hardcoded size (UMOV-style: 10=B, 11=H, 12=S, 13=D)
            {
                const char* elemSize = "?";
                unsigned index = 0;

                if (op.subtype >= 10) {
                    // Hardcoded element size (UMOV-style)
                    // subtype encodes size: 10=B, 11=H, 12=S, 13=D
                    const char* sizes[] = {"b", "h", "s", "d"};
                    unsigned sizeIdx = op.subtype - 10;
                    if (sizeIdx < 4)
                        elemSize = sizes[sizeIdx];

                    // Index is in upper bits of field2 (imm5), position depends on element size
                    // B (size=0): index = imm5[4:1] (4 bits)
                    // H (size=1): index = imm5[4:2] (3 bits)
                    // S (size=2): index = imm5[4:3] (2 bits)
                    // D (size=3): index = imm5[4] (1 bit)
                    if (sizeIdx < 4 && op.field2Width >= 4) {
                        unsigned shift = sizeIdx + 1;  // shift = size + 1
                        unsigned mask = (1U << (op.field2Width - shift)) - 1;
                        index = (field2Val >> shift) & mask;
                    } else
                        index = field2Val;  // Fallback
                } else if (op.subtype == 0 && op.field2Width >= 4) {
                    // imm5-based encoding (common for INS, DUP, etc.)
                    // Element size determined by lowest set bit position in imm5[3:0]
                    // Index in upper bits
                    uint32_t imm5Low = field2Val & 0xF;  // imm5[3:0]

                    if (imm5Low & 0x1) {
                        // Bit 0 set → byte (8-bit)
                        elemSize = "b";
                        index = (field2Val >> 1) & ((1U << (op.field2Width - 1)) - 1);  // imm5[4:1]
                    } else if (imm5Low & 0x2) {
                        // Bit 1 set → halfword (16-bit)
                        elemSize = "h";
                        index = (field2Val >> 2) & ((1U << (op.field2Width - 2)) - 1);  // imm5[4:2]
                    } else if (imm5Low & 0x4) {
                        // Bit 2 set → single (32-bit)
                        elemSize = "s";
                        index = (field2Val >> 3) & ((1U << (op.field2Width - 3)) - 1);  // imm5[4:3]
                    } else if (imm5Low & 0x8) {
                        // Bit 3 set → double (64-bit)
                        elemSize = "d";
                        index = (field2Val >> 4) & ((1U << (op.field2Width - 4)) - 1);  // imm5[4]
                    }
                } else if (op.subtype == 1) {
                    // size field-based encoding (2-bit size field)
                    // Would need additional logic based on specific instruction
                    // For now, handle common cases
                    const char* sizes[] = {"b", "h", "s", "d"};
                    elemSize = sizes[field2Val & 0x3];
                    index = 0;  // Would need separate index field
                }

                offset += snprintf(buffer + offset, bufferSize - offset,
                                 "v%u.%s[%u]", field1Val, elemSize, index);
            }
            break;

        case REG_LIST: // register list with hardcoded or variable arrangement
            // field1 = base register number (Rn/Rt)
            // field2 = arrangement field (for variable) or UNUSED (for hardcoded)
            // subtype = (numRegs << 4) | arrangementType
            //   - High 4 bits: number of registers (1-4)
            //   - Low 4 bits: arrangement type (0-14 = hardcoded, 15 = variable from field2)
            {
                const char* hardcodedArrangements[] = {"16b", "8b", "4s", "2s", "8h", "4h", "2d", "1d"};

                // Decode subtype
                unsigned numRegs = (op.subtype >> 4) & 0xF;
                unsigned arrangementType = op.subtype & 0xF;

                const char* arrangement = "16b";

                if (arrangementType == 15) {
                    // Variable arrangement - decode from field2
                    // Common pattern: size:Q (bits 23-22 for size, bit 30 for Q)
                    if (op.field2Width > 0 && op.field2Start < 32) {
                        // Extract field value - for compound fields like "size:Q", field2Start points to first field
                        // Assume size field (2 bits) + Q bit (1 bit)
                        // For LD1: size at bits 11-10, Q at bit 30
                        uint32_t size = extractBits(opcode, 10, 2);  // size field
                        uint32_t Q = extractBits(opcode, 30, 1);     // Q bit

                        // Decode arrangement from size and Q
                        // size=00: 8-bit, size=01: 16-bit, size=10: 32-bit, size=11: 64-bit
                        if (size == 0) arrangement = Q ? "16b" : "8b";   // 8-bit elements
                        else if (size == 1) arrangement = Q ? "8h" : "4h";    // 16-bit elements
                        else if (size == 2) arrangement = Q ? "4s" : "2s";    // 32-bit elements
                        else if (size == 3) arrangement = Q ? "2d" : "1d";    // 64-bit elements
                    }
                } else {
                    // Hardcoded arrangement
                    if (arrangementType < 8)
                        arrangement = hardcodedArrangements[arrangementType];
                }

                // Format register list
                offset += snprintf(buffer + offset, bufferSize - offset, "{ ");
                for (unsigned r = 0; r < numRegs; r++) {
                    if (r > 0)
                        offset += snprintf(buffer + offset, bufferSize - offset, ", ");
                    unsigned regNum = (field1Val + r) & 0x1F;  // Wrap around at 32
                    offset += snprintf(buffer + offset, bufferSize - offset, "v%u.%s", regNum, arrangement);
                }
                offset += snprintf(buffer + offset, bufferSize - offset, " }");
            }
            break;

        // SVE Registers
        case REG_SVE_Z:
            {
                // SVE Z registers typically have element size suffix (.b, .h, .s, .d)
                // For most instructions, element size is encoded in bits [22:23]
                // For load/store (ld*/st*), size is in the mnemonic itself (ld1b, ld1h, ld1w, ld1d)
                const char* sizeStr = ".b";  // default

                // Check if this is a load/store instruction (cache mnemonic prefix check)
                size_t mnemonicLen = strlen(entry->mnemonic);
                bool isLoadStore = mnemonicLen >= 2 && ((entry->mnemonic[0] == 'l' && entry->mnemonic[1] == 'd') || (entry->mnemonic[0] == 's' && entry->mnemonic[1] == 't'));

                if (isLoadStore) {
                    if (mnemonicLen > 0) {
                        char lastChar = entry->mnemonic[mnemonicLen - 1];
                        // Check last character: ld1b/st1b → .b, ld1h/st1h → .h, etc.
                        if (lastChar == 'b') sizeStr = ".b";
                        else if (lastChar == 'h') sizeStr = ".h";
                        else if (lastChar == 'w') sizeStr = ".s";  // word = 32-bit = single
                        else if (lastChar == 'd') sizeStr = ".d";
                        else {
                            // Fallback to bits [22:23]
                            uint32_t size = extractBits(opcode, 22, 2);
                            const char* sizeChars[] = {".b", ".h", ".s", ".d"};
                            sizeStr = sizeChars[size & 0x3];
                        }
                    }
                } else {
                    // For non-load/store, extract from bits [22:23]
                    uint32_t size = extractBits(opcode, 22, 2);
                    const char* sizeChars[] = {".b", ".h", ".s", ".d"};
                    sizeStr = sizeChars[size & 0x3];
                }

                // SVE load/store wraps first Z register in braces: { z0.s }
                if (isLoadStore && i == startOperand) {
                    offset += snprintf(buffer + offset, bufferSize - offset, "{ z%u%s }", field1Val, sizeStr);
                } else {
                    offset += snprintf(buffer + offset, bufferSize - offset, "z%u%s", field1Val, sizeStr);
                }
            }
            break;
        case REG_SVE_P:
            {
                // SVE P (predicate) registers need modifier /m or /z
                // /z = zeroing (for loads)
                // /m = merging (for arithmetic/logical operations)
                // No modifier for stores
                // Detect based on instruction mnemonic
                const char* modifier = "";

                // Check if this is a load instruction (uses /z)
                size_t mnemonicLen = strlen(entry->mnemonic);
                if (mnemonicLen >= 2 && entry->mnemonic[0] == 'l' && entry->mnemonic[1] == 'd') {
                    modifier = "/z";
                }
                // Stores use no modifier
                else if (mnemonicLen >= 2 && entry->mnemonic[0] == 's' && entry->mnemonic[1] == 't') {
                    modifier = "";
                }
                // Arithmetic/logical operations use /m
                else {
                    modifier = "/m";
                }

                offset += snprintf(buffer + offset, bufferSize - offset, "p%u%s", field1Val, modifier);
            }
            break;

        // Immediates
        case IMM_UINT:
            // Special handling for LSL/LSR/ASR shift amount (third operand)
            if (isLslLsrAsr && i == (startOperand + 2)) {
                // This is the shift operand for LSL/LSR/ASR - use computed value
                offset += snprintf(buffer + offset, bufferSize - offset, "#%u", computedShift);
            }
            // Check if this is a bit position (TBNZ/TBZ style: b40 + b5*32)
            else if (op.field1Width == 5 && op.field1Start == 19 &&
                op.field2Width == 1 && op.field2Start == 31) {
                // Bit position: field1=b40 (bits 19-23), field2=b5 (bit 31)
                // position = b5 * 32 + b40
                unsigned bitPos = field2Val * 32 + field1Val;
                offset += snprintf(buffer + offset, bufferSize - offset, "#%u", bitPos);
            }
            // Check if this is a MOV-style immediate with hw shift field
            else if (op.field2Width > 0 && op.field2Start < 32) {
                // MOV/MOVZ/MOVK/MOVN style: imm16 with hw shift (composite operand)
                // Display as: #0x<imm16>, lsl #<shift> (not pre-shifted)
                offset += snprintf(buffer + offset, bufferSize - offset, "#0x%x", field1Val);
                // Add shift if non-zero
                if (field2Val != 0) {
                    unsigned shiftAmount = field2Val * 16;
                    offset += snprintf(buffer + offset, bufferSize - offset, ", lsl #%u", shiftAmount);
                }
            } else if (op.field1Start == 21 && op.field1Width == 2) {
                // This is a standalone hw field (shift amount in MOVK/MOVN as separate operand)
                // Main loop already adds ", " separator, so just output "lsl #<amount>"
                if (field1Val != 0) {
                    unsigned shiftAmount = field1Val * 16;
                    offset += snprintf(buffer + offset, bufferSize - offset, "lsl #%u", shiftAmount);
                }
                // If zero, don't display anything (it's optional)
            } else if (op.field1Start == 5 && op.field1Width == 16) {
                // This is a standalone imm16 field (in MOVK/MOVN as separate operand)
                // Format as hex for consistency with MOV/MOVZ
                offset += snprintf(buffer + offset, bufferSize - offset, "#0x%x", field1Val);
            } else {
                offset += snprintf(buffer + offset, bufferSize - offset, "#%u", field1Val);
            }
            break;

        case IMM_SINT: {
            int32_t signedVal = signExtend(field1Val, op.field1Width);
            offset += snprintf(buffer + offset, bufferSize - offset, "#%d", signedVal);
            break;
        }

        case IMM_HEX:
            offset += snprintf(buffer + offset, bufferSize - offset, "#0x%x", field1Val);
            break;

        case IMM_FLOAT: {
            // Decode ARM64 floating-point immediate (8-bit encoding)
            // imm8 = abcdefgh
            // Expanded format:
            //   Single (32-bit): a[NOT(b)]bbbbbbcd efgh0000000000000000000
            //   Double (64-bit): a[NOT(b)]bbbbbbbbbcd efgh000000000000000000000000000000000000000000000000

            uint32_t imm8 = field1Val & 0xFF;
            uint32_t a = (imm8 >> 7) & 1;  // Sign bit
            uint32_t b = (imm8 >> 6) & 1;
            uint32_t c = (imm8 >> 5) & 1;
            uint32_t d = (imm8 >> 4) & 1;
            uint32_t e = (imm8 >> 3) & 1;
            uint32_t f = (imm8 >> 2) & 1;
            uint32_t g = (imm8 >> 1) & 1;
            uint32_t h = (imm8 >> 0) & 1;

            // Determine precision from opcode bits 22-23 (type/ftype field)
            uint32_t ftype = extractBits(opcode, 22, 2);
            bool isDouble = (ftype & 1) == 1;  // bit 22: 0=single, 1=double

            if (isDouble) {
                // Double precision (64-bit)
                // Format: a [NOT(b):b:b:b:b:b:b:b:b:b:b:c:d] [e:f:g:h:0...0]
                //         ↑  ←------- 11 bits -------→        ←-- 52 bits -→
                uint64_t sign = (uint64_t)a << 63;

                // Build 11-bit exponent: [NOT(b)]:b:b:b:b:b:b:b:b:b:b:c:d
                uint64_t expBits = ((uint64_t)(b ? 0 : 1) << 10) |  // NOT(b)
                                    ((uint64_t)b << 9) | ((uint64_t)b << 8) | ((uint64_t)b << 7) |
                                    ((uint64_t)b << 6) | ((uint64_t)b << 5) | ((uint64_t)b << 4) |
                                    ((uint64_t)b << 3) | ((uint64_t)b << 2) | ((uint64_t)b << 1) |
                                    (uint64_t)b | ((uint64_t)c << 1) | (uint64_t)d;
                uint64_t exp = expBits << 52;  // Shift to exponent position

                // Build mantissa: e:f:g:h in upper 4 bits, rest zeros
                uint64_t mant = ((uint64_t)e << 51) | ((uint64_t)f << 50) |
                                ((uint64_t)g << 49) | ((uint64_t)h << 48);

                uint64_t bits = sign | exp | mant;
                double value;
                memcpy(&value, &bits, sizeof(value));
                offset += snprintf(buffer + offset, bufferSize - offset, "#%.1f", value);
            } else {
                // Single precision (32-bit)
                // Format: a [NOT(b):b:b:b:b:b:c:d] [e:f:g:h:0...0]
                //         ↑  ←----- 8 bits -----→  ←--- 23 bits --→
                uint32_t sign = a << 31;

                // Build 8-bit exponent: [NOT(b)]:b:b:b:b:b:c:d
                uint32_t expBits = ((b ? 0 : 1) << 7) |  // NOT(b)
                                    (b << 6) | (b << 5) | (b << 4) | (b << 3) | (b << 2) |
                                    (c << 1) | d;
                uint32_t exp = expBits << 23;  // Shift to exponent position

                // Build mantissa: e:f:g:h in upper 4 bits, rest zeros
                uint32_t mant = (e << 22) | (f << 21) | (g << 20) | (h << 19);

                uint32_t bits = sign | exp | mant;
                float value;
                memcpy(&value, &bits, sizeof(value));
                offset += snprintf(buffer + offset, bufferSize - offset, "#%.1f", value);
            }
            break;
        }

        case IMM_LOGICAL: {
            // Decode logical immediate
            uint64_t decodedImm;
            bool is64 = (entry->flags & 1) != 0;

            // Extract N, immr, imms fields
            uint32_t n = field2Val; // N field
            uint32_t immr = extractBits(opcode, 16, 6); // immr
            uint32_t imms = field1Val; // imms

            if (decodeLogicalImmediate(n, immr, imms, is64, &decodedImm)) {
                if (is64)
                    offset += snprintf(buffer + offset, bufferSize - offset, "#0x%llx", (unsigned long long)decodedImm);
                else
                    offset += snprintf(buffer + offset, bufferSize - offset, "#0x%x", (uint32_t)decodedImm);
            } else
                offset += snprintf(buffer + offset, bufferSize - offset, "#?");
            break;
        }

        case IMM_SHIFTED: {
            offset += snprintf(buffer + offset, bufferSize - offset, "#%u", field1Val);
            // Add shift if present
            if (field2Val && op.field2Width > 0) {
                unsigned shiftAmount = field2Val * 16; // Common pattern
                offset += snprintf(buffer + offset, bufferSize - offset, ", lsl #%u", shiftAmount);
            }
            break;
        }

        // PC-relative label
        case LABEL_PCREL: {
            int32_t signedOffset = signExtend(field1Val, op.field1Width);
            // PC-relative in ARM64 is in instructions (4 bytes each)
            int64_t target = (int64_t)pc + (signedOffset * 4);

            // Format based on whether target is in range
            if (startPC && endPC && (uint32_t*)target >= startPC && (uint32_t*)target < endPC) {
                unsigned byte_offset = ((uint32_t*)target - startPC) * 4;
                offset += snprintf(buffer + offset, bufferSize - offset, "%p (<%u>)", (void*)target, byte_offset);
            } else
                offset += snprintf(buffer + offset, bufferSize - offset, "%p", (void*)target);
            break;
        }

        // Condition code
        case CONDITION:
            offset += snprintf(buffer + offset, bufferSize - offset, "%s", g_conditionNames[field1Val & 0xf]);
            break;

        // Shift type
        case SHIFT_TYPE:
            // Note: Zero-shift cases are already filtered in pre-check above
            // Two patterns:
            // 1. ADD/SUB immediate (no field bindings): sh bit determines lsl #12
            // 2. Shifted register (has field bindings): shift type + amount from fields
            if (op.field1Width == 0 && op.field2Width == 0) {
                // Pattern 1: ADD/SUB immediate - sh=1 means lsl #12
                // (sh=0 already filtered out, so we only reach here if sh=1)
                offset += snprintf(buffer + offset, bufferSize - offset, "lsl #12");
            } else {
                // Pattern 2: Shifted register - show shift type and amount
                // (imm6=0 already filtered out, so shift amount is non-zero)
                offset += snprintf(buffer + offset, bufferSize - offset, "%s", g_shiftNames[field1Val & 0x3]);
                if (field2Val)
                    offset += snprintf(buffer + offset, bufferSize - offset, " #%u", field2Val);
            }
            break;

        // Extend type
        case EXTEND_TYPE:
            offset += snprintf(buffer + offset, bufferSize - offset, "%s", g_extendNames[field1Val & 0x7]);
            if (field2Val)
                offset += snprintf(buffer + offset, bufferSize - offset, " #%u", field2Val);
            break;

        // Memory addressing modes
        case MEMORY_BASE:
        case MEMORY_OFFSET:
            offset += snprintf(buffer + offset, bufferSize - offset, "[");
            if (field1Val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "sp");
            else if (field1Val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1Val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1Val);

            // Check if field2 is a register offset (load/store register offset addressing)
            // Register offset: field2 is Rm at bits 16-20 (width=5)
            // Immediate offset: field2 is various immediate fields
            if (op.field2Width == 5 && op.field2Start == 16) {
                // This is register offset addressing
                // Extract extend type from bits 13-15 (option field)
                uint32_t option = extractBits(opcode, 13, 3);
                uint32_t shift = extractBits(opcode, 12, 1);  // S bit

                // Determine if offset register is W or X based on option<0>
                bool useWReg = (option & 1) == 0;

                offset += snprintf(buffer + offset, bufferSize - offset, ", ");
                if (useWReg)
                    offset += snprintf(buffer + offset, bufferSize - offset, "w%u", field2Val);
                else
                    offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field2Val);

                // Format extend/shift type
                // Rules for ARM64 load/store register offset:
                // - X register with LSL and no actual shift: Don't show (default/natural alignment)
                // - X register with LSL and shift>0: Show "lsl #N"
                // - W register with any extend: Always show (uxtw, sxtw, etc.)
                // - Any extend with shift>0: Show shift amount
                const char* extendNames[] = {
                    "uxtb", "uxth", "uxtw", "lsl",  // option 0-3
                    "sxtb", "sxth", "sxtw", "sxtx"  // option 4-7
                };

                // Calculate actual shift amount first
                // shift amount = instruction size (bits 30-31): 0=byte, 1=half, 2=word, 3=double
                uint32_t shiftAmount = 0;
                if (shift)
                    shiftAmount = extractBits(opcode, 30, 2);

                // Check if this is LSL with X register and no actual shift
                // option=3 (LSL) or option=7 (SXTX, effectively LSL for X regs)
                // No shift means: S=0 OR calculated shiftAmount is 0
                bool isXLslNoShift = !useWReg && (option == 3 || option == 7) && (shiftAmount == 0);

                // Only output extend/shift if NOT (X register LSL without shift)
                if (!isXLslNoShift) {
                    offset += snprintf(buffer + offset, bufferSize - offset, ", %s", extendNames[option & 0x7]);

                    // Add shift amount if non-zero
                    if (shiftAmount > 0)
                        offset += snprintf(buffer + offset, bufferSize - offset, " #%u", shiftAmount);
                }
            }
            // Add immediate offset if present
            else if (field2Val && op.field2Width > 0) {
                int32_t immOffset = signExtend(field2Val, op.field2Width);

                // Apply scaling for certain immediate fields
                // imm7 is used in load/store pair and needs scaling
                // Check if this is a 7-bit immediate (load/store pair)
                if (op.field2Width == 7) {
                    // Scale by 4 for 32-bit, 8 for 64-bit
                    int scale = (entry->flags & 1) ? 8 : 4;
                    immOffset *= scale;
                }
                // imm9 (9-bit) is unscaled, no multiplication needed

                if (immOffset != 0)
                    offset += snprintf(buffer + offset, bufferSize - offset, ", #%d", immOffset);
            }
            offset += snprintf(buffer + offset, bufferSize - offset, "]");
            break;

        case MEMORY_REG:
            offset += snprintf(buffer + offset, bufferSize - offset, "[");
            if (field1Val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "sp");
            else if (field1Val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1Val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1Val);

            offset += snprintf(buffer + offset, bufferSize - offset, ", ");
            offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field2Val);
            offset += snprintf(buffer + offset, bufferSize - offset, "]");
            break;

        case MEMORY_PREIDX:
            offset += snprintf(buffer + offset, bufferSize - offset, "[");
            if (field1Val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "sp");
            else if (field1Val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1Val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1Val);

            if (op.field2Width > 0) {
                int32_t immOffset = signExtend(field2Val, op.field2Width);

                // Apply scaling for imm7 (load/store pair)
                if (op.field2Width == 7) {
                    int scale = (entry->flags & 1) ? 8 : 4;
                    immOffset *= scale;
                }

                offset += snprintf(buffer + offset, bufferSize - offset, ", #%d", immOffset);
            }
            offset += snprintf(buffer + offset, bufferSize - offset, "]!");
            break;

        case MEMORY_POSTIDX:
            offset += snprintf(buffer + offset, bufferSize - offset, "[");
            if (field1Val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "sp");
            else if (field1Val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1Val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1Val);
            offset += snprintf(buffer + offset, bufferSize - offset, "]");

            if (op.field2Width > 0) {
                int32_t immOffset = signExtend(field2Val, op.field2Width);

                // Apply scaling for imm7 (load/store pair)
                if (op.field2Width == 7) {
                    int scale = (entry->flags & 1) ? 8 : 4;
                    immOffset *= scale;
                }

                offset += snprintf(buffer + offset, bufferSize - offset, ", #%d", immOffset);
            }
            break;

        case UNKNOWN:
            offset += snprintf(buffer + offset, bufferSize - offset, "?");
            break;
        }
    }

    // For LSL/LSR/ASR with only 2 operands, append the shift amount
    if (isLslLsrAsr && entry->operandCount == 2 + (hasConditionSuffix ? 1 : 0)) {
        // Add separator and shift amount
        if (offset > 0 && (size_t)offset < bufferSize)
            offset += snprintf(buffer + offset, bufferSize - offset, ", #%u", computedShift);
    }
}

"""

    def _copyright(self) -> str:
        return """/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated by generate_arm64_disassembler.py
 * Complete implementation with field metadata and all formatters
 */

"""


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <xml_directory> <output_directory>")
        sys.exit(1)

    xml_dir = sys.argv[1]
    output_dir = sys.argv[2]

    if not os.path.isdir(xml_dir):
        print(f"Error: {xml_dir} is not a directory")
        sys.exit(1)

    os.makedirs(output_dir, exist_ok=True)

    # Parse
    print("Parsing ARM64 instruction XML files...")
    parser = ARM64InstructionParser(xml_dir)
    instructions = parser.parse_all()

    if parser.errors:
        print("\nErrors encountered:")
        for error in parser.errors[:10]:
            print(f"  {error}")
        if len(parser.errors) > 10:
            print(f"  ... and {len(parser.errors) - 10} more")

    # Generate
    print(f"\nGenerating complete C++ code with formatters...")
    generator = CodeGenerator(instructions, output_dir)
    generator.generate_all()

    print(f"\nGenerated disassembler for {len(instructions)} instruction encodings")
    print(f"Output written to: {output_dir}")
    print("\nGenerated files:")
    print("  - A64InstructionTable.h (API)")
    print("  - A64InstructionTable.cpp (Complete implementation)")

if __name__ == '__main__':
    main()
