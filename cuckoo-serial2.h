#include <vector>       // For using std::vector
#include <iostream>     // For printing errors or messages
#include <functional>   // For std::hash
#include <ctime>        // For seeding with current time

// Template class so it works with any data type (int, string, etc.)
template <typename T>
class SequentialCuckooSet {
private:
    // Each Bucket holds a pointer to an entry (nullptr if empty)
    struct Bucket {
        T* entry = nullptr;  // Default is empty
    };

    int capacity;                // Number of slots per table
    int maxDisplacements;        // Max number of evictions before resizing
    size_t salt1, salt2;         // Two salts to generate two different hash functions
    std::vector<std::vector<Bucket>> table;  // Two hash tables (table[0] and table[1])

    // Hash function using XOR between hashed key and salt
    int hash(const T& key, size_t seed) const {
        return (std::hash<T>{}(key) ^ seed) % capacity;  // Keeps result within bounds
    }

    // First hash function
    int hash1(const T& key) const {
        return hash(key, salt1);
    }

    // Second hash function
    int hash2(const T& key) const {
        return hash(key, salt2);
    }

    // Resize the table when insertion fails due to too many displacements
    void resize() {
        int oldCapacity = capacity;        // Store the old size
        capacity *= 2;                     // Double the capacity
        maxDisplacements *= 2;            // Allow more displacements
        auto oldTable = table;            // Copy old table contents

        // Create a new, empty table with double capacity
        table = std::vector<std::vector<Bucket>>(2, std::vector<Bucket>(capacity));

        // Create new salts for new hash functions
        salt1 = std::time(nullptr);
        salt2 = salt1 ^ 0x9e3779b9;  // Just make sure salt2 is different from salt1

        // Re-insert all elements into new table
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < oldCapacity; ++j) {
                if (oldTable[i][j].entry) {
                    insert(*oldTable[i][j].entry);  // Reinsert the value
                    delete oldTable[i][j].entry;    // Free the memory of the old entry
                }
            }
        }
    }

public:
    // Constructor: sets capacity, maxDisplacements, hash seeds, and initializes table
    SequentialCuckooSet(int initialCapacity = 16)
        : capacity(initialCapacity),
          maxDisplacements(initialCapacity / 2),
          salt1(std::time(nullptr)),
          salt2(std::time(nullptr) ^ 0x9e3779b9) {
        table = std::vector<std::vector<Bucket>>(2, std::vector<Bucket>(capacity));
    }

    // Destructor: free all dynamically allocated entries
    ~SequentialCuckooSet() {
        for (auto& row : table) {
            for (auto& bucket : row) {
                delete bucket.entry;  // Safely delete each pointer if it's not nullptr
            }
        }
    }

    // Inserts a value into the table, using cuckoo hashing logic
    bool insert(const T& val) {
        if (contains(val)) return false;  // Do nothing if value is already present

        T* displaced = new T(val);  // Create a pointer to the value to insert

        // Try to insert by displacing existing items
        for (int i = 0; i < maxDisplacements; ++i) {
            int h1 = hash1(*displaced);  // First hash
            if (!table[0][h1].entry) {   // Empty slot in table[0]
                table[0][h1].entry = displaced;
                return true;
            }
            std::swap(displaced, table[0][h1].entry);  // Kick out existing and try again

            int h2 = hash2(*displaced);  // Second hash
            if (!table[1][h2].entry) {   // Empty slot in table[1]
                table[1][h2].entry = displaced;
                return true;
            }
            std::swap(displaced, table[1][h2].entry);  // Kick again and loop
        }

        // If too many displacements, delete temp and resize the table
        delete displaced;
        resize();
        return insert(val);  // Try inserting again after resize
    }

    // Removes a value if it's present
    bool remove(const T& val) {
        int h1 = hash1(val);  // Get index for table[0]
        if (table[0][h1].entry && *table[0][h1].entry == val) {
            delete table[0][h1].entry;    // Free memory
            table[0][h1].entry = nullptr; // Mark as empty
            return true;
        }

        int h2 = hash2(val);  // Get index for table[1]
        if (table[1][h2].entry && *table[1][h2].entry == val) {
            delete table[1][h2].entry;
            table[1][h2].entry = nullptr;
            return true;
        }

        return false;  // Not found in either table
    }

    // Checks if a value exists in the set
    bool contains(const T& val) const {
        int h1 = hash1(val);  // First index
        if (table[0][h1].entry && *table[0][h1].entry == val)
            return true;

        int h2 = hash2(val);  // Second index
        if (table[1][h2].entry && *table[1][h2].entry == val)
            return true;

        return false;  // Not in table
    }

    // Counts how many values are in the set (not thread-safe)
    int size() const {
        int count = 0;
        for (const auto& row : table) {
            for (const auto& bucket : row) {
                if (bucket.entry) ++count;
            }
        }
        return count;
    }

    // Inserts a list of values all at once (non-thread-safe)
    bool populate(const std::vector<T>& elements) {
        for (const T& el : elements) {
            if (!insert(el)) {
                std::cerr << "Populate failed: Duplicate or insertion failure.\n";
                return false;
            }
        }
        return true;
    }
};
