#include <vector>     // For std::vector (dynamic arrays)
#include <iostream>   // For std::cerr
#include <mutex>      // For std::mutex
#include <memory>     // For std::unique_ptr
#include <ctime>      // For std::time (used for salting hashes)
#include <functional> // For std::hash
#include <thread>     // For std::this_thread::yield (used during resize)

// Template class supporting any type T
template <typename T>
class CuckooConcurrentSet
{
private:
    // Entry wraps the actual value; used to enable pointer storage and null checks
    struct Entry
    {
        T value;                                 // The actual data stored in the set
        Entry(T initValue) : value(initValue) {} // Constructor to initialize value
    };

    int capacity;         // Number of slots per table
    int maxDisplacements; // Maximum number of displacements before triggering a resize
    size_t salt1, salt2;  // Salts used in the hash functions to randomize placement

    std::vector<std::vector<Entry *>> table;    // Two hash tables for cuckoo hashing
    std::vector<std::vector<std::mutex>> locks; // Locks for each slot in the hash tables
    std::mutex resizeLock;                      // Global lock used to protect the resize operation

    // Hash function: applies std::hash and mixes with a salt, then mods by table capacity
    int hash(const T &key, size_t seed) const
    {
        return (std::hash<T>{}(key) ^ seed) % capacity;
    }

    // First hash function using salt1
    int hash1(const T &key) const { return hash(key, salt1); }

    // Second hash function using salt2
    int hash2(const T &key) const { return hash(key, salt2); }

    // Acquire a lock for a specific table and index
    std::unique_lock<std::mutex> acquire(int tableIndex, int index)
    {
        return std::unique_lock<std::mutex>(locks[tableIndex][index]);
    }

    // Resize the table by doubling the capacity and rehashing all elements
    void resize()
    {
        std::unique_lock<std::mutex> globalResize(resizeLock); // Lock global resize to prevent concurrent resizes

        int oldCapacity = capacity; // Save old capacity
        capacity *= 2;              // Double the capacity
        maxDisplacements *= 2;     // Increase the max displacement threshold

        // Allocate new tables and locks
        std::vector<std::vector<Entry *>> newTable(2, std::vector<Entry *>(capacity, nullptr));
        std::vector<std::vector<std::mutex>> newLocks(2, std::vector<std::mutex>(capacity));

        // Generate new random salts to rehash with new distribution
        salt1 = std::rand();
        salt2 = std::rand();

        // Rehash all existing entries into new table
        for (int t = 0; t < 2; ++t)
        {
            for (int i = 0; i < oldCapacity; ++i)
            {
                Entry *entry = table[t][i];
                if (entry)
                {
                    bool placed = false; // Track if rehashing succeeded
                    for (int d = 0; d < maxDisplacements; ++d)
                    {
                        int h1 = (std::hash<T>{}(entry->value) ^ salt1) % capacity;
                        if (!newTable[0][h1])
                        {
                            newTable[0][h1] = entry;
                            placed = true;
                            break;
                        }
                        else
                        {
                            std::swap(entry, newTable[0][h1]);
                        }

                        int h2 = (std::hash<T>{}(entry->value) ^ salt2) % capacity;
                        if (!newTable[1][h2])
                        {
                            newTable[1][h2] = entry;
                            placed = true;
                            break;
                        }
                        else
                        {
                            std::swap(entry, newTable[1][h2]);
                        }
                    }
                    if (!placed)
                    {
                        delete entry; // Drop value if cannot be placed after max displacements
                    }
                }
            }
        }

        table = std::move(newTable); // Replace old table
        locks = std::move(newLocks); // Replace old locks
    }

public:
    // Constructor initializes the table and locks
    CuckooConcurrentSet(int initialCapacity = 16)
        : capacity(initialCapacity),
          maxDisplacements(initialCapacity / 2),
          salt1(std::time(nullptr)),
          salt2(std::time(nullptr) ^ 0x9e3779b9),
          table(2, std::vector<Entry *>(initialCapacity, nullptr)),
          locks(2, std::vector<std::mutex>(initialCapacity)) {}

    // Destructor to clean up heap-allocated entries
    ~CuckooConcurrentSet()
    {
        for (auto &row : table)
            for (auto &entry : row)
                delete entry;
    }

    // Adds a value to the table using cuckoo displacement
    bool add(const T &value)
    {
        if (contains(value))
            return false; // Avoid duplicates

        Entry *displaced = new Entry(value); // Wrap value into entry pointer

        for (int i = 0; i < maxDisplacements; ++i)
        {
            int h1 = hash1(displaced->value);
            {
                auto lock = acquire(0, h1);
                if (!table[0][h1])
                {
                    table[0][h1] = displaced;
                    return true;
                }
                std::swap(displaced, table[0][h1]);
            }

            int h2 = hash2(displaced->value);
            {
                auto lock = acquire(1, h2);
                if (!table[1][h2])
                {
                    table[1][h2] = displaced;
                    return true;
                }
                std::swap(displaced, table[1][h2]);
            }
        }

        delete displaced;   // Could not insert: cleanup
        resize();           // Resize to make space
        return add(value); // Retry add after resize
    }

    // Check whether the value exists in either table
    bool contains(const T &value)
    {
        int h1 = hash1(value);
        {
            auto lock = acquire(0, h1);
            if (table[0][h1] && table[0][h1]->value == value)
                return true;
        }

        int h2 = hash2(value);
        {
            auto lock = acquire(1, h2);
            if (table[1][h2] && table[1][h2]->value == value)
                return true;
        }

        return false; // Not found in either table
    }

    // Remove a value from the table if present
    bool remove(const T &value)
    {
        int h1 = hash1(value);
        {
            auto lock = acquire(0, h1);
            if (table[0][h1] && table[0][h1]->value == value)
            {
                delete table[0][h1];
                table[0][h1] = nullptr;
                return true;
            }
        }

        int h2 = hash2(value);
        {
            auto lock = acquire(1, h2);
            if (table[1][h2] && table[1][h2]->value == value)
            {
                delete table[1][h2];
                table[1][h2] = nullptr;
                return true;
            }
        }

        return false; // Not found
    }

    // Count how many entries are stored in total (non-thread-safe)
    int size() const
    {
        int count = 0;
        for (const auto &row : table)         // For each table row
            for (const auto &entry : row)     // For each slot in row
                if (entry)
                    ++count; // Count non-null entries
        return count;
    }

    // Populate with a list of values (non-thread-safe)
    int populate(const std::vector<T> &list)
    {
        int added = 0;
        for (const T &value : list)
        {
            if (add(value))
            {
                added++; // Successfully added
            }
        }
        return added;
    }
};
