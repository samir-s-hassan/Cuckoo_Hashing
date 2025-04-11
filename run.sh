#!/bin/bash

# Initialize the module system
source /etc/profile.d/modules.sh

# Load GCC 11.2.0
module load gcc-11.2.0

# Verify that GCC is the correct version
GCC_VERSION=$(gcc --version | head -n 1)
if [[ ! "$GCC_VERSION" =~ "11.2.0" ]]; then
    echo "Error: GCC 11.2.0 is not loaded. Current version: $GCC_VERSION"
    exit 1
fi

# Define source and output
SRC_FILE="main.cc"
OUTPUT_FILE="main"

rm -f "$OUTPUT_FILE"

# Compile the program with optimization and warnings
g++ -std=c++17 -O2 -pthread "$SRC_FILE" -o "$OUTPUT_FILE"
if [ $? -ne 0 ]; then
    echo "❌ Compilation failed!"
    exit 1
fi

./"$OUTPUT_FILE"
rm -f "$OUTPUT_FILE"

