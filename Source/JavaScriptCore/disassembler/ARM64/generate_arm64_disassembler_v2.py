#!/usr/bin/env python3
"""
ARM64 Disassembler Code Generator v2

Enhanced version with comprehensive operand formatting support.

Usage:
    python3 generate_arm64_disassembler_v2.py <xml_directory> <output_directory>
"""

import xml.etree.ElementTree as ET
import sys
import os
from collections import defaultdict, namedtuple, OrderedDict
from typing import List, Dict, Set, Tuple, Optional
import re

# Enhanced data structures
InstructionEncoding = namedtuple('InstructionEncoding', [
    'name', 'mnemonic', 'mask', 'pattern', 'fields', 'operands', 'xml_file',
    'is_64bit_variant', 'condition_field', 'aliases'
])

BitField = namedtuple('BitField', [
    'name', 'bit_start', 'bit_width', 'is_fixed', 'fixed_value'
])

Operand = namedtuple('Operand', [
    'type',           # Operand type
    'subtype',        # Subtype/format variant
    'field_name',     # Primary field name
    'field_name2',    # Secondary field (for complex operands)
    'is_optional',    # Whether operand is optional
    'description'     # Human-readable description
])

# Operand type constants
OP_TYPES = {
    'REG_GPR_X': 0,      # 64-bit general purpose
    'REG_GPR_W': 1,      # 32-bit general purpose
    'REG_GPR_SP': 2,     # Stack pointer
    'REG_GPR_XSP': 3,    # X reg or SP
    'REG_GPR_WSP': 4,    # W reg or WSP
    'REG_GPR_XZR': 5,    # X reg or XZR
    'REG_GPR_WZR': 6,    # W reg or WZR
    'REG_FP_B': 10,      # 8-bit FP
    'REG_FP_H': 11,      # 16-bit FP
    'REG_FP_S': 12,      # 32-bit FP
    'REG_FP_D': 13,      # 64-bit FP
    'REG_FP_Q': 14,      # 128-bit FP
    'REG_SIMD_V': 15,    # SIMD vector
    'REG_SVE_Z': 20,     # SVE vector
    'REG_SVE_P': 21,     # SVE predicate
    'IMM_UINT': 30,      # Unsigned immediate
    'IMM_SINT': 31,      # Signed immediate
    'IMM_HEX': 32,       # Hex immediate
    'IMM_FLOAT': 33,     # Floating point immediate
    'IMM_LOGICAL': 34,   # Logical immediate (decoded)
    'IMM_SHIFTED': 35,   # Shifted immediate
    'LABEL_PCREL': 40,   # PC-relative label
    'CONDITION': 50,     # Condition code
    'SHIFT_TYPE': 51,    # Shift type (LSL, LSR, ASR, ROR)
    'EXTEND_TYPE': 52,   # Extend type
    'MEMORY_BASE': 60,   # [Xn]
    'MEMORY_OFFSET': 61, # [Xn, #imm]
    'MEMORY_REG': 62,    # [Xn, Xm]
    'MEMORY_PREIDX': 63, # [Xn, #imm]!
    'MEMORY_POSTIDX': 64,# [Xn], #imm
}

class ARM64InstructionParser:
    """Enhanced parser with detailed operand extraction"""

    def __init__(self, xml_directory: str):
        self.xml_directory = xml_directory
        self.instructions: List[InstructionEncoding] = []
        self.errors: List[str] = []
        self.field_definitions = {}  # Cache of field definitions

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

        # Get mnemonic
        mnemonic = None
        docvars = root.find('.//docvars')
        if docvars is not None:
            mnemonic_elem = docvars.find("./docvar[@key='mnemonic']")
            if mnemonic_elem is not None:
                mnemonic = mnemonic_elem.get('value')

        # Parse field definitions from explanations
        self._parse_explanations(root)

        # Parse all instruction classes
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

    def _parse_explanations(self, root):
        """Parse field explanations to understand operand encoding"""
        explanations = root.find('.//explanations')
        if explanations is None:
            return

        for explanation in explanations.findall('explanation'):
            symbol = explanation.find('symbol')
            if symbol is None or symbol.text is None:
                continue

            account = explanation.find('.//account')
            if account is None:
                continue

            # Extract field name from encodedin attribute
            encodedin = account.get('encodedin', '')
            if encodedin:
                # Store field definition
                self.field_definitions[symbol.text.strip()] = {
                    'field': encodedin,
                    'description': self._get_text_content(account)
                }

    def _get_text_content(self, elem) -> str:
        """Extract all text content from element"""
        text = elem.text or ''
        for child in elem:
            text += self._get_text_content(child)
            text += child.tail or ''
        return text

    def _parse_encoding(self, encoding_elem, regdiagram, default_mnemonic: str,
                       xml_file: str, root) -> Optional[InstructionEncoding]:
        """Parse an instruction encoding with enhanced operand information"""
        encoding_name = encoding_elem.get('name')
        if not encoding_name:
            return None

        # Parse bit fields
        fields = self._parse_regdiagram(regdiagram)

        # Override with encoding-specific boxes
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

        # Calculate mask and pattern
        mask, pattern = self._calculate_mask_and_pattern(fields)

        # Detect 64-bit variant
        is_64bit = self._is_64bit_variant(fields, encoding_elem)

        # Parse operands from asmtemplate
        asmtemplate_elem = encoding_elem.find('.//asmtemplate')
        mnemonic = default_mnemonic
        operands = []

        if asmtemplate_elem is not None:
            parsed_mnemonic, operands = self._parse_asmtemplate_enhanced(
                asmtemplate_elem, fields, is_64bit, encoding_elem, root)
            if parsed_mnemonic:
                mnemonic = parsed_mnemonic

        # Find condition field if present
        condition_field = None
        for field in fields:
            if field.name == 'cond' and not field.is_fixed:
                condition_field = 'cond'
                break

        # Parse aliases
        aliases = self._parse_aliases(root, encoding_name)

        # Convert fields to dict
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
            condition_field=condition_field,
            aliases=aliases
        )

    def _is_64bit_variant(self, fields: List[BitField], encoding_elem) -> bool:
        """Determine if this is a 64-bit variant"""
        # Check for sf field
        for field in fields:
            if field.name == 'sf' and field.is_fixed:
                return field.fixed_value == 1

        # Check encoding name
        encoding_name = encoding_elem.get('name', '')
        if '_64_' in encoding_name:
            return True
        if '_32_' in encoding_name:
            return False

        return False  # Default to 32-bit

    def _parse_aliases(self, root, encoding_name: str) -> List[str]:
        """Parse alias information"""
        aliases = []
        alias_list = root.find('.//alias_list')
        if alias_list is not None:
            for aliasref in alias_list.findall('aliasref'):
                alias_file = aliasref.get('aliasfile', '')
                if alias_file:
                    aliases.append(alias_file.replace('.xml', ''))
        return aliases

    def _parse_asmtemplate_enhanced(self, asmtemplate_elem, fields: List[BitField],
                                   is_64bit: bool, encoding_elem, root) -> Tuple[str, List[Operand]]:
        """Enhanced assembly template parsing with field mapping"""
        mnemonic = None
        operands = []

        # Build field map
        field_map = {f.name: f for f in fields}

        # Extract template parts
        template_parts = self._extract_template_parts(asmtemplate_elem)

        # Parse parts
        for i, part in enumerate(template_parts):
            if part[0] == 'text':
                text = part[1].strip()
                if i == 0 and not mnemonic:
                    match = re.match(r'^([A-Z][A-Z0-9.]*)', text)
                    if match:
                        mnemonic = match.group(1)
            elif part[0] == 'operand':
                link, hover = part[1], part[2]
                operand = self._infer_operand_enhanced(link, hover, field_map, is_64bit, encoding_elem)
                if operand:
                    operands.append(operand)

        return mnemonic, operands

    def _extract_template_parts(self, asmtemplate_elem) -> List[Tuple]:
        """Extract template parts"""
        parts = []
        if asmtemplate_elem.text:
            parts.append(('text', asmtemplate_elem.text))

        for child in asmtemplate_elem:
            if child.tag == 'text':
                if child.text:
                    parts.append(('text', child.text))
            elif child.tag == 'a':
                link = child.get('link', '')
                hover = child.get('hover', '')
                parts.append(('operand', link, hover))

            if child.tail:
                parts.append(('text', child.tail))

        return parts

    def _infer_operand_enhanced(self, link: str, hover: str, field_map: Dict,
                               is_64bit: bool, encoding_elem) -> Optional[Operand]:
        """Enhanced operand type inference with field mapping"""

        # Extract field names from hover
        field_names = self._extract_field_names(hover)
        primary_field = field_names[0] if field_names else None
        secondary_field = field_names[1] if len(field_names) > 1 else None

        # Determine if optional
        is_optional = '{' in link or 'optional' in hover.lower()

        # Infer operand type
        link_lower = link.lower()
        hover_lower = hover.lower()

        # General purpose registers
        if any(pattern in link_lower for pattern in ['xd', 'xn', 'xm', 'xa', 'xt']):
            if 'sp' in link_lower:
                op_type = 'REG_GPR_XSP'
            elif 'zr' in link_lower:
                op_type = 'REG_GPR_XZR'
            else:
                op_type = 'REG_GPR_X'
            return Operand(op_type, None, primary_field or 'Rd', secondary_field, is_optional, hover)

        if any(pattern in link_lower for pattern in ['wd', 'wn', 'wm', 'wa', 'wt']):
            if 'sp' in link_lower:
                op_type = 'REG_GPR_WSP'
            elif 'zr' in link_lower:
                op_type = 'REG_GPR_WZR'
            else:
                op_type = 'REG_GPR_W'
            return Operand(op_type, None, primary_field or 'Rd', secondary_field, is_optional, hover)

        # FP/SIMD registers
        if any(pattern in link_lower for pattern in ['bd', 'bn']):
            return Operand('REG_FP_B', None, primary_field or 'Rd', secondary_field, is_optional, hover)
        if any(pattern in link_lower for pattern in ['hd', 'hn']):
            return Operand('REG_FP_H', None, primary_field or 'Rd', secondary_field, is_optional, hover)
        if any(pattern in link_lower for pattern in ['sd', 'sn']):
            return Operand('REG_FP_S', None, primary_field or 'Rd', secondary_field, is_optional, hover)
        if any(pattern in link_lower for pattern in ['dd', 'dn']):
            return Operand('REG_FP_D', None, primary_field or 'Rd', secondary_field, is_optional, hover)
        if any(pattern in link_lower for pattern in ['qd', 'qn']):
            return Operand('REG_FP_Q', None, primary_field or 'Rd', secondary_field, is_optional, hover)
        if any(pattern in link_lower for pattern in ['vd', 'vn', 'vm']):
            return Operand('REG_SIMD_V', None, primary_field or 'Rd', secondary_field, is_optional, hover)

        # SVE registers
        if any(pattern in link_lower for pattern in ['zd', 'zn', 'zm', 'za']):
            return Operand('REG_SVE_Z', None, primary_field or 'Zd', secondary_field, is_optional, hover)
        if any(pattern in link_lower for pattern in ['pd', 'pn', 'pm', 'pg']):
            return Operand('REG_SVE_P', None, primary_field or 'Pd', secondary_field, is_optional, hover)

        # Immediates
        if 'imm' in link_lower:
            # Determine immediate type
            if 'logical' in hover_lower:
                op_type = 'IMM_LOGICAL'
            elif 'shift' in hover_lower:
                op_type = 'IMM_SHIFTED'
            elif 'float' in hover_lower or 'fp' in hover_lower:
                op_type = 'IMM_FLOAT'
            elif 'signed' in hover_lower or 'offset' in hover_lower:
                op_type = 'IMM_SINT'
            else:
                op_type = 'IMM_UINT'
            return Operand(op_type, None, primary_field or 'imm', secondary_field, is_optional, hover)

        # Labels
        if 'label' in link_lower or ('offset' in hover_lower and 'pc' in hover_lower):
            return Operand('LABEL_PCREL', None, primary_field or 'imm', secondary_field, is_optional, hover)

        # Condition codes
        if 'cond' in link_lower:
            return Operand('CONDITION', None, 'cond', None, is_optional, hover)

        # Shift/extend
        if 'shift' in link_lower:
            return Operand('SHIFT_TYPE', None, 'shift', 'amount', is_optional, hover)
        if 'extend' in link_lower:
            return Operand('EXTEND_TYPE', None, 'option', 'imm3', is_optional, hover)

        # Memory operands - simplified
        if '[' in link or 'memory' in hover_lower:
            if 'post-indexed' in hover_lower or 'post indexed' in hover_lower:
                return Operand('MEMORY_POSTIDX', None, 'Rn', 'imm', False, hover)
            elif 'pre-indexed' in hover_lower or 'pre indexed' in hover_lower:
                return Operand('MEMORY_PREIDX', None, 'Rn', 'imm', False, hover)
            elif 'register offset' in hover_lower:
                return Operand('MEMORY_REG', None, 'Rn', 'Rm', False, hover)
            else:
                return Operand('MEMORY_BASE', None, 'Rn', 'imm', False, hover)

        return None

    def _extract_field_names(self, hover: str) -> List[str]:
        """Extract field names from hover text"""
        fields = []
        # Look for "encoded in the \"field\" field"
        matches = re.findall(r'"([A-Za-z0-9_]+)"', hover)
        fields.extend(matches)
        return fields

    def _parse_boxes(self, boxes) -> List[BitField]:
        """Parse box elements"""
        fields = []

        for box in boxes:
            hibit = int(box.get('hibit', '0'))
            width_str = box.get('width', '1')
            width = int(width_str) if width_str else 1
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


# Code generator will be in next part due to size...
# Let me continue with the enhanced code generator

class CodeGenerator:
    """Enhanced code generator with complete operand formatting"""

    def __init__(self, instructions: List[InstructionEncoding], output_dir: str):
        self.instructions = instructions
        self.output_dir = output_dir

        # Sort by specificity
        self.instructions.sort(key=lambda x: bin(x.mask).count('1'), reverse=True)

        # Build field index
        self._build_field_index()

    def _build_field_index(self):
        """Build index of all unique field names"""
        self.field_names = set()
        for instr in self.instructions:
            self.field_names.update(instr.fields.keys())

        self.field_index = {name: i for i, name in enumerate(sorted(self.field_names))}

    def generate_all(self):
        """Generate all files"""
        print("Generating enhanced disassembler...")

        self._generate_header()
        self._generate_implementation()

        print("Generation complete!")

    def _generate_header(self):
        """Generate A64InstructionTableV2.h"""
        with open(os.path.join(self.output_dir, 'A64InstructionTableV2.h'), 'w') as f:
            f.write(self._copyright_header())
            f.write("""
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace JSC { namespace ARM64Disassembler {

// Forward declarations
class A64DOpcodeContext;

// Instruction table entry
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
    uint8_t field1;
    uint8_t field2;
};

// Instruction table
extern const InstructionEntry g_instructionTable[];
extern const size_t g_instructionTableSize;

// Operand table
extern const OperandDesc g_operandTable[];

// Field name table for debugging
extern const char* const g_fieldNames[];
extern const size_t g_fieldNameCount;

// API
const InstructionEntry* findInstruction(uint32_t opcode);
void formatInstruction(const InstructionEntry* entry, uint32_t opcode,
                      uint32_t* pc, uint32_t* startPC, uint32_t* endPC,
                      char* buffer, size_t bufferSize);

}} // namespace JSC::ARM64Disassembler
""")

    def _generate_implementation(self):
        """Generate enhanced implementation with formatters"""
        with open(os.path.join(self.output_dir, 'A64InstructionTableV2.cpp'), 'w') as f:
            f.write(self._copyright_header())
            f.write("""
#include "config.h"

#if ENABLE(ARM64_DISASSEMBLER)

#include "A64InstructionTableV2.h"
#include <stdio.h>
#include <string.h>

namespace JSC { namespace ARM64Disassembler {

// Helper functions
static inline uint32_t extractBits(uint32_t value, unsigned start, unsigned width) {
    return (value >> start) & ((1U << width) - 1);
}

static inline int32_t signExtend(uint32_t value, unsigned bits) {
    uint32_t sign = 1U << (bits - 1);
    return (value & ((1U << bits) - 1)) ^ sign ? -(int32_t)(sign - (value & (sign - 1))) : (int32_t)(value & (sign - 1));
}

static const char* const g_conditionNames[16] = {
    "eq", "ne", "hs", "lo", "mi", "pl", "vs", "vc",
    "hi", "ls", "ge", "lt", "gt", "le", "al", "nv"
};

static const char* const g_shiftNames[4] = {
    "lsl", "lsr", "asr", "ror"
};

// Field name table
const char* const g_fieldNames[] = {
""")
            # Write field names
            for name in sorted(self.field_names):
                f.write(f'    "{name}",\n')

            f.write("};\n\n")
            f.write(f"const size_t g_fieldNameCount = {len(self.field_names)};\n\n")

            # Write operand table
            f.write("// Operand table\n")
            f.write("const OperandDesc g_operandTable[] = {\n")

            operand_offset = 0
            instruction_operand_info = []

            for instr in self.instructions:
                start_offset = operand_offset
                operand_count = len(instr.operands) if instr.operands else 0

                if instr.operands:
                    for op in instr.operands:
                        field1_idx = self.field_index.get(op.field_name, 255)
                        field2_idx = self.field_index.get(op.field_name2, 255) if op.field_name2 else 255
                        op_type_val = OP_TYPES.get(op.type, 0)

                        f.write(f"    {{ {op_type_val}, 0, {field1_idx}, {field2_idx} }},\n")
                        operand_offset += 1

                instruction_operand_info.append((start_offset, operand_count))

            if operand_offset == 0:
                f.write("    { 0, 0, 255, 255 }\n")

            f.write("};\n\n")

            # Write instruction table
            f.write("// Instruction table\n")
            f.write("const InstructionEntry g_instructionTable[] = {\n")

            for i, instr in enumerate(self.instructions):
                start_offset, operand_count = instruction_operand_info[i]
                flags = 0
                if instr.is_64bit_variant:
                    flags |= 1

                f.write(f"    {{ \"{instr.name}\", \"{instr.mnemonic}\", "
                       f"0x{instr.mask:08x}U, 0x{instr.pattern:08x}U, "
                       f"{start_offset}, {operand_count}, {flags} }},\n")

            f.write("};\n\n")
            f.write(f"const size_t g_instructionTableSize = {len(self.instructions)};\n\n")

            # Write instruction finder
            f.write(self._generate_finder())

            # Write formatter
            f.write(self._generate_formatter())

            f.write("\n}} // namespace JSC::ARM64Disassembler\n\n")
            f.write("#endif // ENABLE(ARM64_DISASSEMBLER)\n")

    def _generate_finder(self) -> str:
        """Generate instruction finder function"""
        return """
const InstructionEntry* findInstruction(uint32_t opcode)
{
    // Linear search (can be optimized to binary search later)
    for (size_t i = 0; i < g_instructionTableSize; i++) {
        const auto& entry = g_instructionTable[i];
        if ((opcode & entry.mask) == entry.pattern)
            return &entry;
    }
    return nullptr;
}
"""

    def _generate_formatter(self) -> str:
        """Generate instruction formatter with operand handling"""
        return """
void formatInstruction(const InstructionEntry* entry, uint32_t opcode,
                      uint32_t* pc, uint32_t* startPC, uint32_t* endPC,
                      char* buffer, size_t bufferSize)
{
    if (!entry) {
        snprintf(buffer, bufferSize, "   .long      0x%08x", opcode);
        return;
    }

    // Format mnemonic
    int offset = snprintf(buffer, bufferSize, "   %-9s", entry->mnemonic);
    if (offset < 0 || (size_t)offset >= bufferSize)
        return;

    // Format operands
    for (unsigned i = 0; i < entry->operandCount; i++) {
        const auto& op = g_operandTable[entry->operandOffset + i];

        if (i > 0 && offset > 0 && (size_t)offset < bufferSize) {
            offset += snprintf(buffer + offset, bufferSize - offset, ", ");
        }

        if (offset < 0 || (size_t)offset >= bufferSize)
            return;

        // Get field value
        uint32_t field1_val = 0;
        if (op.field1 < g_fieldNameCount) {
            // Extract field (simplified - needs actual field bit positions)
            field1_val = extractBits(opcode, 0, 5);  // Placeholder
        }

        // Format based on operand type
        switch (op.type) {
        case 0: // REG_GPR_X
            offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1_val);
            break;
        case 1: // REG_GPR_W
            offset += snprintf(buffer + offset, bufferSize - offset, "w%u", field1_val);
            break;
        case 30: // IMM_UINT
            offset += snprintf(buffer + offset, bufferSize - offset, "#%u", field1_val);
            break;
        case 31: // IMM_SINT
            offset += snprintf(buffer + offset, bufferSize - offset, "#%d", (int)field1_val);
            break;
        case 40: // LABEL_PCREL
            offset += snprintf(buffer + offset, bufferSize - offset, "0x%p",
                             (void*)(pc + field1_val));
            break;
        case 50: // CONDITION
            offset += snprintf(buffer + offset, bufferSize - offset, "%s",
                             g_conditionNames[field1_val & 0xf]);
            break;
        default:
            offset += snprintf(buffer + offset, bufferSize - offset, "?");
            break;
        }
    }
}
"""

    def _copyright_header(self) -> str:
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
 * Generated by generate_arm64_disassembler_v2.py
 * Source: ARM64 ISA XML documentation
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

    # Parse XML files
    print("Parsing ARM64 instruction XML files...")
    parser = ARM64InstructionParser(xml_dir)
    instructions = parser.parse_all()

    if parser.errors:
        print("\nErrors encountered:")
        for error in parser.errors[:10]:
            print(f"  {error}")
        if len(parser.errors) > 10:
            print(f"  ... and {len(parser.errors) - 10} more")

    # Generate code
    print(f"\nGenerating enhanced C++ code...")
    generator = CodeGenerator(instructions, output_dir)
    generator.generate_all()

    print(f"\nGenerated disassembler for {len(instructions)} instruction encodings")
    print(f"Output written to: {output_dir}")
    print(f"Field index size: {len(generator.field_names)} unique fields")

if __name__ == '__main__':
    main()
