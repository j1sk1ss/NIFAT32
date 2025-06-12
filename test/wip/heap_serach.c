/*
Idea in order all directory_entry_t in cluster by heap.
First idea is to reserve entire cluster befor entry chain
Second idea is to use lazy-cache approach
*/

#include <stdio.h>
#include <stdint.h>

#define MAX_HEAP_SIZE 200
#define MAX_HASH      400

typedef struct {
    unsigned int hash;
    int index;
} entry_info_t;

int heap_size = 0;
entry_info_t heap[MAX_HEAP_SIZE];
int hash_to_index[MAX_HASH];

void init_hash_table() {
    for (int i = 0; i < MAX_HASH; ++i) {
        hash_to_index[i] = -1;
    }
}

int hash_func(unsigned int key) {
    return key % MAX_HASH;
}

int hash_insert(unsigned int key, int heap_index) {
    int h = hash_func(key);
    while (hash_to_index[h] != -1 && heap[hash_to_index[h]].hash != key) h = (h + 1) % MAX_HASH;
    hash_to_index[h] = heap_index;
    return 1;
}

int hash_find(unsigned int key) {
    int h = hash_func(key);
    while (hash_to_index[h] != -1) {
        int i = hash_to_index[h];
        if (heap[i].hash == key) return i;
        h = (h + 1) % MAX_HASH;
    }

    return -1;
}

int hash_delete(unsigned int key) {
    int h = hash_func(key);
    while (hash_to_index[h] != -1) {
        int i = hash_to_index[h];
        if (heap[i].hash == key) {
            hash_to_index[h] = -1;
            return 1;
        }

        h = (h + 1) % MAX_HASH;
    }

    return 0;
}

int swap(entry_info_t* a, entry_info_t* b) {
    entry_info_t tmp = *a;
    *a = *b;
    *b = tmp;
    return 1;
}

int heapify_up(int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap[parent].hash >= heap[i].hash) break;
        swap(&heap[parent], &heap[i]);
        hash_insert(heap[parent].hash, parent);
        hash_insert(heap[i].hash, i);
        i = parent;
    }

    return 1;
}

int heapify_down(int i) {
    while (1) {
        int largest = i;
        int left = 2 * i + 1;
        int right = 2 * i + 2;

        if (left < heap_size && heap[left].hash > heap[largest].hash) largest = left;
        if (right < heap_size && heap[right].hash > heap[largest].hash) largest = right;
        if (largest == i) break;

        swap(&heap[i], &heap[largest]);
        hash_insert(heap[i].hash, i);
        hash_insert(heap[largest].hash, largest);

        i = largest;
    }

    return 1;
}

int heap_insert(unsigned int hash, int index) {
    if (heap_size >= MAX_HEAP_SIZE) {
        printf("Heap overflow\n");
        return 1;
    }

    int i = heap_size++;
    heap[i].hash = hash;
    heap[i].index = index;
    hash_insert(hash, i);
    heapify_up(i);
    return 1;
}

int heap_remove(unsigned int hash) {
    int i = hash_find(hash);
    if (i == -1) return 0;

    hash_delete(hash);
    heap_size--;
    if (i == heap_size) return 1;

    heap[i] = heap[heap_size];
    hash_insert(heap[i].hash, i);
    heapify_down(i);
    heapify_up(i);
    return 1;
}

int heap_search(unsigned int hash, entry_info_t* out) {
    int i = hash_find(hash);
    if (i == -1) return 0;
    if (out) *out = heap[i];
    return 1;
}

int main() {
    init_hash_table();

    printf("Inserting data to heap...\n");
    for (int i = 1; i < 50; i++) heap_insert(((i * i - 123) / 9) * i, i);

    printf("Inserting target data to heap...\n");
    int expected_hash = 124923;
    heap_insert(expected_hash, 50);

    printf("Inserting data to heap...\n");
    for (int i = 51; i < 100; i++) heap_insert(((i * i - 123) / 9) * i, i);
    
    printf("Searching target data...\n");
    entry_info_t found;
    if (heap_search(expected_hash, &found)) {
        printf("Found hash=%u, index=%d\n", found.hash, found.index);
    }

    printf("Removing target data...\n");
    heap_remove(expected_hash);
    if (!heap_search(expected_hash, NULL)) {
        printf("hash=%i deleted\n", expected_hash);
    }

    return 0;
}

