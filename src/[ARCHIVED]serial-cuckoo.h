// #include <vector>
// #include <iostream>
// #include <functional>
// #include <ctime>

// //FIRST SUBMISSION

// // Template class so it can store any type (like int, string, etc.)
// template <typename T>
// class CuckooSequentialSet {
// private:
//     // Simple node wrapper for storing elements (lets us use pointers and detect empty slots)
//     struct Node {
//         T key;
//         Node(T key) : key(key) {}
//     };

//     int capacity;            // Size of each table (we have 2 tables)
//     int maxTries;            // Max number of times we try before resizing
//     bool resizing = false;   // Used to prevent recursive resizing
//     std::vector<std::vector<Node*>> table; // 2D vector: table[0] and table[1]
//     size_t seed0, seed1;     // Seeds used to make two different hash functions

//     // Simple hash function that uses a seed to change behavior
//     int hash(const T& val, size_t seed) {
//         std::hash<T> hasher;
//         return (hasher(val) ^ seed) % capacity;
//     }

//     // First hash function (uses seed0)
//     int hash0(const T& val) {
//         return hash(val, seed0);
//     }

//     // Second hash function (uses seed1)
//     int hash1(const T& val) {
//         return hash(val, seed1);
//     }

//     // Swap the given node into the table and return the old one (if any)
//     Node* kickOut(int tableIndex, int slotIndex, Node* node) {
//         Node* old = table[tableIndex][slotIndex];
//         table[tableIndex][slotIndex] = node;
//         return old;
//     }

//     // Doubles the size of the table and re-inserts everything
//     bool resize() {
//         if (resizing) return false;
//         resizing = true;

//         int oldCapacity = capacity;
//         capacity *= 2;
//         maxTries *= 2;

//         // Save current table
//         std::vector<std::vector<Node*>> oldTable = table;

//         // Make new empty tables
//         table.clear();
//         table.resize(2, std::vector<Node*>(capacity, nullptr));

//         // Use new seeds so that hash functions behave differently
//         seed0 = std::rand();
//         seed1 = std::rand();

//         // Re-insert everything into the new tables
//         for (auto& row : oldTable) {
//             for (Node* node : row) {
//                 if (node && !insert(node->key)) {
//                     resizing = false;
//                     return false;
//                 }
//                 delete node; // Free memory of old node
//             }
//         }

//         resizing = false;
//         return true;
//     }

// public:
//     // Constructor: set initial capacity, hash seeds, and allocate space
//     CuckooSequentialSet(int cap) : capacity(cap), maxTries(cap / 2) {
//         table.resize(2, std::vector<Node*>(capacity, nullptr));
//         std::srand(std::time(0)); // Initialize random seed
//         seed0 = std::rand();      // Random seed for hash0
//         seed1 = std::rand();      // Random seed for hash1
//     }

//     // Destructor: free all dynamically allocated nodes
//     ~CuckooSequentialSet() {
//         for (auto& row : table)
//             for (Node* node : row)
//                 delete node;
//     }

//     // Insert a value into the set
//     bool insert(const T& val) {
//         if (contains(val)) return false; // Don't add duplicates

//         Node* curr = new Node(val);

//         // Try placing the node by swapping it with others
//         for (int i = 0; i < maxTries; i++) {
//             int h0 = hash0(curr->key);
//             curr = kickOut(0, h0, curr);
//             if (!curr) return true; // Successfully inserted

//             int h1 = hash1(curr->key);
//             curr = kickOut(1, h1, curr);
//             if (!curr) return true; // Successfully inserted
//         }

//         // Couldn't place the item, need to resize
//         delete curr;
//         if (!resize()) return false;
//         return insert(val); // Try again after resizing
//     }

//     // Remove a value from the set
//     bool remove(const T& val) {
//         int h0 = hash0(val);
//         if (table[0][h0] && table[0][h0]->key == val) {
//             delete table[0][h0];
//             table[0][h0] = nullptr;
//             return true;
//         }

//         int h1 = hash1(val);
//         if (table[1][h1] && table[1][h1]->key == val) {
//             delete table[1][h1];
//             table[1][h1] = nullptr;
//             return true;
//         }

//         return false; // Not found
//     }

//     // Check if the value exists in the set
//     bool contains(const T& val) {
//         int h0 = hash0(val);
//         if (table[0][h0] && table[0][h0]->key == val) return true;

//         int h1 = hash1(val);
//         if (table[1][h1] && table[1][h1]->key == val) return true;

//         return false;
//     }

//     // Count how many items are currently in the set
//     int size() {
//         int count = 0;
//         for (auto& row : table)
//             for (Node* node : row)
//                 if (node) count++;
//         return count;
//     }

//     // Add multiple items from a vector (used to initialize the set)
//     bool populate(const std::vector<T>& list) {
//         for (const T& val : list) {
//             if (!insert(val)) {
//                 std::cout << "Duplicate: " << val << "\n";
//                 return false;
//             }
//         }
//         return true;
//     }
// };
