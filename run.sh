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
OUTPUT_FILE="main-cuckoo"

# Compile the program with optimization and warnings
g++ -std=c++11 -O2 "$SRC_FILE" -o "$OUTPUT_FILE"
if [ $? -ne 0 ]; then
    echo "❌ Compilation failed!"
    exit 1
fi

echo "✅ Compilation successful. Running program..."
echo "======================"
./"$OUTPUT_FILE"
