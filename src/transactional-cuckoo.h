#include <vector>     // For std::vector (dynamic arrays)
#include <iostream>   // For std::cout and std::cerr
#include <functional> // For std::hash
#include <ctime>      // For std::time (used for hashing seeds)
#include <atomic>     // For atomic variables to ensure consistent memory ordering

/*This class implements a Cuckoo Hash Set with transactional support. The key operations like add, remove, and contains are wrapped in atomic transactions to ensure that these operations are atomic and consistent, even in a multi-threaded environment. The set uses two hash tables to store entries, with probing to handle collisions and displacement to move entries in case of conflicts. The set dynamically resizes when it becomes full and ensures thread safety using atomic operations. The class uses C++ transactional memory features to ensure that the add, remove, and contains operations are performed safely across multiple threads.
 */

template <typename T>
class CuckooTransactionalSet
{
private:
    // Entry wraps the actual value; used so we can store pointers and handle nulls
    struct Entry
    {
        T value;                                 // The actual data stored
        Entry(T initValue) : value(initValue) {} // Constructor to initialize 'value'
    };

    int capacity;                            // Number of slots per table
    int maxDisplacements;                    // Max number of attempts before resize
    std::atomic<bool> resizing{false};       // Flag to prevent recursive resize
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

    // Resize the table (double the size) and re-add all elements
    void resize()
    {
        // Use compare_exchange to ensure only one thread performs the resize
        bool expected = false;
        if (!resizing.compare_exchange_strong(expected, true))
            return; // Another thread is already resizing

        int oldCap = capacity; // Store old capacity
        capacity *= 2;         // Double the capacity
        maxDisplacements *= 2; // Increase displacement limit

        // Create a temporary vector to hold all current values
        std::vector<T> allValues;
        allValues.reserve(oldCap); // Reserve space to avoid reallocations

        // Extract all values from current table
        for (int i = 0; i < 2; ++i)
        {
            for (int j = 0; j < oldCap; ++j)
            {
                if (table[i][j])
                {
                    allValues.push_back(table[i][j]->value);
                    delete table[i][j]; // Free old memory
                    table[i][j] = nullptr;
                }
            }
        }

        // Create new empty table of increased size
        table = std::vector<std::vector<Entry *>>(2, std::vector<Entry *>(capacity, nullptr));

        // Generate new random salts for hashing
        salt1 = std::rand();
        salt2 = std::rand();

        // Re-add all collected values
        for (const T &val : allValues)
        {
            // Using internal add without transactions to avoid nesting issues
            bool added = false;
            Entry *entry = new Entry(val);
            Entry *temp = entry;

            for (int i = 0; i < maxDisplacements && temp != nullptr; ++i)
            {
                int h1 = hash1(temp->value);
                temp = swap(0, h1, temp);
                if (temp == nullptr)
                {
                    added = true;
                    break;
                }

                int h2 = hash2(temp->value);
                temp = swap(1, h2, temp);
                if (temp == nullptr)
                {
                    added = true;
                    break;
                }
            }

            if (!added)
            {
                if (temp != nullptr)
                {
                    delete temp; // Clean up if we couldn't add
                }
                std::cerr << "Warning: Failed to add element during resize.\n";
            }
        }

        resizing.store(false); // Mark resize as complete
    }

public:
    // Constructor to initialize capacity, maxDisplacements, salts, and table
    CuckooTransactionalSet(int initialCapacity = 32)
        : capacity(initialCapacity),
          maxDisplacements(initialCapacity / 2),
          salt1(std::time(nullptr)),              // Use current time as salt1
          salt2(std::time(nullptr) ^ 0x9e3779b9), // Use XOR of time for salt2
          table(2, std::vector<Entry *>(initialCapacity, nullptr))
    {
        std::srand(std::time(nullptr)); // Initialize random seed
    }

    // Destructor to clean up dynamically allocated memory
    ~CuckooTransactionalSet()
    {
        for (auto &row : table)    // For each row (table 0 and 1)
            for (auto entry : row) // For each entry in the row
                delete entry;      // Delete if not null
    }

    // Add a value using Cuckoo hashing inside a transaction
    bool add(const T &value)
    {
        // First check if value already exists to avoid transaction overhead
        bool valueExists = false;

        __transaction_atomic
        {
            valueExists = contains(value);
        }

        if (valueExists)
            return false; // Avoid duplicates

        // Create entry outside transaction
        Entry *entry = new Entry(value);
        T valueCopy = value; // Make a copy in case we need to retry
        bool success = false;
        Entry *leftover = nullptr;

        __transaction_atomic
        {
            Entry *temp = entry;

            for (int i = 0; i < maxDisplacements && temp != nullptr; ++i)
            {
                int h1 = hash1(temp->value);
                temp = swap(0, h1, temp);
                if (temp == nullptr)
                {
                    success = true;
                    break;
                }

                int h2 = hash2(temp->value);
                temp = swap(1, h2, temp);
                if (temp == nullptr)
                {
                    success = true;
                    break;
                }
            }

            leftover = temp; // Save any displaced entry for cleanup outside transaction
        }

        // Handle cleanup and resize outside transaction
        if (!success)
        {
            if (leftover != nullptr)
                delete leftover;
            delete entry;

            // Resize and try again
            resize();
            return add(valueCopy); // Recursive call with original value
        }

        return success;
    }

    // Remove a value if it exists using a transaction
    bool remove(const T &value)
    {
        bool found = false;
        Entry *entryToDelete = nullptr;

        __transaction_atomic
        {
            int h1 = hash1(value);
            if (table[0][h1] && table[0][h1]->value == value)
            {
                entryToDelete = table[0][h1];
                table[0][h1] = nullptr;
                found = true;
            }
            else
            {
                int h2 = hash2(value);
                if (table[1][h2] && table[1][h2]->value == value)
                {
                    entryToDelete = table[1][h2];
                    table[1][h2] = nullptr;
                    found = true;
                }
            }
        }

        // Clean up memory outside transaction
        if (found && entryToDelete)
        {
            delete entryToDelete;
        }

        return found;
    }

    // Check if the value is present using a transaction
    bool contains(const T &value) const
    {
        bool found = false;

        __transaction_atomic
        {
            int h1 = hash1(value);
            if (table[0][h1] && table[0][h1]->value == value)
            {
                found = true;
            }
            else
            {
                int h2 = hash2(value);
                if (table[1][h2] && table[1][h2]->value == value)
                {
                    found = true;
                }
            }
        }

        return found;
    }

    // Count how many entries are stored in total (non-thread-safe)
    int size() const
    {
        int count = 0;
        for (const auto &row : table)
            for (const auto &entry : row)
                if (entry)
                    ++count;
        return count;
    }

    // Add a list of values into the table (non-thread-safe)
    int populate(const std::vector<T> &list)
    {
        int added = 0;
        for (const T &value : list)
        {
            if (add(value))
            {
                added++;
            }
        }
        return added;
    }
};