#include <iostream>      // For std::cout and std::cerr
#include <vector>        // For std::vector
#include <random>        // For random number generation
#include <chrono>        // For measuring execution time
#include <algorithm>     // For std::find and std::swap
#include <unordered_set> // For generating unique initial keys

#include "new-serial-cuckoo.h" // Include your sequential cuckoo header

// Struct to track statistics from the benchmark
struct Stats {
    int hits_contains = 0;
    int misses_contains = 0;
    int successful_inserts = 0;
    int failed_inserts = 0;
    int successful_removes = 0;
    int failed_removes = 0;
    long long time_ns = 0;
};

// Run benchmark workload on the sequential cuckoo set
void run_serial_benchmark(CuckooSequentialSet<int>& set, std::vector<int>& liveKeys, int totalOps, Stats& stats) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> value_gen(1, 1000000); // For generating new keys
    std::uniform_real_distribution<double> op_dist(0.0, 1.0); // For choosing operation type

    auto start = std::chrono::high_resolution_clock::now(); // Start timer

    for (int i = 0; i < totalOps; ++i) {
        double choice = op_dist(rng);
        int value;

        // Ensure removal/contains don't access empty liveKeys
        if (liveKeys.empty()) choice = 1.0;

        std::uniform_int_distribution<int> key_pick(0, liveKeys.size() - 1);

        if (choice < 0.8) {
            // 80% contains
            value = liveKeys[key_pick(rng)];
            if (set.contains(value)) stats.hits_contains++;
            else stats.misses_contains++;
        } else if (choice < 0.9) {
            // 10% insert
            value = value_gen(rng);
            if (set.insert(value)) {
                stats.successful_inserts++;
                liveKeys.push_back(value); // Track key for possible remove
            } else {
                stats.failed_inserts++;
            }
        } else {
            // 10% remove
            value = liveKeys[key_pick(rng)];
            if (set.remove(value)) {
                stats.successful_removes++;
                auto it = std::find(liveKeys.begin(), liveKeys.end(), value);
                if (it != liveKeys.end()) {
                    std::swap(*it, liveKeys.back());
                    liveKeys.pop_back();
                }
            } else {
                stats.failed_removes++;
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now(); // End timer
    stats.time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

int main() {
    const int NUM_INITIAL_KEYS = 1000;
    const int TOTAL_OPS = 10000;

    std::vector<int> initialKeys;
    std::unordered_set<int> keyCheck;
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> val_gen(1, 1000000);

    // Generate unique initial keys
    while (initialKeys.size() < NUM_INITIAL_KEYS) {
        int key = val_gen(rng);
        if (keyCheck.insert(key).second) {
            initialKeys.push_back(key);
        }
    }

    // Initialize and populate the set
    CuckooSequentialSet<int> cuckooSet;
    int initialSize = cuckooSet.populate(initialKeys);

    std::cout << "Initial population complete. Inserted: " << initialSize << "\n";

    // Run benchmark
    Stats stats;
    std::vector<int> liveKeys = initialKeys; // Used to track valid keys during benchmark
    run_serial_benchmark(cuckooSet, liveKeys, TOTAL_OPS, stats);

    int expectedSize = initialSize + stats.successful_inserts - stats.successful_removes;
    int actualSize = cuckooSet.size();

    // Output benchmark summary
    std::cout << "\n=== Cuckoo Sequential Set Benchmark ===\n";
    std::cout << "Operations performed: " << TOTAL_OPS << "\n";
    std::cout << "Contains → Hits: " << stats.hits_contains << ", Misses: " << stats.misses_contains << "\n";
    std::cout << "Insert   → Successes: " << stats.successful_inserts << ", Failures: " << stats.failed_inserts << "\n";
    std::cout << "Remove   → Successes: " << stats.successful_removes << ", Failures: " << stats.failed_removes << "\n";
    std::cout << "Expected final size: " << expectedSize << "\n";
    std::cout << "Actual final size:   " << actualSize << "\n";
    std::cout << "Size correctness: " << (expectedSize == actualSize ? "PASS ✅" : "FAIL ❌") << "\n";
    std::cout << "Time taken: " << (stats.time_ns / 1000) << " microseconds (µs)\n";

    return 0;
}
