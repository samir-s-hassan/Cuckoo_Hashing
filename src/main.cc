#include <iostream>      // For std::cout and std::cerr
#include <vector>        // For std::vector
#include <random>        // For random number generation
#include <chrono>        // For measuring execution time
#include <unordered_set> // For generating unique initial keys
#include <thread>        // For creating threads
#include <atomic>        // For atomic operations
#include <iomanip>       // For std::setw (for output formatting)

#include "header/serial-cuckoo.h"        // Include your sequential cuckoo header
#include "header/concurrent-cuckoo.h"    // Include your concurrent cuckoo header
#include "header/transactional-cuckoo.h" // Include your transactional cuckoo header

// GLOBAL VARIABLES that affect performance
const int numThreads = 4;                                   // Number of threads to be used for concurrent benchmarks
const int NUM_INITIAL_KEYS = 100000;                        // Number of initial keys to insert into the set
const int TOTAL_OPS = 1000000;                              // Total number of operations to perform during benchmarking
std::uniform_int_distribution<int> value_gen(1, 100000);    // Random generator for values used in operations (contains, add, remove)
std::uniform_int_distribution<int> val_gen_main(1, 100000); // Random generator for values used in populating the set

// Struct to track statistics from the benchmark
struct Stats
{
    std::atomic<int> hits_contains{0};      // Number of successful "contains" operations
    std::atomic<int> misses_contains{0};    // Number of unsuccessful "contains" operations
    std::atomic<int> successful_adds{0};    // Number of successful "add" operations
    std::atomic<int> failed_adds{0};        // Number of failed "add" operations
    std::atomic<int> successful_removes{0}; // Number of successful "remove" operations
    std::atomic<int> failed_removes{0};     // Number of failed "remove" operations
    long long time_ns = 0;                  // Time taken for the benchmark in nanoseconds
};

// Run benchmark workload on the serial cuckoo set
void run_serial_benchmark(CuckooSequentialSet<int> &set, int totalOps, Stats &stats)
{
    std::uniform_real_distribution<double> op_dist(0.0, 1.0); // For randomly selecting between contains, add, and remove
    std::mt19937 rng(std::random_device{}());                 // Random number generator

    auto start = std::chrono::high_resolution_clock::now(); // Start the timer to measure execution time

    for (int i = 0; i < totalOps; ++i)
    {
        double choice = op_dist(rng); // Randomly choose an operation (contains, add, or remove)
        int value = value_gen(rng);   // Generate a random value to operate on

        if (choice < 0.8) // 80% chance for a "contains" operation
        {
            if (set.contains(value))   // If the value is present in the set
                stats.hits_contains++; // Increment the hits counter
            else
                stats.misses_contains++; // Increment the misses counter
        }
        else if (choice < 0.9) // 10% chance for an "add" operation
        {
            if (set.add(value))          // If the value is successfully added
                stats.successful_adds++; // Increment the successful adds counter
            else
                stats.failed_adds++; // Increment the failed adds counter
        }
        else // 10% chance for a "remove" operation
        {
            if (set.remove(value))          // If the value is successfully removed
                stats.successful_removes++; // Increment the successful removes counter
            else
                stats.failed_removes++; // Increment the failed removes counter
        }
    }

    auto end = std::chrono::high_resolution_clock::now();                                      // End the timer
    stats.time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(); // Calculate time taken in nanoseconds
}

// Run benchmark workload on the concurrent cuckoo set using threads
void run_concurrent_benchmark(CuckooConcurrentSet<int> &set, int totalOps, Stats &stats)
{
    std::uniform_real_distribution<double> op_dist(0.0, 1.0); // For randomly selecting between contains, add, and remove

    auto start = std::chrono::high_resolution_clock::now(); // Start the timer to measure execution time

    std::vector<std::thread> threads; // Vector to hold threads for parallel execution

    // Create threads and execute operations concurrently
    for (int t = 0; t < numThreads; ++t)
    {
        threads.push_back(std::thread([&set, totalOps, &stats, &op_dist]
                                      {
            std::mt19937 local_rng(std::random_device{}()); // Local RNG for each thread
            for (int i = 0; i < totalOps / numThreads; ++i) {
                double choice = op_dist(local_rng); // Randomly choose an operation (contains, add, or remove)
                int value = value_gen(local_rng);   // Generate a random value to operate on

                if (choice < 0.8) { // 80% chance for a "contains" operation
                    if (set.contains(value)) stats.hits_contains++;
                    else stats.misses_contains++;
                } else if (choice < 0.9) { // 10% chance for an "add" operation
                    if (set.add(value)) stats.successful_adds++;
                    else stats.failed_adds++;
                } else { // 10% chance for a "remove" operation
                    if (set.remove(value)) stats.successful_removes++;
                    else stats.failed_removes++;
                }
            } }));
    }

    // Wait for all threads to finish
    for (auto &th : threads)
        th.join();

    auto end = std::chrono::high_resolution_clock::now();                                      // End the timer
    stats.time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(); // Calculate time taken in nanoseconds
}

// Run benchmark workload on the transactional cuckoo set using threads
void run_transactional_benchmark(CuckooTransactionalSet<int> &set, int totalOps, Stats &stats)
{
    std::uniform_real_distribution<double> op_dist(0.0, 1.0); // For randomly selecting between contains, add, and remove

    auto start = std::chrono::high_resolution_clock::now(); // Start the timer to measure execution time

    std::vector<std::thread> threads; // Vector to hold threads for parallel execution

    // Create threads and execute operations concurrently
    for (int t = 0; t < numThreads; ++t)
    {
        threads.push_back(std::thread([&set, totalOps, &stats, &op_dist]
                                      {
            std::mt19937 local_rng(std::random_device{}()); // Local RNG for each thread
            for (int i = 0; i < totalOps / numThreads; ++i) {
                double choice = op_dist(local_rng); // Randomly choose an operation (contains, add, or remove)
                int value = value_gen(local_rng);   // Generate a random value to operate on

                if (choice < 0.8) { // 80% chance for a "contains" operation
                    if (set.contains(value)) stats.hits_contains++;
                    else stats.misses_contains++;
                } else if (choice < 0.9) { // 10% chance for an "add" operation
                    if (set.add(value)) stats.successful_adds++;
                    else stats.failed_adds++;
                } else { // 10% chance for a "remove" operation
                    if (set.remove(value)) stats.successful_removes++;
                    else stats.failed_removes++;
                }
            } }));
    }

    // Wait for all threads to finish
    for (auto &th : threads)
        th.join();

    auto end = std::chrono::high_resolution_clock::now();                                      // End the timer
    stats.time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(); // Calculate time taken in nanoseconds
}

int main()
{
    std::vector<int> initialKeys;
    std::unordered_set<int> keyCheck;
    std::mt19937 rng(std::random_device{}());

    // Generate unique initial keys
    while (initialKeys.size() < NUM_INITIAL_KEYS)
    {
        int key = val_gen_main(rng);
        if (keyCheck.insert(key).second)
        {
            initialKeys.push_back(key);
        }
    }

    // Initialize and populate the set
    CuckooSequentialSet<int> cuckooSet(2 * NUM_INITIAL_KEYS);
    int initially_added_serial = cuckooSet.populate(initialKeys); // Track how many were added

    // Run benchmark for the serial version
    Stats stats_serial;
    run_serial_benchmark(cuckooSet, TOTAL_OPS, stats_serial);

    // Adjust expected size based on successful adds/removes only
    int expectedSize_serial = initially_added_serial + stats_serial.successful_adds - stats_serial.successful_removes;
    int actualSize_serial = cuckooSet.size();

    // Calculate percentage rates for contains, add, and remove
    double contains_percentage = (stats_serial.hits_contains + stats_serial.misses_contains) > 0
                                     ? (double)stats_serial.hits_contains / (stats_serial.hits_contains + stats_serial.misses_contains) * 100
                                     : 0;
    double add_percentage = (stats_serial.successful_adds + stats_serial.failed_adds) > 0
                                ? (double)stats_serial.successful_adds / (stats_serial.successful_adds + stats_serial.failed_adds) * 100
                                : 0;
    double remove_percentage = (stats_serial.successful_removes + stats_serial.failed_removes) > 0
                                   ? (double)stats_serial.successful_removes / (stats_serial.successful_removes + stats_serial.failed_removes) * 100
                                   : 0;

    // Output serial benchmark summary with percentage rates
    std::cout << "=== Cuckoo Sequential Set Benchmark ===\n";
    std::cout << std::setw(30) << std::left << "Initial elements added:" << std::setw(10) << initially_added_serial << "\n";
    std::cout << std::setw(30) << std::left << "Operations performed:" << std::setw(10) << TOTAL_OPS << "\n";
    std::cout << std::setw(30) << std::left << "Contains → Hits:" << std::setw(10) << stats_serial.hits_contains
              << std::setw(10) << "Misses:" << std::setw(10) << stats_serial.misses_contains
              << std::setw(10) << "Percentage: " << std::fixed << std::setprecision(2) << contains_percentage << "%\n";
    std::cout << std::setw(30) << std::left << "Add      → Successes:" << std::setw(10) << stats_serial.successful_adds
              << std::setw(10) << "Failures:" << std::setw(10) << stats_serial.failed_adds
              << std::setw(10) << "Percentage: " << std::fixed << std::setprecision(2) << add_percentage << "%\n";
    std::cout << std::setw(30) << std::left << "Remove   → Successes:" << std::setw(10) << stats_serial.successful_removes
              << std::setw(10) << "Failures:" << std::setw(10) << stats_serial.failed_removes
              << std::setw(10) << "Percentage: " << std::fixed << std::setprecision(2) << remove_percentage << "%\n";
    std::cout << std::setw(30) << std::left << "Expected final size:" << std::setw(10) << expectedSize_serial << "\n";
    std::cout << std::setw(30) << std::left << "Actual final size:" << std::setw(10) << actualSize_serial << "\n";
    std::cout << std::setw(30) << std::left << "Size correctness:" << (expectedSize_serial == actualSize_serial ? "PASS ✅" : "FAIL ❌") << "\n";
    std::cout << std::setw(30) << std::left << "Time taken:" << std::setw(10) << (stats_serial.time_ns / 1000000) << " milliseconds (ms)\n\n"; // milliseconds

    // Initialize and populate the concurrent set
    CuckooConcurrentSet<int> cuckooConcurrentSet(2 * NUM_INITIAL_KEYS);
    int initially_added_concurrent = cuckooConcurrentSet.populate(initialKeys); // Track the number of elements added

    // Run benchmark for the concurrent version
    Stats stats_concurrent;
    run_concurrent_benchmark(cuckooConcurrentSet, TOTAL_OPS, stats_concurrent);

    // Compute expected size based on successful operations
    int expectedSize_concurrent = initially_added_concurrent + stats_concurrent.successful_adds - stats_concurrent.successful_removes;
    int actualSize_concurrent = cuckooConcurrentSet.size();

    // Calculate percentage rates for concurrent
    contains_percentage = (stats_concurrent.hits_contains + stats_concurrent.misses_contains) > 0
                              ? (double)stats_concurrent.hits_contains / (stats_concurrent.hits_contains + stats_concurrent.misses_contains) * 100
                              : 0;
    add_percentage = (stats_concurrent.successful_adds + stats_concurrent.failed_adds) > 0
                         ? (double)stats_concurrent.successful_adds / (stats_concurrent.successful_adds + stats_concurrent.failed_adds) * 100
                         : 0;
    remove_percentage = (stats_concurrent.successful_removes + stats_concurrent.failed_removes) > 0
                            ? (double)stats_concurrent.successful_removes / (stats_concurrent.successful_removes + stats_concurrent.failed_removes) * 100
                            : 0;

    // Output concurrent benchmark summary with percentage rates
    std::cout << "=== Cuckoo Concurrent Set Benchmark ===\n";
    std::cout << std::setw(30) << std::left << "Initial elements added:" << std::setw(10) << initially_added_concurrent << "\n";
    std::cout << std::setw(30) << std::left << "Operations performed:" << std::setw(10) << TOTAL_OPS << "\n";
    std::cout << std::setw(30) << std::left << "Contains → Hits:" << std::setw(10) << stats_concurrent.hits_contains
              << std::setw(10) << "Misses:" << std::setw(10) << stats_concurrent.misses_contains
              << std::setw(10) << "Percentage: " << std::fixed << std::setprecision(2) << contains_percentage << "%\n";
    std::cout << std::setw(30) << std::left << "Add      → Successes:" << std::setw(10) << stats_concurrent.successful_adds
              << std::setw(10) << "Failures:" << std::setw(10) << stats_concurrent.failed_adds
              << std::setw(10) << "Percentage: " << std::fixed << std::setprecision(2) << add_percentage << "%\n";
    std::cout << std::setw(30) << std::left << "Remove   → Successes:" << std::setw(10) << stats_concurrent.successful_removes
              << std::setw(10) << "Failures:" << std::setw(10) << stats_concurrent.failed_removes
              << std::setw(10) << "Percentage: " << std::fixed << std::setprecision(2) << remove_percentage << "%\n";
    std::cout << std::setw(30) << std::left << "Expected final size:" << std::setw(10) << expectedSize_concurrent << "\n";
    std::cout << std::setw(30) << std::left << "Actual final size:" << std::setw(10) << actualSize_concurrent << "\n";
    std::cout << std::setw(30) << std::left << "Size correctness:" << (expectedSize_concurrent == actualSize_concurrent ? "PASS ✅" : "FAIL ❌") << "\n";
    std::cout << std::setw(30) << std::left << "Time taken:" << std::setw(10) << (stats_concurrent.time_ns / 1000000) << " milliseconds (ms)\n\n"; // milliseconds

    // Initialize and populate the transactional set
    CuckooTransactionalSet<int> cuckooTransactionalSet(2 * NUM_INITIAL_KEYS);
    int initially_added_transactional = cuckooTransactionalSet.populate(initialKeys); // Track the number of elements added

    // Run benchmark for the transactional version
    Stats stats_transactional;
    run_transactional_benchmark(cuckooTransactionalSet, TOTAL_OPS, stats_transactional);

    // Compute expected size based on successful operations
    int expectedSize_transactional = initially_added_transactional + stats_transactional.successful_adds - stats_transactional.successful_removes;
    int actualSize_transactional = cuckooTransactionalSet.size();

    // Calculate percentage rates for transactional
    contains_percentage = (stats_transactional.hits_contains + stats_transactional.misses_contains) > 0
                              ? (double)stats_transactional.hits_contains / (stats_transactional.hits_contains + stats_transactional.misses_contains) * 100
                              : 0;
    add_percentage = (stats_transactional.successful_adds + stats_transactional.failed_adds) > 0
                         ? (double)stats_transactional.successful_adds / (stats_transactional.successful_adds + stats_transactional.failed_adds) * 100
                         : 0;
    remove_percentage = (stats_transactional.successful_removes + stats_transactional.failed_removes) > 0
                            ? (double)stats_transactional.successful_removes / (stats_transactional.successful_removes + stats_transactional.failed_removes) * 100
                            : 0;

    // Output transactional benchmark summary with percentage rates
    std::cout << "=== Cuckoo Transactional Set Benchmark ===\n";
    std::cout << std::setw(30) << std::left << "Initial elements added:" << std::setw(10) << initially_added_transactional << "\n";
    std::cout << std::setw(30) << std::left << "Operations performed:" << std::setw(10) << TOTAL_OPS << "\n";
    std::cout << std::setw(30) << std::left << "Contains → Hits:" << std::setw(10) << stats_transactional.hits_contains
              << std::setw(10) << "Misses:" << std::setw(10) << stats_transactional.misses_contains
              << std::setw(10) << "Percentage: " << std::fixed << std::setprecision(2) << contains_percentage << "%\n";
    std::cout << std::setw(30) << std::left << "Add      → Successes:" << std::setw(10) << stats_transactional.successful_adds
              << std::setw(10) << "Failures:" << std::setw(10) << stats_transactional.failed_adds
              << std::setw(10) << "Percentage: " << std::fixed << std::setprecision(2) << add_percentage << "%\n";
    std::cout << std::setw(30) << std::left << "Remove   → Successes:" << std::setw(10) << stats_transactional.successful_removes
              << std::setw(10) << "Failures:" << std::setw(10) << stats_transactional.failed_removes
              << std::setw(10) << "Percentage: " << std::fixed << std::setprecision(2) << remove_percentage << "%\n";
    std::cout << std::setw(30) << std::left << "Expected final size:" << std::setw(10) << expectedSize_transactional << "\n";
    std::cout << std::setw(30) << std::left << "Actual final size:" << std::setw(10) << actualSize_transactional << "\n";
    std::cout << std::setw(30) << std::left << "Size correctness:" << (expectedSize_transactional == actualSize_transactional ? "PASS ✅" : "FAIL ❌") << "\n";
    std::cout << std::setw(30) << std::left << "Time taken:" << std::setw(10) << (stats_transactional.time_ns / 1000000) << " milliseconds (ms)\n\n"; // milliseconds

    return 0;
}
