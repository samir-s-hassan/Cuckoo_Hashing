#include <iostream>      // For std::cout and std::cerr
#include <vector>        // For std::vector
#include <random>        // For random number generation
#include <chrono>        // For measuring execution time
#include <algorithm>     // For std::find and std::swap
#include <unordered_set> // For generating unique initial keys
#include <thread>        // For creating threads
#include <atomic>        // For atomic operations

#include "src/serial-cuckoo.h"        // Include your sequential cuckoo header
#include "src/concurrent-cuckoo.h"    // Include your concurrent cuckoo header
#include "src/transactional-cuckoo.h" // Include your transactional cuckoo header

// Struct to track statistics from the benchmark
struct Stats
{
    std::atomic<int> hits_contains{0};
    std::atomic<int> misses_contains{0};
    std::atomic<int> successful_adds{0};
    std::atomic<int> failed_adds{0};
    std::atomic<int> successful_removes{0};
    std::atomic<int> failed_removes{0};
    long long time_ns = 0;
};

// Run benchmark workload on the serial cuckoo set
void run_serial_benchmark(CuckooSequentialSet<int> &set, std::vector<int> &liveKeys, int totalOps, Stats &stats)
{
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> value_gen(1, 1000000); // For generating new keys
    std::uniform_real_distribution<double> op_dist(0.0, 1.0); // For choosing operation type

    auto start = std::chrono::high_resolution_clock::now(); // Start timer

    for (int i = 0; i < totalOps; ++i)
    {
        double choice = op_dist(rng);
        int value;

        // Ensure removal/contains don't access empty liveKeys
        if (liveKeys.empty())
            choice = 1.0;

        std::uniform_int_distribution<int> key_pick(0, liveKeys.size() - 1);

        if (choice < 0.8)
        {
            // 80% contains
            value = liveKeys[key_pick(rng)];
            if (set.contains(value))
                stats.hits_contains++;
            else
                stats.misses_contains++;
        }
        else if (choice < 0.9)
        {
            // 10% add
            value = value_gen(rng);
            if (set.add(value))
            {
                stats.successful_adds++;
                liveKeys.push_back(value); // Track key for possible remove
            }
            else
            {
                stats.failed_adds++;
            }
        }
        else
        {
            // 10% remove
            value = liveKeys[key_pick(rng)];
            if (set.remove(value))
            {
                stats.successful_removes++;
                auto it = std::find(liveKeys.begin(), liveKeys.end(), value);
                if (it != liveKeys.end())
                {
                    std::swap(*it, liveKeys.back());
                    liveKeys.pop_back();
                }
            }
            else
            {
                stats.failed_removes++;
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now(); // End timer
    stats.time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// Run benchmark workload on the concurrent cuckoo set using threads
void run_concurrent_benchmark(CuckooConcurrentSet<int> &set, std::vector<int> &liveKeys, int totalOps, Stats &stats)
{
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> value_gen(1, 1000000); // For generating new keys
    std::uniform_real_distribution<double> op_dist(0.0, 1.0); // For choosing operation type

    auto start = std::chrono::high_resolution_clock::now(); // Start timer

    const int numThreads = 16;
    std::vector<std::thread> threads;
    std::mutex liveKeysMutex;

    for (int t = 0; t < numThreads; ++t)
    {
        threads.push_back(std::thread([&set, &liveKeys, totalOps, &stats, &rng, &op_dist, &value_gen, &liveKeysMutex]
                                      {
            for (int i = 0; i < totalOps / numThreads; ++i) {
                double choice = op_dist(rng);
                int value;

                {
                    // std::lock_guard<std::mutex> lock(liveKeysMutex);
                    if (liveKeys.empty()) choice = 1.0;
                }

                if (choice < 0.8) {
                    // 80% contains
                    // std::lock_guard<std::mutex> lock(liveKeysMutex);
                    std::uniform_int_distribution<int> key_pick(0, liveKeys.size() - 1);
                    value = liveKeys[key_pick(rng)];
                    if (set.contains(value)) stats.hits_contains++;
                    else stats.misses_contains++;
                } else if (choice < 0.9) {
                    // 10% add
                    value = value_gen(rng);
                    if (set.add(value)) {
                        stats.successful_adds++;
                        // std::lock_guard<std::mutex> lock(liveKeysMutex);
                        liveKeys.push_back(value);
                    } else {
                        stats.failed_adds++;
                    }
                } else {
                    // 10% remove
                    // std::lock_guard<std::mutex> lock(liveKeysMutex);
                    if (!liveKeys.empty()) {
                        std::uniform_int_distribution<int> key_pick(0, liveKeys.size() - 1);
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
            } }));
    }

    for (auto &th : threads)
        th.join();

    auto end = std::chrono::high_resolution_clock::now();
    stats.time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// Run benchmark workload on the transactional cuckoo set using threads
void run_transactional_benchmark(CuckooTransactionalSet<int> &set, std::vector<int> &liveKeys, int totalOps, Stats &stats)
{
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> value_gen(1, 1000000); // For generating new keys
    std::uniform_real_distribution<double> op_dist(0.0, 1.0); // For choosing operation type

    auto start = std::chrono::high_resolution_clock::now(); // Start timer

    const int numThreads = 16;
    std::vector<std::thread> threads;
    std::mutex liveKeysMutex;

    for (int t = 0; t < numThreads; ++t)
    {
        threads.push_back(std::thread([&set, &liveKeys, totalOps, &stats, &rng, &op_dist, &value_gen, &liveKeysMutex]
                                      {
            for (int i = 0; i < totalOps / numThreads; ++i) {
                double choice = op_dist(rng);
                int value;

                {
                    // std::lock_guard<std::mutex> lock(liveKeysMutex);
                    if (liveKeys.empty()) choice = 1.0;
                }

                if (choice < 0.8) {
                    // 80% contains
                    // std::lock_guard<std::mutex> lock(liveKeysMutex);
                    std::uniform_int_distribution<int> key_pick(0, liveKeys.size() - 1);
                    value = liveKeys[key_pick(rng)];
                    if (set.contains(value)) stats.hits_contains++;
                    else stats.misses_contains++;
                } else if (choice < 0.9) {
                    // 10% add
                    value = value_gen(rng);
                    if (set.add(value)) {
                        stats.successful_adds++;
                        // std::lock_guard<std::mutex> lock(liveKeysMutex);
                        liveKeys.push_back(value);
                    } else {
                        stats.failed_adds++;
                    }
                } else {
                    // 10% remove
                    // std::lock_guard<std::mutex> lock(liveKeysMutex);
                    if (!liveKeys.empty()) {
                        std::uniform_int_distribution<int> key_pick(0, liveKeys.size() - 1);
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
            } }));
    }

    for (auto &th : threads)
        th.join();

    auto end = std::chrono::high_resolution_clock::now();
    stats.time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

int main()
{
    const int NUM_INITIAL_KEYS = 10000; // default 1,000 <- CHANGE FOR SCALE
    const int TOTAL_OPS = 100000;       // default 10,000 <- CHANGE FOR SCALE

    std::vector<int> initialKeys;
    std::unordered_set<int> keyCheck;
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> val_gen(1, 10000000); // default (1, 1,000,000) <- CHANGE FOR SCALE

    // Generate unique initial keys
    while (initialKeys.size() < NUM_INITIAL_KEYS)
    {
        int key = val_gen(rng);
        if (keyCheck.insert(key).second)
        {
            initialKeys.push_back(key);
        }
    }

    // Initialize and populate the set
    CuckooSequentialSet<int> cuckooSet;
    int initialSize = cuckooSet.populate(initialKeys);

    std::cout << "Initial population complete. added: " << initialSize << "\n";

    // Run benchmark for the serial version
    Stats stats_serial;
    std::vector<int> liveKeys_serial = initialKeys; // Used to track valid keys during benchmark
    run_serial_benchmark(cuckooSet, liveKeys_serial, TOTAL_OPS, stats_serial);

    int expectedSize_serial = initialSize + stats_serial.successful_adds - stats_serial.successful_removes;
    int actualSize_serial = cuckooSet.size();

    // Output serial benchmark summary
    std::cout << "=== Cuckoo Sequential Set Benchmark ===\n";
    std::cout << "Operations performed: " << TOTAL_OPS << "\n";
    std::cout << "Contains → Hits: " << stats_serial.hits_contains << ",     Misses: " << stats_serial.misses_contains << "\n";
    std::cout << "Add      → Successes: " << stats_serial.successful_adds << ", Failures: " << stats_serial.failed_adds << "\n";
    std::cout << "Remove   → Successes: " << stats_serial.successful_removes << ", Failures: " << stats_serial.failed_removes << "\n";
    std::cout << "Expected final size: " << expectedSize_serial << "\n";
    std::cout << "Actual final size:   " << actualSize_serial << "\n";
    std::cout << "Size correctness: " << (expectedSize_serial == actualSize_serial ? "PASS ✅" : "FAIL ❌") << "\n";
    std::cout << "Time taken: " << (stats_serial.time_ns / 1000) << " microseconds (µs)\n";

    // Initialize and populate the concurrent set
    CuckooConcurrentSet<int> cuckooConcurrentSet(NUM_INITIAL_KEYS);

    int initialSize_concurrent = cuckooConcurrentSet.populate(initialKeys);
    std::cout << "\nInitial population complete. added: " << initialSize_concurrent << "\n";

    // Run benchmark for the concurrent version
    Stats stats_concurrent;
    std::vector<int> liveKeys_concurrent = initialKeys; // Used to track valid keys during benchmark
    run_concurrent_benchmark(cuckooConcurrentSet, liveKeys_concurrent, TOTAL_OPS, stats_concurrent);

    int expectedSize_concurrent = initialSize_concurrent + stats_concurrent.successful_adds - stats_concurrent.successful_removes;
    int actualSize_concurrent = cuckooConcurrentSet.size();

    // Output concurrent benchmark summary
    std::cout << "=== Cuckoo Concurrent Set Benchmark ===\n";
    std::cout << "Operations performed: " << TOTAL_OPS << "\n";
    std::cout << "Contains → Hits: " << stats_concurrent.hits_contains << ",     Misses: " << stats_concurrent.misses_contains << "\n";
    std::cout << "Add      → Successes: " << stats_concurrent.successful_adds << ", Failures: " << stats_concurrent.failed_adds << "\n";
    std::cout << "Remove   → Successes: " << stats_concurrent.successful_removes << ", Failures: " << stats_concurrent.failed_removes << "\n";
    std::cout << "Expected final size: " << expectedSize_concurrent << "\n";
    std::cout << "Actual final size:   " << actualSize_concurrent << "\n";
    std::cout << "Size correctness: " << (expectedSize_concurrent == actualSize_concurrent ? "PASS ✅" : "FAIL ❌") << "\n";
    std::cout << "Time taken: " << (stats_concurrent.time_ns / 1000) << " microseconds (µs)\n";

    // Initialize and populate the transactional set
    CuckooTransactionalSet<int> cuckooTransactionalSet(NUM_INITIAL_KEYS);

    int initialSize_transactional = cuckooTransactionalSet.populate(initialKeys);
    std::cout << "\nInitial population complete. added: " << initialSize_transactional << "\n";

    // Run benchmark for the transactional version
    Stats stats_transactional;
    std::vector<int> liveKeys_transactional = initialKeys; // Used to track valid keys during benchmark
    run_transactional_benchmark(cuckooTransactionalSet, liveKeys_transactional, TOTAL_OPS, stats_transactional);

    int expectedSize_transactional = initialSize_transactional + stats_transactional.successful_adds - stats_transactional.successful_removes;
    int actualSize_transactional = cuckooTransactionalSet.size();

    // Output transactional benchmark summary
    std::cout << "=== Cuckoo Transactional Set Benchmark ===\n";
    std::cout << "Operations performed: " << TOTAL_OPS << "\n";
    std::cout << "Contains → Hits: " << stats_transactional.hits_contains << ",     Misses: " << stats_transactional.misses_contains << "\n";
    std::cout << "Add      → Successes: " << stats_transactional.successful_adds << ", Failures: " << stats_transactional.failed_adds << "\n";
    std::cout << "Remove   → Successes: " << stats_transactional.successful_removes << ", Failures: " << stats_transactional.failed_removes << "\n";
    std::cout << "Expected final size: " << expectedSize_transactional << "\n";
    std::cout << "Actual final size:   " << actualSize_transactional << "\n";
    std::cout << "Size correctness: " << (expectedSize_transactional == actualSize_transactional ? "PASS ✅" : "FAIL ❌") << "\n";
    std::cout << "Time taken: " << (stats_transactional.time_ns / 1000) << " microseconds (µs)\n";

    return 0;
}
