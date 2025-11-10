#\!/bin/bash
# Unified ARM64 Disassembler Test Runner

echo "Building test suite..."
clang++ -std=c++20 -I. -o test_disassembler test_disassembler.cpp A64InstructionTable.cpp

if [ $? -ne 0 ]; then
    echo "Build failed\!"
    exit 1
fi

echo ""
./test_disassembler
exit $?
