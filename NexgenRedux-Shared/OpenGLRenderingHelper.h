#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

#pragma once

#include <map>
#include <string>

namespace NexgenRedux
{
	class OpenGLRenderingHelper
	{		
	public:
		static bool Init(uint32_t windowHandle);
	private:
		static void CreateShaderLookup(std::string shaderName);
		static bool AddShaderLookupKeyValue(std::string shaderName, std::string key, uint32_t value);
		static bool GetShaderLookupValue(std::string shaderName, std::string key, uint32_t& value);
	};
}

#endif


// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

// #define HASH_SIZE 100

// typedef struct node_t {
//     char *key;
//     char *value;
//     struct node_t *next;
// } node;

// node hash_table[HASH_SIZE];

// // Hash function
// unsigned long hash(char *str) {
//     unsigned long hash = 5381;
//     int c;
//     while ((c = *str++))
//         hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
//     return hash % HASH_SIZE;
// }

// // Insert a key-value pair into the hash table
// void hash_insert(char *key, char *value) {
//     // Compute hash index
//     unsigned long index = hash(key);
    
//     // Find next free node in linked list at index
//     node *curr = &hash_table[index];
//     while (curr->next != NULL) {
//         curr = curr->next;
//     }
    
//     // Insert node into linked list
//     node new_node;
//     new_node.key = key;
//     new_node.value = value;
//     new_node.next = NULL;
//     *curr = new_node;
// }

// // Lookup a value in the hash table by key
// char *hash_lookup(char *key) {
//     // Compute hash index
//     unsigned long index = hash(key);
    
//     // Traverse linked list at index
//     node *curr = &hash_table[index];
//     while (curr != NULL) {
//         if (strcmp(curr->key, key) == 0) {
//             return curr->value;
//         }
//         curr = curr->next;
//     }
    
//     // Key not found
//     return NULL;
// }

// // Remove a key-value pair from the hash table by key
// void hash_delete(char *key) {
//     // Compute hash index
//     unsigned long index = hash(key);
    
//     // Traverse linked list at index
//     node *curr = &hash_table[index];
//     node *prev = NULL;
//     while (curr != NULL) {
//         if (strcmp(curr->key, key) == 0) {
//             // Remove node from linked list
//             if (prev == NULL) {
//                 *curr = *(curr->next);
//             } else {
//                 prev->next = curr->next;
//             }
//             return;
//         }
//         prev = curr;
//         curr = curr->next;
//     }
// }

// int main() {
//     // Insert some key-value pairs
//     hash_insert("apple", "red");
//     hash_insert("banana", "yellow");
//     hash_insert("cherry", "red");
    
//     // Lookup some keys
//     printf("%s\n", hash_lookup("apple"));
//     printf("%s\n", hash_lookup("banana"));
//     printf("%s\n", hash_lookup("cherry"));
//     printf("%s\n", hash_lookup("orange"));
    
//     // Delete a key-value pair
//     hash_delete("cherry");
    
//     // Lookup some keys again
//     printf("%s\n", hash_lookup("apple"));
//     printf("%s\n", hash_lookup("banana"));
//     printf("%s\n", hash_lookup("cherry"));
//     printf("%s\n", hash_lookup("orange"));
    
//     return 0;
// }
























// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <stdbool.h>
// #include <stdint.h>

// #define INITIAL_SIZE 100
// #define GROW_FACTOR 2
// #define LOAD_FACTOR 0.75

// typedef struct node_t {
//     char *key;
//     int value;
// } node;

// typedef struct {
//     node nodes[INITIAL_SIZE];
//     size_t size;
//     size_t count;
// } hashtable;

// // Initialize a new hash table
// void hash_init(hashtable *ht) {
//     ht->size = INITIAL_SIZE;
//     ht->count = 0;
//     for (size_t i = 0; i < ht->size; i++) {
//         ht->nodes[i].key = NULL;
//         ht->nodes[i].value = 0;
//     }
// }

// // Free the memory used by a hash table
// void hash_destroy(hashtable *ht) {
//     // No need to free anything
//     ht->size = 0;
//     ht->count = 0;
// }

// // Hash function (FNV-1a)
// uint32_t hash(char *str) {
//     uint32_t hash = 2166136261u;
//     int c;
//     while ((c = *str++)) {
//         hash ^= c;
//         hash *= 16777619;
//     }
//     return hash;
// }

// // Insert a key-value pair into the hash table
// void hash_insert(hashtable *ht, char *key, int value) {
//     // Check load factor and resize if necessary
//     if ((double)ht->count / ht->size > LOAD_FACTOR) {
//         size_t new_size = ht->size * GROW_FACTOR;
//         node new_nodes[new_size];
//         for (size_t i = 0; i < new_size; i++) {
//             new_nodes[i].key = NULL;
//             new_nodes[i].value = 0;
//         }
//         for (size_t i = 0; i < ht->size; i++) {
//             node *curr = &ht->nodes[i];
//             if (curr->key != NULL) {
//                 // Compute new index for node
//                 uint32_t index = hash(curr->key) % new_size;
                
//                 // Insert node into new array
//                 node *new_node = &new_nodes[index];
//                 while (new_node->key != NULL) {
//                     new_node++;
//                 }
//                 *new_node = *curr;
//             }
//         }
//         memcpy(ht->nodes, new_nodes, new_size * sizeof(node));
//         ht->size = new_size;
//     }
    
//     // Compute hash index
//     uint32_t index = hash(key) % ht->size;
    
//     // Find next free node in array at index
//     node *curr = &ht->nodes[index];
//     while (curr->key != NULL) {
//         curr++;
//     }
    
//     // Insert node into array
//     curr->key = key;
//     curr->value = value;
    
//     // Increment count
//     ht->count++;
// }

// // Lookup a value in the hash table by key
// int hash_lookup(hashtable *ht, char *key) {
//     // Compute hash index
//     uint32_t index = hash(key) % ht->size;
    
//     // Traverse array at index
//     node *curr = &ht->nodes[index];
//     while (curr->key != NULL) {
//         if (strcmp(curr->key, key) == 0) {
//             return curr->value;
//         }
//         curr++;
//     }
    
//     // Key not found
//     return 0;
// }

// // Remove a key-value pair from the hash table
// void hash_delete(hashtable *ht, char *key) {
//     // Compute hash index
//     uint32_t index = hash(key) % ht->size;
    
//     // Traverse array at index
//     node *curr = &ht->nodes[index];
//     while (curr->key != NULL) {
//         if (strcmp(curr->key, key) == 0) {
//             // Shift all nodes after the current node one position to the left
//             while (curr->key != NULL) {
//                 node *next = curr + 1;
//                 if (next->key != NULL) {
//                     *curr = *next;
//                 } else {
//                     curr->key = NULL;
//                     curr->value = 0;
//                 }
//                 curr = next;
//             }
//             ht->count--;
//             break;
//         }
//         curr++;
//     }
// }

// // Example usage
// int main() {
//     hashtable ht;
//     hash_init(&ht);
//     hash_insert(&ht, "apple", 1);
//     hash_insert(&ht, "banana", 2);
//     hash_insert(&ht, "cherry", 3);
//     printf("%d\n", hash_lookup(&ht, "banana"));  // prints 2
//     hash_delete(&ht, "banana");
//     printf("%d\n", hash_lookup(&ht, "banana"));  // prints 0
//     hash_destroy(&ht);
//     return 0;
// }