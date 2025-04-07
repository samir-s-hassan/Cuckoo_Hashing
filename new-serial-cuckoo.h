#include <vector>     // For std::vector (dynamic arrays)
#include <iostream>   // For std::cout and std::cerr
#include <functional> // For std::hash
#include <ctime>      // For std::time (used for hashing seeds)

// Template class so it works with any type (int, string, etc.)
template <typename T>
class CuckooSequentialSet
{
private:
    // Entry wraps the actual value; used so we can store pointers and handle nulls
    struct Entry
    {
        T val;                 // The actual data stored
        Entry(T v) : val(v) {} // Constructor to initialize val
    };

    int capacity;                            // Number of slots per table
    int maxDisplacements;                    // Max number of attempts before resize
    bool resizing = false;                   // Flag to prevent recursive resize
    size_t salt1, salt2;                     // Two seeds for hash functions (to make them different)
    std::vector<std::vector<Entry *>> table; // Two hash tables (each a vector of pointers)

    // Hash function that XORs std::hash with a salt and takes modulo capacity
    int hash(const T &key, size_t seed) const
    {
        return (std::hash<T>{}(key) ^ seed) % capacity;
    }

    // First hash function using salt1
    int hash1(const T &key) const
    {
        return hash(key, salt1);
    }

    // Second hash function using salt2
    int hash2(const T &key) const
    {
        return hash(key, salt2);
    }

    // Swap the new entry into the specified table slot, return the old entry (can be null)
    Entry *swap(int tableIndex, int idx, Entry *entry)
    {
        Entry *old = table[tableIndex][idx]; // Store the current occupant
        table[tableIndex][idx] = entry;      // Replace with new entry
        return old;                          // Return old occupant (null if empty)
    }

    // Resize the table (double the size) and re-insert all elements
    void resize()
    {
        if (resizing)
            return;      // Avoid recursive resizes
        resizing = true; // Mark that we're resizing

        int oldCap = capacity; // Store old capacity
        capacity *= 2;         // Double the capacity
        maxDisplacements *= 2; // Increase displacement limit
        auto oldTable = table; // Save the old table

        // Create a new empty table of size (2 x new capacity)
        table = std::vector<std::vector<Entry *>>(2, std::vector<Entry *>(capacity, nullptr));

        // Generate new random salts for hashing
        salt1 = std::rand();
        salt2 = std::rand();

        // Re-insert each entry from the old table into the new one
        for (int i = 0; i < 2; ++i)
        {
            for (int j = 0; j < oldCap; ++j)
            {
                if (oldTable[i][j])
                {                                // If the slot isn't empty
                    insert(oldTable[i][j]->val); // Re-insert value into new table
                    delete oldTable[i][j];       // Free old memory
                }
            }
        }

        resizing = false; // Mark resize complete
    }

public:
    // Constructor to initialize capacity, maxDisplacements, salts, and table
    CuckooSequentialSet(int initialCapacity = 16)
        : capacity(initialCapacity),
          maxDisplacements(initialCapacity / 2),
          salt1(std::time(nullptr)),              // Use current time as salt1
          salt2(std::time(nullptr) ^ 0x9e3779b9), // Use XOR of time for salt2
          table(2, std::vector<Entry *>(initialCapacity, nullptr))
    {
    } // Allocate two empty tables

    // Destructor to clean up dynamically allocated memory
    ~CuckooSequentialSet()
    {
        for (auto &row : table)    // For each row (table 0 and 1)
            for (auto entry : row) // For each entry in the row
                delete entry;      // Delete if not null
    }

    // Insert a value using Cuckoo hashing
    bool insert(const T &val)
    {
        if (contains(val))
            return false; // Avoid duplicates

        Entry *temp = new Entry(val); // Wrap the value in a new entry

        // Try to place the entry for up to maxDisplacements times
        for (int i = 0; i < maxDisplacements; ++i)
        {
            int h1 = hash1(temp->val);                 // Get index in table 0
            if ((temp = swap(0, h1, temp)) == nullptr) // Try placing in table 0
                return true;                           // Success if no previous entry

            int h2 = hash2(temp->val);                 // Get index in table 1
            if ((temp = swap(1, h2, temp)) == nullptr) // Try placing in table 1
                return true;                           // Success if no previous entry
        }

        delete temp;        // If we gave up, free memory
        resize();           // Resize the table
        return insert(val); // Try again after resize
    }

    // Remove a value if it exists
    bool remove(const T &val)
    {
        int h1 = hash1(val); // Check table 0
        if (table[0][h1] && table[0][h1]->val == val)
        {
            delete table[0][h1];    // Free memory
            table[0][h1] = nullptr; // Mark as empty
            return true;
        }

        int h2 = hash2(val); // Check table 1
        if (table[1][h2] && table[1][h2]->val == val)
        {
            delete table[1][h2];
            table[1][h2] = nullptr;
            return true;
        }

        return false; // Not found in either table
    }

    // Check if the value is present
    bool contains(const T &val) const
    {
        int h1 = hash1(val); // Check table 0
        if (table[0][h1] && table[0][h1]->val == val)
            return true;

        int h2 = hash2(val); // Check table 1
        if (table[1][h2] && table[1][h2]->val == val)
            return true;

        return false; // Not found
    }

    // Count how many entries are stored in total (non-thread-safe)
    int size() const
    {
        int count = 0;
        for (const auto &row : table)     // For each table row
            for (const auto &entry : row) // For each slot
                if (entry)
                    ++count; // Count non-null entries
        return count;
    }

    // Insert a list of values into the table (non-thread-safe)
    // New safer version: returns number of successful insertions
    int populate(const std::vector<T> &list)
    {
        int inserted = 0;
        for (const T &val : list)
        {
            if (insert(val))
            {
                inserted++;
            }
            else
            {
                std::cerr << "Populate failed: Duplicate or collision at value = " << val << "\n";
            }
        }
        return inserted;
    }
};
