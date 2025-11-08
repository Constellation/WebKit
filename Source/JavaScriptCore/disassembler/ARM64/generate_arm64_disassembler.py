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
    'is_64bit_variant', 'aliases'
])

BitField = namedtuple('BitField', [
    'name', 'bit_start', 'bit_width', 'is_fixed', 'fixed_value'
])

Operand = namedtuple('Operand', [
    'type', 'subtype', 'field_name', 'field_name2', 'is_optional', 'description'
])

class FieldMetadata:
    """Metadata for instruction fields"""
    def __init__(self, name: str, bit_start: int, bit_width: int):
        self.name = name
        self.bit_start = bit_start
        self.bit_width = bit_width

class ARM64InstructionParser:
    """Enhanced parser with complete field tracking"""

    def __init__(self, xml_directory: str):
        self.xml_directory = xml_directory
        self.instructions: List[InstructionEncoding] = []
        self.errors: List[str] = []
        self.field_metadata: Dict[str, FieldMetadata] = {}

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
        print(f"Collected {len(self.field_metadata)} unique fields with positions")

        if self.errors:
            print(f"Encountered {len(self.errors)} errors")

        return self.instructions

    def _parse_file(self, xml_path: str):
        """Parse a single XML file"""
        tree = ET.parse(xml_path)
        root = tree.getroot()

        mnemonic = None
        docvars = root.find('.//docvars')
        if docvars is not None:
            mnemonic_elem = docvars.find("./docvar[@key='mnemonic']")
            if mnemonic_elem is not None:
                mnemonic = mnemonic_elem.get('value')

        for iclass in root.findall('.//iclass'):
            regdiagram = iclass.find('regdiagram')
            if regdiagram is None:
                continue

            for encoding in iclass.findall('.//encoding'):
                try:
                    instr = self._parse_encoding(encoding, regdiagram, mnemonic,
                                                 os.path.basename(xml_path), root)
                    if instr:
                        self.instructions.append(instr)
                except Exception as e:
                    self.errors.append(f"Error in {xml_path}: {e}")

    def _parse_encoding(self, encoding_elem, regdiagram, default_mnemonic: str,
                       xml_file: str, root) -> Optional[InstructionEncoding]:
        """Parse encoding with field position tracking"""
        encoding_name = encoding_elem.get('name')
        if not encoding_name:
            return None

        fields = self._parse_regdiagram(regdiagram)

        # Store field metadata
        for field in fields:
            if not field.is_fixed and field.name not in self.field_metadata:
                self.field_metadata[field.name] = FieldMetadata(
                    field.name, field.bit_start, field.bit_width)

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
                    # Update metadata
                    if not ef.is_fixed and ef.name not in self.field_metadata:
                        self.field_metadata[ef.name] = FieldMetadata(
                            ef.name, ef.bit_start, ef.bit_width)
                fields = list(field_dict.values())

        if not fields:
            return None

        mask, pattern = self._calculate_mask_and_pattern(fields)
        is_64bit = self._is_64bit_variant(fields, encoding_elem)

        asmtemplate_elem = encoding_elem.find('.//asmtemplate')
        mnemonic = default_mnemonic
        operands = []

        if asmtemplate_elem is not None:
            parsed_mnemonic, operands = self._parse_asmtemplate(
                asmtemplate_elem, fields, is_64bit, encoding_elem)
            if parsed_mnemonic:
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
            aliases=aliases
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
                          is_64bit: bool, encoding_elem) -> Tuple[str, List[Operand]]:
        """Parse assembly template with proper memory operand detection"""
        mnemonic = None
        operands = []
        field_map = {f.name: f for f in fields}

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

        # Extract mnemonic
        for i, part in enumerate(parts):
            if part[0] == 'text':
                text = part[1].strip()
                if i == 0 and not mnemonic:
                    match = re.match(r'^([A-Z][A-Z0-9.]*)', text)
                    if match:
                        mnemonic = match.group(1)
                        break

        # Parse operands with memory grouping
        i = 0
        last_r_option = None  # Track R_option (width specifier) for next register

        while i < len(parts):
            part = parts[i]

            if part[0] == 'operand':
                link, hover = part[1], part[2]

                # Check if this is R_option (width specifier)
                if link.lower() in ['r_option', 'r_option__2', 'r_option__3']:
                    # Store hover to determine width for next operand
                    last_r_option = hover
                    i += 1
                    continue

                # Look ahead for memory operand pattern: operand followed by ", ["
                if i + 1 < len(parts) and parts[i + 1][0] == 'text':
                    next_text = parts[i + 1][1]

                    # Check if this starts a memory operand
                    if ', [' in next_text or ',[' in next_text:
                        # This operand is not part of memory, add it normally
                        operand = self._infer_operand(link, hover, field_map, is_64bit, last_r_option)
                        if operand:
                            operands.append(operand)
                            last_r_option = None  # Reset after use
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
                operand = self._infer_operand(link, hover, field_map, is_64bit, last_r_option)
                if operand:
                    operands.append(operand)
                    last_r_option = None  # Reset after use

            i += 1

        return mnemonic, operands

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

    def _infer_operand(self, link: str, hover: str, field_map: Dict,
                      is_64bit: bool, r_option_hover: Optional[str] = None) -> Optional[Operand]:
        """Infer operand type with field mapping"""
        field_names = re.findall(r'"([A-Za-z0-9_]+)"', hover)
        primary_field = field_names[0] if field_names else None
        secondary_field = field_names[1] if len(field_names) > 1 else None
        is_optional = '{' in link or 'optional' in hover.lower()

        link_lower = link.lower()
        hover_lower = hover.lower()

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

        # FP registers
        if any(p in link_lower for p in ['bd', 'bn']): return Operand('REG_FP_B', None, primary_field or 'Rd', None, is_optional, hover)
        if any(p in link_lower for p in ['hd', 'hn']): return Operand('REG_FP_H', None, primary_field or 'Rd', None, is_optional, hover)
        if any(p in link_lower for p in ['sd', 'sn']): return Operand('REG_FP_S', None, primary_field or 'Rd', None, is_optional, hover)
        if any(p in link_lower for p in ['dd', 'dn']): return Operand('REG_FP_D', None, primary_field or 'Rd', None, is_optional, hover)
        if any(p in link_lower for p in ['qd', 'qn']): return Operand('REG_FP_Q', None, primary_field or 'Rd', None, is_optional, hover)
        if any(p in link_lower for p in ['vd', 'vn', 'vm']): return Operand('REG_SIMD_V', None, primary_field or 'Rd', None, is_optional, hover)

        # SVE
        if any(p in link_lower for p in ['zd', 'zn', 'zm', 'za']): return Operand('REG_SVE_Z', None, primary_field or 'Zd', None, is_optional, hover)
        if any(p in link_lower for p in ['pd', 'pn', 'pm', 'pg']): return Operand('REG_SVE_P', None, primary_field or 'Pd', None, is_optional, hover)

        # Immediates
        if 'imm' in link_lower:
            if 'logical' in hover_lower:
                return Operand('IMM_LOGICAL', None, primary_field or 'imm', 'N', is_optional, hover)
            elif 'shift' in hover_lower:
                return Operand('IMM_SHIFTED', None, primary_field or 'imm', 'sh', is_optional, hover)
            elif 'float' in hover_lower or 'fp' in hover_lower:
                return Operand('IMM_FLOAT', None, primary_field or 'imm', None, is_optional, hover)
            elif 'signed' in hover_lower or 'offset' in hover_lower:
                return Operand('IMM_SINT', None, primary_field or 'imm', None, is_optional, hover)
            return Operand('IMM_UINT', None, primary_field or 'imm', None, is_optional, hover)

        # Labels
        if 'label' in link_lower or ('offset' in hover_lower and 'pc' in hover_lower):
            return Operand('LABEL_PCREL', None, primary_field or 'imm', None, is_optional, hover)

        # Condition
        if 'cond' in link_lower:
            return Operand('CONDITION', None, 'cond', None, is_optional, hover)

        # Shift/extend
        if 'shift' in link_lower or 'shift_option' in link_lower:
            return Operand('SHIFT_TYPE', None, 'shift', 'amount', is_optional, hover)
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
            is_fixed = settings is not None
            fixed_value = 0

            if is_fixed and settings:
                fixed_bits = []
                for child in box:
                    if child.tag == 'c' and child.text and child.text.strip():
                        fixed_bits.append(child.text.strip())

                if fixed_bits:
                    binary_str = ''.join(fixed_bits).replace('x', '0')
                    if all(c in '01' for c in binary_str):
                        fixed_value = int(binary_str, 2)

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
        'REG_FP_B': 10, 'REG_FP_H': 11, 'REG_FP_S': 12,
        'REG_FP_D': 13, 'REG_FP_Q': 14, 'REG_SIMD_V': 15,
        'REG_SVE_Z': 20, 'REG_SVE_P': 21,
        'IMM_UINT': 30, 'IMM_SINT': 31, 'IMM_HEX': 32,
        'IMM_FLOAT': 33, 'IMM_LOGICAL': 34, 'IMM_SHIFTED': 35,
        'LABEL_PCREL': 40,
        'CONDITION': 50, 'SHIFT_TYPE': 51, 'EXTEND_TYPE': 52,
        'MEMORY_BASE': 60, 'MEMORY_OFFSET': 61, 'MEMORY_REG': 62,
        'MEMORY_PREIDX': 63, 'MEMORY_POSTIDX': 64,
    }

    def __init__(self, instructions: List[InstructionEncoding],
                 field_metadata: Dict[str, FieldMetadata], output_dir: str):
        self.instructions = instructions
        self.field_metadata = field_metadata
        self.output_dir = output_dir

        # Sort by specificity
        self.instructions.sort(key=lambda x: bin(x.mask).count('1'), reverse=True)

        # Build indices
        self.field_names = sorted(field_metadata.keys())
        self.field_index = {name: i for i, name in enumerate(self.field_names)}

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

namespace JSC { namespace ARM64Disassembler {

// Instruction entry
struct InstructionEntry {
    const char* name;
    const char* mnemonic;
    uint32_t mask;
    uint32_t pattern;
    uint16_t operandOffset;
    uint8_t operandCount;
    uint8_t flags;
};

// Operand descriptor
struct OperandDesc {
    uint8_t type;
    uint8_t subtype;
    uint8_t field1_start;
    uint8_t field1_width;
    uint8_t field2_start;
    uint8_t field2_width;
};

// Field metadata
struct FieldMeta {
    const char* name;
    uint8_t bitStart;
    uint8_t bitWidth;
};

// Tables
extern const InstructionEntry g_instructionTable[];
extern const size_t g_instructionTableSize;
extern const OperandDesc g_operandTable[];
extern const FieldMeta g_fieldMetadata[];
extern const size_t g_fieldMetadataSize;

// API
const InstructionEntry* findInstruction(uint32_t opcode);
void formatInstruction(const InstructionEntry* entry, uint32_t opcode,
                      uint32_t* pc, uint32_t* startPC, uint32_t* endPC,
                      char* buffer, size_t bufferSize);

}} // namespace JSC::ARM64Disassembler
""")

    def _generate_implementation(self):
        """Generate complete implementation"""
        with open(os.path.join(self.output_dir, 'A64InstructionTable.cpp'), 'w') as f:
            f.write(self._copyright())
            f.write("""
#include "config.h"

#if ENABLE(ARM64_DISASSEMBLER)

#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>
#include <string>

namespace JSC { namespace ARM64Disassembler {

// Helper functions
static inline uint32_t extractBits(uint32_t value, unsigned start, unsigned width) {
    return (value >> start) & ((1U << width) - 1);
}

static inline int32_t signExtend(uint32_t value, unsigned bits) {
    if (bits == 0 || bits >= 32) return (int32_t)value;
    uint32_t sign_bit = 1U << (bits - 1);
    if (value & sign_bit)
        return (int32_t)(value | (~0U << bits));
    return (int32_t)value;
}

static inline int64_t signExtend64(uint64_t value, unsigned bits) {
    if (bits == 0 || bits >= 64) return (int64_t)value;
    uint64_t sign_bit = 1ULL << (bits - 1);
    if (value & sign_bit)
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

            # Generate field metadata table
            f.write(self._generate_field_metadata())

            # Generate operand table
            f.write(self._generate_operand_table())

            # Generate instruction table
            f.write(self._generate_instruction_table())

            # Generate logical immediate decoder
            f.write(self._generate_logical_immediate_decoder())

            # Generate instruction finder
            f.write(self._generate_finder())

            # Generate complete formatter
            f.write(self._generate_complete_formatter())

            f.write("\n}} // namespace JSC::ARM64Disassembler\n\n")
            f.write("#endif // ENABLE(ARM64_DISASSEMBLER)\n")

    def _generate_field_metadata(self) -> str:
        """Generate field metadata table"""
        code = "// Field metadata table\n"
        code += "const FieldMeta g_fieldMetadata[] = {\n"

        for name in self.field_names:
            meta = self.field_metadata[name]
            code += f'    {{ "{meta.name}", {meta.bit_start}, {meta.bit_width} }},\n'

        code += "};\n\n"
        code += f"const size_t g_fieldMetadataSize = {len(self.field_names)};\n\n"
        return code

    def _generate_operand_table(self) -> str:
        """Generate operand table"""
        code = "// Operand table\n"
        code += "const OperandDesc g_operandTable[] = {\n"

        operand_offset = 0
        self.instruction_operand_info = []

        for instr in self.instructions:
            start_offset = operand_offset
            operand_count = len(instr.operands) if instr.operands else 0

            if instr.operands:
                for op in instr.operands:
                    # Get bit positions from instruction's field dict
                    field1_start, field1_width = instr.fields.get(op.field_name, (255, 0))
                    field2_start, field2_width = instr.fields.get(op.field_name2, (255, 0)) if op.field_name2 else (255, 0)
                    op_type_val = self.OP_TYPES.get(op.type, 0)

                    code += f"    {{ {op_type_val}, 0, {field1_start}, {field1_width}, {field2_start}, {field2_width} }},\n"
                    operand_offset += 1

            self.instruction_operand_info.append((start_offset, operand_count))

        if operand_offset == 0:
            code += "    { 0, 0, 255, 0, 255, 0 }\n"

        code += "};\n\n"
        return code

    def _generate_instruction_table(self) -> str:
        """Generate instruction table"""
        code = "// Instruction table\n"
        code += "const InstructionEntry g_instructionTable[] = {\n"

        for i, instr in enumerate(self.instructions):
            start_offset, operand_count = self.instruction_operand_info[i]
            flags = 1 if instr.is_64bit_variant else 0

            # Convert mnemonic to lowercase for table
            lowercase_mnemonic = instr.mnemonic.lower()

            code += f"    {{ \"{instr.name}\", \"{lowercase_mnemonic}\", "
            code += f"0x{instr.mask:08x}U, 0x{instr.pattern:08x}U, "
            code += f"{start_offset}, {operand_count}, {flags} }},\n"

        code += "};\n\n"
        code += f"const size_t g_instructionTableSize = {len(self.instructions)};\n\n"
        return code

    def _generate_logical_immediate_decoder(self) -> str:
        """Generate ARM64 logical immediate decoding algorithm"""
        return """
// Decode ARM64 logical immediate
// Based on ARM Architecture Reference Manual, pseudocode for DecodeBitMasks
static bool decodeLogicalImmediate(uint32_t n, uint32_t immr, uint32_t imms,
                                   bool is64bit, uint64_t* result) {
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
        """Generate instruction finder"""
        return """
const InstructionEntry* findInstruction(uint32_t opcode) {
    // Linear search (optimized to binary search later)
    for (size_t i = 0; i < g_instructionTableSize; i++) {
        const auto& entry = g_instructionTable[i];
        if ((opcode & entry.mask) == entry.pattern)
            return &entry;
    }
    return nullptr;
}

"""

    def _generate_complete_formatter(self) -> str:
        """Generate complete formatter with all operand types"""
        return """
void formatInstruction(const InstructionEntry* entry, uint32_t opcode,
                      uint32_t* pc, uint32_t* startPC, uint32_t* endPC,
                      char* buffer, size_t bufferSize) {
    if (!entry) {
        snprintf(buffer, bufferSize, "   .long      0x%08x", opcode);
        return;
    }

    // Check if first operand is a condition code for conditional branches
    bool hasConditionSuffix = false;
    const char* conditionCode = nullptr;
    if (entry->operandCount > 0) {
        const auto& firstOp = g_operandTable[entry->operandOffset];
        if (firstOp.type == 50) { // CONDITION
            hasConditionSuffix = true;
            if (firstOp.field1_width > 0 && firstOp.field1_start < 32) {
                uint32_t cond = extractBits(opcode, firstOp.field1_start, firstOp.field1_width);
                conditionCode = g_conditionNames[cond & 0xf];
            }
        }
    }

    // Format mnemonic (with optional condition suffix)
    // Mnemonic is already lowercase in table
    int offset;
    if (hasConditionSuffix && conditionCode) {
        // Check if mnemonic already ends with a dot (like "b.")
        std::string mnemonicStr(entry->mnemonic);
        if (!mnemonicStr.empty() && mnemonicStr.back() == '.') {
            // Already has dot, just append condition
            offset = snprintf(buffer, bufferSize, "   %-9s",
                             (mnemonicStr + conditionCode).c_str());
        } else {
            // Add dot before condition
            offset = snprintf(buffer, bufferSize, "   %-9s",
                             (mnemonicStr + "." + conditionCode).c_str());
        }
    } else {
        offset = snprintf(buffer, bufferSize, "   %-9s", entry->mnemonic);
    }
    if (offset < 0 || (size_t)offset >= bufferSize)
        return;

    // Format each operand (skip first if it was a condition code)
    unsigned startOperand = hasConditionSuffix ? 1 : 0;
    for (unsigned i = startOperand; i < entry->operandCount; i++) {
        const auto& op = g_operandTable[entry->operandOffset + i];

        // Add separator
        if (i > startOperand && offset > 0 && (size_t)offset < bufferSize) {
            offset += snprintf(buffer + offset, bufferSize - offset, ", ");
        }

        if (offset < 0 || (size_t)offset >= bufferSize)
            return;

        // Extract field values
        uint32_t field1_val = 0;
        uint32_t field2_val = 0;

        if (op.field1_width > 0 && op.field1_start < 32) {
            field1_val = extractBits(opcode, op.field1_start, op.field1_width);
        }

        if (op.field2_width > 0 && op.field2_start < 32) {
            field2_val = extractBits(opcode, op.field2_start, op.field2_width);
        }

        // Format based on operand type
        switch (op.type) {
        // GP Registers
        case 0: // REG_GPR_X
            if (field1_val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1_val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1_val);
            break;

        case 1: // REG_GPR_W
            offset += snprintf(buffer + offset, bufferSize - offset, "w%u", field1_val);
            break;

        case 2: // REG_GPR_SP
        case 3: // REG_GPR_XSP
            if (field1_val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "sp");
            else if (field1_val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1_val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1_val);
            break;

        case 4: // REG_GPR_WSP
            if (field1_val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "wsp");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "w%u", field1_val);
            break;

        case 5: // REG_GPR_XZR
            if (field1_val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "xzr");
            else if (field1_val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1_val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1_val);
            break;

        case 6: // REG_GPR_WZR
            if (field1_val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "wzr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "w%u", field1_val);
            break;

        // FP Registers
        case 10: // REG_FP_B
            offset += snprintf(buffer + offset, bufferSize - offset, "b%u", field1_val);
            break;
        case 11: // REG_FP_H
            offset += snprintf(buffer + offset, bufferSize - offset, "h%u", field1_val);
            break;
        case 12: // REG_FP_S
            offset += snprintf(buffer + offset, bufferSize - offset, "s%u", field1_val);
            break;
        case 13: // REG_FP_D
            offset += snprintf(buffer + offset, bufferSize - offset, "d%u", field1_val);
            break;
        case 14: // REG_FP_Q
            offset += snprintf(buffer + offset, bufferSize - offset, "q%u", field1_val);
            break;
        case 15: // REG_SIMD_V
            offset += snprintf(buffer + offset, bufferSize - offset, "v%u", field1_val);
            break;

        // SVE Registers
        case 20: // REG_SVE_Z
            offset += snprintf(buffer + offset, bufferSize - offset, "z%u", field1_val);
            break;
        case 21: // REG_SVE_P
            offset += snprintf(buffer + offset, bufferSize - offset, "p%u", field1_val);
            break;

        // Immediates
        case 30: // IMM_UINT
            offset += snprintf(buffer + offset, bufferSize - offset, "#%u", field1_val);
            break;

        case 31: { // IMM_SINT
            int32_t signed_val = signExtend(field1_val, op.field1_width);
            offset += snprintf(buffer + offset, bufferSize - offset, "#%d", signed_val);
            break;
        }

        case 32: // IMM_HEX
            offset += snprintf(buffer + offset, bufferSize - offset, "#0x%x", field1_val);
            break;

        case 34: { // IMM_LOGICAL
            // Decode logical immediate
            uint64_t decoded_imm;
            bool is64 = (entry->flags & 1) != 0;

            // Extract N, immr, imms fields
            uint32_t n = field2_val; // N field
            uint32_t immr = extractBits(opcode, 16, 6); // immr
            uint32_t imms = field1_val; // imms

            if (decodeLogicalImmediate(n, immr, imms, is64, &decoded_imm)) {
                if (is64)
                    offset += snprintf(buffer + offset, bufferSize - offset,
                                     "#0x%llx", (unsigned long long)decoded_imm);
                else
                    offset += snprintf(buffer + offset, bufferSize - offset,
                                     "#0x%x", (uint32_t)decoded_imm);
            } else {
                offset += snprintf(buffer + offset, bufferSize - offset, "#?");
            }
            break;
        }

        case 35: { // IMM_SHIFTED
            offset += snprintf(buffer + offset, bufferSize - offset, "#%u", field1_val);
            // Add shift if present
            if (field2_val && op.field2_width > 0) {
                unsigned shift_amount = field2_val * 16; // Common pattern
                offset += snprintf(buffer + offset, bufferSize - offset,
                                 ", lsl #%u", shift_amount);
            }
            break;
        }

        // PC-relative label
        case 40: { // LABEL_PCREL
            int32_t signed_offset = signExtend(field1_val, op.field1_width);
            // PC-relative in ARM64 is in instructions (4 bytes each)
            int64_t target = (int64_t)pc + (signed_offset * 4);

            // Format based on whether target is in range
            if (startPC && endPC && (uint32_t*)target >= startPC && (uint32_t*)target < endPC) {
                unsigned byte_offset = ((uint32_t*)target - startPC) * 4;
                offset += snprintf(buffer + offset, bufferSize - offset,
                                 "0x%p (<%u>)", (void*)target, byte_offset);
            } else {
                offset += snprintf(buffer + offset, bufferSize - offset,
                                 "0x%p", (void*)target);
            }
            break;
        }

        // Condition code
        case 50: // CONDITION
            offset += snprintf(buffer + offset, bufferSize - offset, "%s",
                             g_conditionNames[field1_val & 0xf]);
            break;

        // Shift type
        case 51: // SHIFT_TYPE
            offset += snprintf(buffer + offset, bufferSize - offset, "%s",
                             g_shiftNames[field1_val & 0x3]);
            if (field2_val && op.field2_width > 0) {
                offset += snprintf(buffer + offset, bufferSize - offset,
                                 " #%u", field2_val);
            }
            break;

        // Extend type
        case 52: // EXTEND_TYPE
            offset += snprintf(buffer + offset, bufferSize - offset, "%s",
                             g_extendNames[field1_val & 0x7]);
            if (field2_val) {
                offset += snprintf(buffer + offset, bufferSize - offset,
                                 " #%u", field2_val);
            }
            break;

        // Memory addressing modes
        case 60: // MEMORY_BASE
        case 61: // MEMORY_OFFSET
            offset += snprintf(buffer + offset, bufferSize - offset, "[");
            if (field1_val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "sp");
            else if (field1_val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1_val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1_val);

            // Add offset if present
            if (field2_val && op.field2_width > 0) {
                int32_t imm_offset = signExtend(field2_val, op.field2_width);

                // Apply scaling for certain immediate fields
                // imm7 is used in load/store pair and needs scaling
                // Check if this is a 7-bit immediate (load/store pair)
                if (op.field2_width == 7) {
                    // Scale by 4 for 32-bit, 8 for 64-bit
                    int scale = (entry->flags & 1) ? 8 : 4;
                    imm_offset *= scale;
                }
                // imm9 (9-bit) is unscaled, no multiplication needed

                if (imm_offset != 0) {
                    offset += snprintf(buffer + offset, bufferSize - offset,
                                     ", #%d", imm_offset);
                }
            }
            offset += snprintf(buffer + offset, bufferSize - offset, "]");
            break;

        case 62: // MEMORY_REG
            offset += snprintf(buffer + offset, bufferSize - offset, "[");
            if (field1_val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "sp");
            else if (field1_val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1_val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1_val);

            offset += snprintf(buffer + offset, bufferSize - offset, ", ");
            offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field2_val);
            offset += snprintf(buffer + offset, bufferSize - offset, "]");
            break;

        case 63: // MEMORY_PREIDX
            offset += snprintf(buffer + offset, bufferSize - offset, "[");
            if (field1_val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "sp");
            else if (field1_val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1_val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1_val);

            if (op.field2_width > 0) {
                int32_t imm_offset = signExtend(field2_val, op.field2_width);

                // Apply scaling for imm7 (load/store pair)
                if (op.field2_width == 7) {
                    int scale = (entry->flags & 1) ? 8 : 4;
                    imm_offset *= scale;
                }

                offset += snprintf(buffer + offset, bufferSize - offset,
                                 ", #%d", imm_offset);
            }
            offset += snprintf(buffer + offset, bufferSize - offset, "]!");
            break;

        case 64: // MEMORY_POSTIDX
            offset += snprintf(buffer + offset, bufferSize - offset, "[");
            if (field1_val == 31)
                offset += snprintf(buffer + offset, bufferSize - offset, "sp");
            else if (field1_val == 29)
                offset += snprintf(buffer + offset, bufferSize - offset, "fp");
            else if (field1_val == 30)
                offset += snprintf(buffer + offset, bufferSize - offset, "lr");
            else
                offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1_val);
            offset += snprintf(buffer + offset, bufferSize - offset, "]");

            if (op.field2_width > 0) {
                int32_t imm_offset = signExtend(field2_val, op.field2_width);

                // Apply scaling for imm7 (load/store pair)
                if (op.field2_width == 7) {
                    int scale = (entry->flags & 1) ? 8 : 4;
                    imm_offset *= scale;
                }

                offset += snprintf(buffer + offset, bufferSize - offset,
                                 ", #%d", imm_offset);
            }
            break;

        default:
            offset += snprintf(buffer + offset, bufferSize - offset, "?");
            break;
        }
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
    generator = CodeGenerator(instructions, parser.field_metadata, output_dir)
    generator.generate_all()

    print(f"\nGenerated disassembler for {len(instructions)} instruction encodings")
    print(f"Field metadata: {len(generator.field_names)} unique fields")
    print(f"Output written to: {output_dir}")
    print("\nGenerated files:")
    print("  - A64InstructionTableV3.h (API)")
    print("  - A64InstructionTableV3.cpp (Complete implementation)")

if __name__ == '__main__':
    main()
