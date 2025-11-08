#\!/bin/bash
# Extract just the implementation parts from A64InstructionTable.cpp without config.h

# Create a minimal standalone version
cat > A64InstructionTable_standalone.cpp << 'ENDCPP'
// Standalone version without WebKit dependencies
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Field metadata
struct FieldMeta {
    const char* name;
    uint8_t bitStart;
    uint8_t bitWidth;
};

struct OperandDesc {
    uint8_t type;
    uint8_t subtype;
    uint8_t field1;
    uint8_t field2;
};

struct InstructionEntry {
    const char* name;
    const char* mnemonic;
    uint32_t mask;
    uint32_t pattern;
    uint16_t operandOffset;
    uint8_t operandCount;
    uint8_t flags;
};

ENDCPP

# Extract the actual table data and functions from A64InstructionTable.cpp
# Skip the includes and namespace wrapping
sed -n '/^\/\/ Helper functions/,/^#endif/p' A64InstructionTable.cpp | \
  grep -v "^#endif" | \
  grep -v "namespace JSC" | \
  grep -v "^}}" >> A64InstructionTable_standalone.cpp

echo "Created A64InstructionTable_standalone.cpp"

# Try to compile
clang++ -std=c++17 -O2 \
  test_stp_minimal.cpp \
  A64InstructionTable_standalone.cpp \
  -o test_stp_minimal 2>&1 | head -30

if [ -f test_stp_minimal ]; then
  echo ""
  echo "✅ Compilation successful\!"
  echo "Running test..."
  echo ""
  ./test_stp_minimal
else
  echo "❌ Compilation failed"
fi
