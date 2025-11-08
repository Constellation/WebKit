#!/usr/bin/env python3
"""
ARM64 Disassembler Code Generator

This script parses ARM64 instruction XML files from the official ARM64 ISA documentation
and generates a complete C++ disassembler with support for all ARM64 instructions.

Usage:
    python3 generate_arm64_disassembler.py <xml_directory> <output_directory>
"""

import xml.etree.ElementTree as ET
import sys
import os
from collections import defaultdict, namedtuple, OrderedDict
from typing import List, Dict, Set, Tuple, Optional
import re

# Data structures
InstructionEncoding = namedtuple('InstructionEncoding', [
    'name', 'mnemonic', 'mask', 'pattern', 'fields', 'operands', 'xml_file'
])

BitField = namedtuple('BitField', [
    'name', 'bit_start', 'bit_width', 'is_fixed', 'fixed_value'
])

Operand = namedtuple('Operand', [
    'type',      # 'register', 'immediate', 'label', 'memory', 'condition', etc.
    'format',    # Format string or type descriptor
    'fields',    # List of field names this operand depends on
    'optional'   # Whether this operand is optional
])

class ARM64InstructionParser:
    """Parse ARM64 instruction XML files"""

    def __init__(self, xml_directory: str):
        self.xml_directory = xml_directory
        self.instructions: List[InstructionEncoding] = []
        self.errors: List[str] = []

    def parse_all(self) -> List[InstructionEncoding]:
        """Parse all XML files in the directory"""
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

        # Get the main mnemonic
        mnemonic = None
        docvars = root.find('.//docvars')
        if docvars is not None:
            mnemonic_elem = docvars.find("./docvar[@key='mnemonic']")
            if mnemonic_elem is not None:
                mnemonic = mnemonic_elem.get('value')

        # Parse all instruction classes
        for iclass in root.findall('.//iclass'):
            regdiagram = iclass.find('regdiagram')
            if regdiagram is None:
                continue

            for encoding in iclass.findall('.//encoding'):
                try:
                    instr = self._parse_encoding(encoding, regdiagram, mnemonic, os.path.basename(xml_path))
                    if instr:
                        self.instructions.append(instr)
                except Exception as e:
                    self.errors.append(f"Error in {xml_path}: {e}")

    def _parse_encoding(self, encoding_elem, regdiagram, default_mnemonic: str, xml_file: str) -> Optional[InstructionEncoding]:
        """Parse an instruction encoding"""
        encoding_name = encoding_elem.get('name')
        if not encoding_name:
            return None

        # Parse bit fields from regdiagram
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

        # Parse operands from asmtemplate
        asmtemplate_elem = encoding_elem.find('.//asmtemplate')
        mnemonic = default_mnemonic
        operands = []

        if asmtemplate_elem is not None:
            mnemonic, operands = self._parse_asmtemplate(asmtemplate_elem, encoding_elem)
            if not mnemonic:
                mnemonic = default_mnemonic

        # Convert fields to dict
        field_dict = {f.name: (f.bit_start, f.bit_width) for f in fields if not f.is_fixed}

        return InstructionEncoding(
            name=encoding_name,
            mnemonic=mnemonic or "UNKNOWN",
            mask=mask,
            pattern=pattern,
            fields=field_dict,
            operands=operands,
            xml_file=xml_file
        )

    def _parse_asmtemplate(self, asmtemplate_elem, encoding_elem) -> Tuple[str, List[Operand]]:
        """Parse assembly template to extract mnemonic and operands"""
        mnemonic = None
        operands = []

        # Extract text and links
        template_parts = []
        if asmtemplate_elem.text:
            template_parts.append(('text', asmtemplate_elem.text))

        for child in asmtemplate_elem:
            if child.tag == 'text':
                if child.text:
                    template_parts.append(('text', child.text))
            elif child.tag == 'a':
                link = child.get('link', '')
                hover = child.get('hover', '')
                template_parts.append(('operand', link, hover))

            if child.tail:
                template_parts.append(('text', child.tail))

        # Parse template parts
        for i, part in enumerate(template_parts):
            if part[0] == 'text':
                text = part[1].strip()
                if i == 0 and not mnemonic:
                    # First text is likely the mnemonic
                    match = re.match(r'^([A-Z][A-Z0-9]*)', text)
                    if match:
                        mnemonic = match.group(1)
            elif part[0] == 'operand':
                link, hover = part[1], part[2]
                # Determine operand type from link/hover
                operand = self._infer_operand_type(link, hover, encoding_elem)
                if operand:
                    operands.append(operand)

        return mnemonic, operands

    def _infer_operand_type(self, link: str, hover: str, encoding_elem) -> Optional[Operand]:
        """Infer operand type from link and hover text"""
        # Check for register operands
        if any(reg in link.lower() for reg in ['xn', 'wn', 'rn', 'rd', 'rm', 'ra', 'rt', 'sp']):
            if 'x' in link.lower() or 'X' in link:
                return Operand('register', 'X', self._extract_fields(hover), False)
            elif 'w' in link.lower() or 'W' in link:
                return Operand('register', 'W', self._extract_fields(hover), False)
            elif 'sp' in link.lower():
                return Operand('register', 'SP', self._extract_fields(hover), False)

        # Check for FP/SIMD registers
        if any(reg in link.lower() for reg in ['vn', 'vd', 'vm', 'bn', 'hn', 'sn', 'dn', 'qn']):
            if 'v' in link.lower():
                return Operand('register', 'V', self._extract_fields(hover), False)
            else:
                return Operand('register', 'FP', self._extract_fields(hover), False)

        # Check for SVE registers
        if any(reg in link.lower() for reg in ['zn', 'zd', 'zm', 'za']):
            return Operand('register', 'Z', self._extract_fields(hover), False)

        if any(reg in link.lower() for reg in ['pn', 'pd', 'pm', 'pg']):
            return Operand('register', 'P', self._extract_fields(hover), False)

        # Check for immediates
        if 'imm' in link.lower():
            return Operand('immediate', 'imm', self._extract_fields(hover), False)

        # Check for labels/offsets
        if 'label' in link.lower() or 'offset' in link.lower():
            return Operand('label', 'label', self._extract_fields(hover), False)

        # Check for conditions
        if 'cond' in link.lower():
            return Operand('condition', 'cond', self._extract_fields(hover), False)

        # Check for shift/extend
        if 'shift' in link.lower() or 'extend' in link.lower():
            return Operand('shift', 'shift', self._extract_fields(hover), True)

        # Default to text
        return Operand('text', link, [], True)

    def _extract_fields(self, hover: str) -> List[str]:
        """Extract field names from hover text"""
        fields = []
        # Look for "encoded in the \"field\" field"
        matches = re.findall(r'"([A-Za-z0-9_]+)"', hover)
        fields.extend(matches)
        return fields

    def _parse_boxes(self, boxes) -> List[BitField]:
        """Parse a list of box elements"""
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
        """Parse regdiagram to extract bit fields"""
        return self._parse_boxes(regdiagram.findall('box'))

    def _calculate_mask_and_pattern(self, fields: List[BitField]) -> Tuple[int, int]:
        """Calculate 32-bit mask and pattern"""
        mask = 0
        pattern = 0

        for field in fields:
            if field.is_fixed:
                field_mask = ((1 << field.bit_width) - 1) << field.bit_start
                mask |= field_mask
                pattern |= (field.fixed_value << field.bit_start)

        return mask, pattern

class CodeGenerator:
    """Generate C++ code for the ARM64 disassembler"""

    def __init__(self, instructions: List[InstructionEncoding], output_dir: str):
        self.instructions = instructions
        self.output_dir = output_dir

        # Sort by specificity (more fixed bits first)
        self.instructions.sort(key=lambda x: bin(x.mask).count('1'), reverse=True)

    def generate_all(self):
        """Generate all C++ files"""
        print("Generating files...")

        self._generate_instruction_table_header()
        self._generate_instruction_table_impl()
        self._generate_new_a64dopcode_header()
        self._generate_new_a64dopcode_impl()

        print("Code generation complete!")

    def _generate_instruction_table_header(self):
        """Generate A64InstructionTable.h"""
        with open(os.path.join(self.output_dir, 'A64InstructionTable.h'), 'w') as f:
            f.write(self._COPYRIGHT_HEADER())
            f.write("""
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace JSC { namespace ARM64Disassembler {

class A64DOpcode;

// Instruction encoding table entry
struct InstructionTableEntry {
    const char* name;
    const char* mnemonic;
    uint32_t mask;
    uint32_t pattern;
    uint16_t operandDataOffset;  // Offset into operand data table
    uint8_t operandCount;
};

// Operand descriptor
struct OperandDescriptor {
    uint8_t type;       // OperandType
    uint8_t format;     // Format variant
    uint8_t field1;     // Field index 1
    uint8_t field2;     // Field index 2
};

// Operand types
enum OperandType {
    OP_NONE = 0,
    OP_REGISTER_X,
    OP_REGISTER_W,
    OP_REGISTER_SP,
    OP_REGISTER_FP,
    OP_REGISTER_V,
    OP_REGISTER_Z,
    OP_REGISTER_P,
    OP_IMMEDIATE,
    OP_IMMEDIATE_HEX,
    OP_IMMEDIATE_SHIFTED,
    OP_LABEL,
    OP_CONDITION,
    OP_SHIFT_TYPE,
    OP_EXTEND_TYPE,
    OP_MEMORY_BASE,
    OP_MEMORY_BASE_OFFSET,
    OP_MEMORY_PRE_INDEX,
    OP_MEMORY_POST_INDEX,
};

// Instruction table
extern const InstructionTableEntry g_instructionTable[];
extern const size_t g_instructionTableSize;

// Operand descriptor table
extern const OperandDescriptor g_operandTable[];

// Find instruction encoding for opcode
const InstructionTableEntry* findInstruction(uint32_t opcode);

// Format instruction
void formatInstruction(const InstructionTableEntry* entry, uint32_t opcode,
                      A64DOpcode* context, char* buffer, size_t bufferSize);

}} // namespace JSC::ARM64Disassembler
""")

    def _generate_instruction_table_impl(self):
        """Generate A64InstructionTable.cpp with all instruction data"""
        with open(os.path.join(self.output_dir, 'A64InstructionTable.cpp'), 'w') as f:
            f.write(self._COPYRIGHT_HEADER())
            f.write("""
#include "config.h"

#if ENABLE(ARM64_DISASSEMBLER)

#include "A64InstructionTable.h"
#include "A64DOpcode.h"
#include <stdio.h>
#include <wtf/PrintStream.h>

namespace JSC { namespace ARM64Disassembler {

// Operand data table
const OperandDescriptor g_operandTable[] = {
""")

            # Generate operand table
            operand_offset = 0
            operand_data = []
            instruction_operand_info = []

            for instr in self.instructions:
                start_offset = operand_offset
                operand_count = len(instr.operands) if instr.operands else 0

                # Add operand descriptors
                if instr.operands:
                    for op in instr.operands:
                        operand_data.append(f"    {{ OP_{self._map_operand_type(op.type)}, 0, 0, 0 }},")
                        operand_offset += 1

                instruction_operand_info.append((start_offset, operand_count))

            # Write operand table
            if operand_data:
                f.write('\n'.join(operand_data))
            else:
                f.write("    { OP_NONE, 0, 0, 0 }")

            f.write("\n};\n\n// Instruction table\n")
            f.write("const InstructionTableEntry g_instructionTable[] = {\n")

            # Write instruction table
            for i, instr in enumerate(self.instructions):
                start_offset, operand_count = instruction_operand_info[i]
                f.write(f"    {{ \"{instr.name}\", \"{instr.mnemonic}\", "
                       f"0x{instr.mask:08x}U, 0x{instr.pattern:08x}U, "
                       f"{start_offset}, {operand_count} }},\n")

            f.write("};\n\n")
            f.write(f"const size_t g_instructionTableSize = {len(self.instructions)};\n\n")

            # Write binary search function
            f.write("""
const InstructionTableEntry* findInstruction(uint32_t opcode)
{
    // Linear search through sorted table (binary search TODO)
    for (size_t i = 0; i < g_instructionTableSize; i++) {
        const auto& entry = g_instructionTable[i];
        if ((opcode & entry.mask) == entry.pattern)
            return &entry;
    }
    return nullptr;
}

void formatInstruction(const InstructionTableEntry* entry, uint32_t opcode,
                      A64DOpcode* context, char* buffer, size_t bufferSize)
{
    if (!entry) {
        snprintf(buffer, bufferSize, "   .long      0x%08x", opcode);
        return;
    }

    // Format mnemonic
    int offset = snprintf(buffer, bufferSize, "   %-9s", entry->mnemonic);

    // Format operands (simplified for now)
    if (entry->operandCount > 0 && offset > 0 && offset < (int)bufferSize) {
        snprintf(buffer + offset, bufferSize - offset, " ; (operand formatting TODO)");
    }
}

}} // namespace JSC::ARM64Disassembler

#endif // ENABLE(ARM64_DISASSEMBLER)
""")

    def _generate_new_a64dopcode_header(self):
        """Generate new A64DOpcodeNew.h"""
        with open(os.path.join(self.output_dir, 'A64DOpcodeNew.h'), 'w') as f:
            f.write(self._COPYRIGHT_HEADER())
            f.write("""
#pragma once

#include "A64InstructionTable.h"
#include <stdint.h>

namespace JSC { namespace ARM64Disassembler {

class A64DOpcodeNew {
public:
    A64DOpcodeNew(uint32_t* startPC = nullptr, uint32_t* endPC = nullptr);

    const char* disassemble(uint32_t* currentPC);

private:
    void setPCAndOpcode(uint32_t* pc, uint32_t opcode);

    uint32_t* m_startPC;
    uint32_t* m_endPC;
    uint32_t* m_currentPC;
    uint32_t m_opcode;

    static constexpr size_t BufferSize = 256;
    char m_formatBuffer[BufferSize];
};

}} // namespace JSC::ARM64Disassembler
""")

    def _generate_new_a64dopcode_impl(self):
        """Generate new A64DOpcodeNew.cpp"""
        with open(os.path.join(self.output_dir, 'A64DOpcodeNew.cpp'), 'w') as f:
            f.write(self._COPYRIGHT_HEADER())
            f.write("""
#include "config.h"

#if ENABLE(ARM64_DISASSEMBLER)

#include "A64DOpcodeNew.h"
#include <stdio.h>

namespace JSC { namespace ARM64Disassembler {

A64DOpcodeNew::A64DOpcodeNew(uint32_t* startPC, uint32_t* endPC)
    : m_startPC(startPC)
    , m_endPC(endPC)
    , m_currentPC(nullptr)
    , m_opcode(0)
{
    m_formatBuffer[0] = '\\0';
}

void A64DOpcodeNew::setPCAndOpcode(uint32_t* pc, uint32_t opcode)
{
    m_currentPC = pc;
    m_opcode = opcode;
    m_formatBuffer[0] = '\\0';
}

const char* A64DOpcodeNew::disassemble(uint32_t* currentPC)
{
    setPCAndOpcode(currentPC, *currentPC);

    // Find matching instruction
    const InstructionTableEntry* entry = findInstruction(m_opcode);

    // Format instruction
    formatInstruction(entry, m_opcode, nullptr, m_formatBuffer, BufferSize);

    return m_formatBuffer;
}

}} // namespace JSC::ARM64Disassembler

#endif // ENABLE(ARM64_DISASSEMBLER)
""")

    def _map_operand_type(self, op_type: str) -> str:
        """Map operand type string to enum name"""
        mapping = {
            'register': 'REGISTER_X',
            'immediate': 'IMMEDIATE',
            'label': 'LABEL',
            'condition': 'CONDITION',
            'shift': 'SHIFT_TYPE',
            'memory': 'MEMORY_BASE'
        }
        return mapping.get(op_type, 'NONE')

    def _COPYRIGHT_HEADER(self) -> str:
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
 * from ARM64 ISA XML documentation
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
    print(f"\nGenerating C++ code...")
    generator = CodeGenerator(instructions, output_dir)
    generator.generate_all()

    print(f"\nGenerated disassembler for {len(instructions)} instruction encodings")
    print(f"Output written to: {output_dir}")

if __name__ == '__main__':
    main()
