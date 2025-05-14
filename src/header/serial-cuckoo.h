#include <vector>     // For std::vector (dynamic arrays)
#include <iostream>   // For std::cout and std::cerr
#include <functional> // For std::hash
#include <ctime>      // For std::time (used for hashing seeds)

// This class implements a Cuckoo Hash Set using two hash tables (Cuckoo hashing).
// It dynamically resizes the set when necessary and uses two hash functions with different salts to store elements. CuckooSequentialSet is a hash set implemented with two hash tables, using Cuckoo hashing to place elements. It automatically resizes when necessary.

template <typename T>
class CuckooSequentialSet
{
private:
    // Entry wraps the actual value; used so we can store pointers and handle nulls.
    struct Entry
    {
        T value;                                 // The actual data stored in the entry.
        Entry(T initValue) : value(initValue) {} // Constructor to initialize 'value' with initValue.
    };

    int capacity;                            // The number of slots per table (capacity of the hash tables).
    int maxDisplacements;                    // The maximum number of attempts to place an item before resizing.
    bool resizing = false;                   // Flag to prevent recursive resize during the resizing process.
    size_t salt1, salt2;                     // Two different seeds (salts) for hash functions to make them independent.
    std::vector<std::vector<Entry *>> table; // Two hash tables (each a vector of pointers to Entry objects).

    // Hash function that XORs std::hash with a salt and takes modulo capacity.
    int hash(const T &key, size_t seed) const
    {
        // This function hashes the key using std::hash and applies the XOR with the salt (seed) and the modulo capacity to get the index.
        return (std::hash<T>{}(key) ^ seed) % capacity;
    }

    // First hash function using salt1.
    int hash1(const T &key) const
    {
        return hash(key, salt1);  // Calls the general hash function with salt1.
    }

    // Second hash function using salt2.
    int hash2(const T &key) const
    {
        return hash(key, salt2);  // Calls the general hash function with salt2.
    }

    // Swap the new entry into the specified table slot and return the old entry (can be null).
    Entry *swap(int tableIndex, int idx, Entry *entry)
    {
        // Swap the current entry at table[tableIndex][idx] with the new entry and return the old entry.
        Entry *old = table[tableIndex][idx];  // Store the current occupant of the slot.
        table[tableIndex][idx] = entry;       // Replace the current entry with the new entry.
        return old;                           // Return the old entry (null if the slot was empty).
    }

    // Resize the table (double the size) and re-add all elements from the old table into the new one.
    void resize()
    {
        if (resizing) 
            return;      // If resizing is already in progress, avoid a recursive resize.
        resizing = true; // Mark that resizing is in progress.

        int oldCap = capacity; // Store the old capacity.
        capacity *= 2;         // Double the capacity of the hash tables.
        maxDisplacements *= 2; // Double the maximum displacement limit.
        auto oldTable = table; // Save the old table to rehash the existing elements.

        // Create a new empty table with 2 hash tables of the new capacity.
        table = std::vector<std::vector<Entry *>>(2, std::vector<Entry *>(capacity, nullptr));

        // Generate new random salts for hashing (ensures a different hash function after resizing).
        salt1 = std::rand();
        salt2 = std::rand();

        // Re-add each entry from the old table into the new one.
        for (int i = 0; i < 2; ++i)
        {
            for (int j = 0; j < oldCap; ++j)
            {
                if (oldTable[i][j])
                {                               // If the slot isn't empty, rehash the element.
                    add(oldTable[i][j]->value); // Re-add value into the new table.
                    delete oldTable[i][j];      // Free old memory for the element.
                }
            }
        }

        resizing = false; // Mark the resizing process as complete.
    }

public:
    // Constructor to initialize capacity, maxDisplacements, salts, and table.
    CuckooSequentialSet(int initialCapacity)
        : capacity(initialCapacity),
          maxDisplacements(initialCapacity / 2),           // Set maxDisplacements to half the initial capacity.
          salt1(std::time(nullptr)),                        // Use current time as salt1.
          salt2(std::time(nullptr) ^ 0x9e3779b9),           // Use XOR of time for salt2.
          table(2, std::vector<Entry *>(initialCapacity, nullptr)) // Allocate two empty tables.
    {
    }

    // Destructor to clean up dynamically allocated memory.
    ~CuckooSequentialSet()
    {
        for (auto &row : table)    // For each row (table 0 and table 1).
            for (auto entry : row) // For each entry in the row.
                delete entry;      // Delete entry if not null.
    }

    // Add a value using Cuckoo hashing.
    bool add(const T &value)
    {
        if (contains(value)) 
            return false;  // Avoid duplicates, return false if the value already exists.

        Entry *temp = new Entry(value); // Wrap the value in a new entry object.

        // Try to place the entry for up to maxDisplacements times.
        for (int i = 0; i < maxDisplacements; ++i)
        {
            int h1 = hash1(temp->value);               // Get index in table 0 using hash1.
            if ((temp = swap(0, h1, temp)) == nullptr) // Try placing in table 0.
                return true;                           // Success if no previous entry.

            int h2 = hash2(temp->value);               // Get index in table 1 using hash2.
            if ((temp = swap(1, h2, temp)) == nullptr) // Try placing in table 1.
                return true;                           // Success if no previous entry.
        }

        delete temp;       // If we gave up after maxDisplacements, free memory.
        resize();          // Resize the table since the insertion failed.
        return add(value); // Try again after resizing.
    }

    // Remove a value if it exists in the set.
    bool remove(const T &value)
    {
        int h1 = hash1(value); // Check table 0 using hash1.
        if (table[0][h1] && table[0][h1]->value == value)
        {
            delete table[0][h1];    // Free memory for the entry.
            table[0][h1] = nullptr; // Mark the slot as empty.
            return true;
        }

        int h2 = hash2(value); // Check table 1 using hash2.
        if (table[1][h2] && table[1][h2]->value == value)
        {
            delete table[1][h2];   // Free memory for the entry.
            table[1][h2] = nullptr; // Mark the slot as empty.
            return true;
        }

        return false; // Return false if the value is not found in either table.
    }

    // Check if the value is present in the set.
    bool contains(const T &value) const
    {
        int h1 = hash1(value); // Check table 0 using hash1.
        if (table[0][h1] && table[0][h1]->value == value)
            return true;

        int h2 = hash2(value); // Check table 1 using hash2.
        if (table[1][h2] && table[1][h2]->value == value)
            return true;

        return false; // Return false if the value is not found in either table.
    }

    // Count how many entries are stored in the set (non-thread-safe).
    int size() const
    {
        int count = 0;
        for (const auto &row : table)     // For each row (table 0 and 1).
            for (const auto &entry : row) // For each slot in the row.
                if (entry)               // If the slot is not empty, increment the count.
                    ++count;
        return count; // Return the total count of non-null entries.
    }

    // Add a list of values into the set (non-thread-safe).
    // Returns the number of successful additions.
    int populate(const std::vector<T> &list)
    {
        int added = 0;
        for (const T &value : list)
        {
            if (add(value)) // Attempt to add each value from the list.
            {
                added++; // Increment added if successful.
            }
        }
        return added; // Return the total number of successful additions.
    }
};
