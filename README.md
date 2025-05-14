# Cuckoo Hashing
This project implements and compares three different versions of a Cuckoo Hash Table: sequential, concurrent, and transactional. The implementation explores different synchronization approaches for concurrent hash tables and evaluates their performance characteristics.

The project includes a benchmark suite (`main.cc`) that measures:
- Operation success rates (contains, add, remove)
    - Hit/miss rates for contains operations
    - Success/failure rates for add operations
    - Success/failure rates for remove operations
- Execution time in nanoseconds for different operations
- Performance comparison between implementations
- Concurrent operation behavior with multiple threads
- Size consistency verification is necessary

## How to run

### Prerequisites
1. GCC 11.2.0 (required for transactional memory support)
2. Environment with module support (for loading GCC)
3. pthread support

### Before Running
At the top of main.cc, you'll see a comment // GLOBAL VARIABLES that affect performance
There are 5 variables here that the user can change that will affect performance:
- `numThreads` - Controls how many threads to use for parallel execution (default: 4)
- `NUM_INITIAL_KEYS` - Sets how many keys to initially insert into the hash table (default: 100000)
- `TOTAL_OPS` - Determines total number of operations for benchmarking (default: 1000000)
- `value_gen(1, 100000)` - Range for the values used in the operations (contains, add, remove)
- `val_gen_main(1, 100000)` - Range for the values used in populating the set

### Example commands of how to run
1. Make the run script executable:
```bash
chmod +x run.sh
```

2. Run the benchmark suite:
```bash
./run.sh
```

3. If you need to compile manually (I prefer the script as it takes care of everything and more):
```bash
g++ -std=c++17 -O3 -pthread -fgnu-tm main.cc -o main
./main
```

### Understanding the output

The program will output benchmark results for all three implementations (Sequential, Concurrent, and Transactional). For each implementation, you'll see:

1. Initial setup information:
   - Number of initial elements added
   - Total operations performed

2. Operation statistics:
   - Contains operations: hits, misses, and success percentage
   - Add operations: successes, failures, and success percentage
   - Remove operations: successes, failures, and success percentage

3. Size verification:
   - Expected final size
   - Actual final size
   - Size correctness check (PASS ✅ or FAIL ❌)

4. Performance metrics:
   - Total time taken in milliseconds

## Explanation of the source code
1. **Sequential Cuckoo Hash Table** (`serial-cuckoo.h`)
   - Basic cuckoo hash table implementation using two hash functions
   - Supports add, remove, and contains operations
   - Implements automatic resizing when the table becomes too full
   - Uses displacement-based insertion with a maximum displacement limit

2. **Concurrent Cuckoo Hash Table** (`concurrent-cuckoo.h`)
   - Thread-safe implementation using fine-grained synchronization
   - Supports concurrent operations from multiple threads
   - Maintains consistency during concurrent access

3. **Transactional Cuckoo Hash Table** (`transactional-cuckoo.h`)
   - Implementation using transactional memory concepts
   - Provides atomic operations for concurrent access
   - Alternative approach to traditional locking mechanisms

## Charts
View the chart:
1. [On Google Sheets](https://docs.google.com/spreadsheets/d/19pdmQfLoDorniIDlVOsryQ8h0etsa2YlvdLRHreLdAk/edit?usp=sharing)  
2. [Download File](./chart.xlsx)

The chart illustrates how the application scales with increasing levels of concurrency. The x-axis represents the number of threads used (1, 2, 4, 8, 16), while the y-axis shows the total execution time in milliseconds for completing a fixed number of operations. We also measure throughput (operations per ms).
- Serial execution (1 thread) serves as the baseline for comparison.
- Concurrent execution demonstrates how multithreading reduces overall runtime as threads increase.
- Transactional execution shows the impact of synchronization overhead on scalability.

This chart helps draw conclusions about parallel performance, thread efficiency, and bottlenecks introduced by synchronization mechanisms.

## Notes
- My transactional version is simply a wrapped version of my serial version. The transactional version wraps critical operations in __transaction_atomic blocks. These key operations (add, remove, contains) are executed atomically

## License
Copyright 2025 Samir Hassan

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
