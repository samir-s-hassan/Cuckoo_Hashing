#include <iostream>
#include <vector>
#include <random>
#include <thread>
#include <atomic>
#include <chrono>
#include "cuckoo-serial2.h"  // Make sure this matches your filename

// Parameters
const int NUM_THREADS = 4;
const int OPERATIONS_PER_THREAD = 10000;
const int INITIAL_POPULATION = 1000;

// Global counters
std::atomic<int> inserts_done(0);
std::atomic<int> removes_done(0);
std::atomic<int> contains_done(0);

// Thread worker function
void runOperations(CuckooSequentialSet<int>& set, const std::vector<int>& keys, int thread_id) {
    std::mt19937 rng(thread_id + std::random_device{}());  // Thread-local RNG
    std::uniform_int_distribution<int> dist(0, keys.size() - 1);
    std::uniform_real_distribution<double> op_dist(0.0, 1.0);

    for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
        int key = keys[dist(rng)];
        double op_type = op_dist(rng);

        if (op_type < 0.8) {
            set.contains(key);
            contains_done++;
        } else if (op_type < 0.9) {
            set.insert(key);
            inserts_done++;
        } else {
            set.remove(key);
            removes_done++;
        }
    }
}

int main() {
    // Initialize data structure
    CuckooSequentialSet<int> set;
    std::vector<int> keys;

    // Populate with initial values
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1, 1000000);

    for (int i = 0; i < INITIAL_POPULATION; ++i) {
        int key = dist(rng);
        keys.push_back(key);
        set.insert(key);
    }

    std::cout << "Initial population complete. Size: " << set.size() << std::endl;

    // Launch threads
    std::vector<std::thread> threads;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(runOperations, std::ref(set), std::cref(keys), i);
    }

    // Wait for all threads to finish
    for (auto& t : threads) t.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double>(end_time - start_time).count();

    // Final results
    std::cout << "\n--- Benchmark Results ---" << std::endl;
    std::cout << "Threads: " << NUM_THREADS << std::endl;
    std::cout << "Operations per thread: " << OPERATIONS_PER_THREAD << std::endl;
    std::cout << "Total operations: " << NUM_THREADS * OPERATIONS_PER_THREAD << std::endl;
    std::cout << "Contains: " << contains_done.load() << std::endl;
    std::cout << "Inserts: " << inserts_done.load() << std::endl;
    std::cout << "Removes: " << removes_done.load() << std::endl;
    std::cout << "Final set size: " << set.size() << std::endl;
    std::cout << "Time taken: " << duration << " seconds" << std::endl;

    return 0;
}
