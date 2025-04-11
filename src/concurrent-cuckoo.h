#include <vector>     // Include the vector library for dynamic array support
#include <iostream>   // Include the iostream library for input and output operations
#include <functional> // Include the functional library for std::hash and other functions
#include <ctime>      // Include the time library for generating time-based salt
#include <list>       // Include the list library for using doubly linked lists
#include <mutex>      // Include the mutex library for thread synchronization
#include <atomic>     // Include the atomic library for thread-safe atomic operations
#include <thread>     // Include the thread library for multi-threading operations

template <class T>
class CuckooConcurrentSet
{
private:
    bool is_resizing = false; // Flag to track if resizing is happening to avoid recursion

    const int PROBE_SIZE = 8;                                              // Size of the probing list in each hash table slot
    const int THRESHOLD = PROBE_SIZE / 2;                                  // Threshold of items in a slot before relocation is triggered
    const int LIMIT = 16;                                                  // Maximum number of relocation attempts before resizing is triggered
    int capacity;                                                          // The current size of the table
    size_t salt0, salt1;                                                   // Salts used for the hashing functions for randomness
    std::vector<std::vector<std::list<T>>> table;                          // The hash table, represented as two vector rows of linked lists
    std::vector<std::vector<std::unique_ptr<std::recursive_mutex>>> locks; // Locks for synchronization

    // Hash function that XORs std::hash with a salt and takes modulo capacity
    int hash(const T &key, size_t seed) const
    {
        return (std::hash<T>{}(key) ^ seed) % capacity; // Combine std::hash of key with salt and modulo by capacity
    }

    // First hash function using salt1
    int hash1(const T &key) const
    {
        return hash(key, salt1); // Using salt1 for the first hash
    }

    // Second hash function using salt0
    int hash2(const T &key) const
    {
        return hash(key, salt0); // Using salt0 for the second hash
    }

    // Relocate an element if a bucket overflows
    bool relocate(int i, int hi)
    {
        int hj = 0;                                 // Holds the index of the bucket in the other table
        int j = 1 - i;                              // The other table (0 or 1)
        for (int round = 0; round < LIMIT; round++) // Try relocating multiple times if needed
        {
            T val = table[i][hi].front(); // Get the value at the head of the list in the current bucket
            switch (i)                    // Choose the hash function based on which table we're working with
            {
            case 0:
                hj = hash1(val); // Use hash1 for the first table
                break;
            case 1:
                hj = hash2(val); // Use hash2 for the second table
                break;
            }
            acquire(val);                                                       // Acquire the lock to synchronize access to the current element
            auto it = std::find(table[i][hi].begin(), table[i][hi].end(), val); // Search for the value in the current table slot
            if (it != table[i][hi].end())                                       // If the value was found
            {
                table[i][hi].erase(it);              // Remove the value from the current slot
                if (table[j][hj].size() < THRESHOLD) // If the other slot is under the threshold
                {
                    table[j][hj].push_back(val); // Move the value to the other table's slot
                    release(val);                // Release the lock after moving the value
                    return true;
                }
                else if (table[j][hj].size() < PROBE_SIZE) // If the slot has room for more values
                {
                    table[j][hj].push_back(val); // Place the value in the other slot
                    i = 1 - i;                   // Swap tables and continue relocating
                    hi = hj;
                    j = 1 - j;
                    release(val); // Release the lock before continuing to the next iteration
                }
                else // If both slots are full, return false
                {
                    table[i][hi].push_back(val);
                    release(val); // Release the lock
                    return false;
                }
            }
            else if (table[i][hi].size() >= THRESHOLD) // If the current slot is over the threshold
            {
                release(val); // Release the lock
                continue;     // Try relocating again
            }
            else // If relocation is successful
            {
                release(val); // Release the lock
                return true;  // Successfully relocated
            }
        }
        return false; // After max attempts, return false if relocation failed
    }

    // Acquire locks for both tables before modifying them
    void acquire(const T &val)
    {
        locks[0][hash1(val) % locks[0].size()]->lock(); // Lock the first table slot
        locks[1][hash2(val) % locks[1].size()]->lock(); // Lock the second table slot
    }

    // Release the locks after modification
    void release(const T &val)
    {
        locks[0][hash1(val) % locks[0].size()]->unlock(); // Unlock the first table slot
        locks[1][hash2(val) % locks[1].size()]->unlock(); // Unlock the second table slot
    }

    // Resize the table when it exceeds capacity
    void resize()
    {
        // Prevent recursion into add during resize
        if (is_resizing)
            return;

        int oldCapacity = capacity;
        for (auto &lock : locks[0]) // Lock all entries of the first table
        {
            lock->lock();
        }

        if (capacity != oldCapacity) // Check if resizing already happened
        {
            return;
        }

        // Start resizing
        is_resizing = true;

        salt0 = time(NULL); // Update salt0 with current time
        salt1 = salt0;      // Set salt1 the same as salt0

        capacity *= 2;                                           // Double the capacity
        std::vector<std::vector<std::list<T>>> old_table(table); // Save old table for re-insertion
        table.clear();                                           // Clear the current table

        // Rebuild the table with new capacity
        for (int i = 0; i < 2; i++)
        {
            std::vector<std::list<T>> row;
            for (int j = 0; j < capacity; j++)
            {
                row.push_back(std::list<T>());
            }
            table.push_back(row);
        }

        // Re-insert all old elements back into the new table
        for (auto &row : old_table)
        {
            for (auto &probe_set : row)
            {
                for (auto &entry : probe_set)
                {
                    add(entry); // Recursively handle resizing during re-insertion
                }
            }
        }

        for (auto &lock : locks[0]) // Release all locks after resizing
        {
            lock->unlock();
        }
    }

public:
    CuckooConcurrentSet(int initial_capacity) : capacity(initial_capacity)
    {
        for (int i = 0; i < 2; i++) // Initialize two hash tables
        {
            std::vector<std::list<T>> row;
            std::vector<std::unique_ptr<std::recursive_mutex>> locks_row;
            for (int j = 0; j < capacity; j++) // Initialize capacity for each table slot
            {
                row.push_back(std::list<T>());
                locks_row.push_back(std::make_unique<std::recursive_mutex>());
            }
            table.push_back(row);
            locks.push_back(std::move(locks_row));
        }
        salt0 = time(NULL); // Initialize salt0 with current time
        salt1 = salt0;      // Set salt1 to the same value as salt0
    }

    bool add(const T &val)
    {
        acquire(val);        // Lock both tables before modifying
        int h0 = hash1(val); // Compute hash1 for the first table
        int h1 = hash2(val); // Compute hash2 for the second table
        int i = -1;
        int h = -1;
        bool mustResize = false; // Flag to check if resizing is needed

        if (contains(val)) // Check if the value already exists
        {
            release(val); // Release the locks before returning
            return false;
        }

        // Attempt to add the value to the first or second table
        if (table[0][h0].size() < THRESHOLD)
        {
            table[0][h0].push_back(val);
            release(val);
            return true;
        }
        else if (table[1][h1].size() < THRESHOLD)
        {
            table[1][h1].push_back(val);
            release(val);
            return true;
        }
        else if (table[0][h0].size() < PROBE_SIZE)
        {
            table[0][h0].push_back(val);
            i = 0;
            h = h0;
        }
        else if (table[1][h1].size() < PROBE_SIZE)
        {
            table[1][h1].push_back(val);
            i = 1;
            h = h1;
        }
        else
        {
            mustResize = true; // Set flag to resize if both slots are full
        }
        release(val);

        if (mustResize) // Resize if needed
        {
            resize();
            add(val); // Retry after resizing
        }
        else if (!relocate(i, h)) // Relocate the element if needed
        {
            resize(); // Resize the table if relocation fails
        }

        return true; // Successfully added the value
    }

    bool remove(const T &val)
    {
        acquire(val);        // Lock both tables before modifying
        int h0 = hash1(val); // Compute hash1 for the first table
        int h1 = hash2(val); // Compute hash2 for the second table
        auto it0 = std::find(table[0][h0].begin(), table[0][h0].end(), val);
        if (it0 != table[0][h0].end()) // Check if the value is found in the first table
        {
            table[0][h0].erase(it0); // Remove the value
            release(val);            // Release the locks before returning
            return true;
        }
        else
        {
            auto it1 = std::find(table[1][h1].begin(), table[1][h1].end(), val);
            if (it1 != table[1][h1].end()) // Check if the value is found in the second table
            {
                table[1][h1].erase(it1); // Remove the value
                release(val);            // Release the locks before returning
                return true;
            }
        }
        release(val); // Release the locks if the value is not found
        return false;
    }

    bool contains(const T &val)
    {
        acquire(val); // Lock both tables before modifying
        bool found = std::find(table[0][hash1(val)].begin(), table[0][hash1(val)].end(), val) != table[0][hash1(val)].end() ||
                     std::find(table[1][hash2(val)].begin(), table[1][hash2(val)].end(), val) != table[1][hash2(val)].end();
        release(val); // Release the locks after checking
        return found; // Return whether the value was found
    }

    int size() const
    {
        int size = 0;
        for (const auto &row : table)
        {
            for (const auto &probe_set : row)
            {
                size += probe_set.size(); // Add up the sizes of all the probe sets
            }
        }
        return size; // Return the total size of the hash set
    }

    int populate(const std::vector<T> &list)
    {
        int added = 0;
        for (const T &value : list)
        {
            if (add(value)) // Attempt to add each value
            {
                added++; // Count the number of successful additions
            }
        }
        return added; // Return the number of values successfully added
    }
};
