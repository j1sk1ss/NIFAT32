#include <std/mm.h>

static unsigned char _buffer[ALLOC_BUFFER_SIZE] = { 0 };
static mm_block_t* _mm_head = (mm_block_t*)_buffer;
static int _allocated = 0;

int mm_init() {
    if (_mm_head->magic != MM_BLOCK_MAGIC) {
        print_log("Memory manager init!");
        _mm_head->magic = MM_BLOCK_MAGIC;
        _mm_head->size  = ALLOC_BUFFER_SIZE - sizeof(mm_block_t);
        _mm_head->free  = 1;
        _mm_head->next  = NULL;
    }
    
    return 1;
}

static int __coalesce_memory() {
    int merged = 0;
    mm_block_t* current = _mm_head;
    
    do {
        merged = 0;
        current = _mm_head;

        while (current && current->magic == MM_BLOCK_MAGIC && current->next) {
            if (current->free && current->next->free) {
                current->size += sizeof(mm_block_t) + current->next->size;
                current->next = current->next->next;
                merged = 1;
            } 
            else {
                current = current->next;
            }
        }
    } while (merged);
    return 1;
}

lock_t _malloc_lock = NULL_LOCK;

static void* __malloc_s(unsigned int size, unsigned int offset, int prepare_mem) {
    if (!size) return NULL;
    if (prepare_mem) __coalesce_memory();

    size   = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
    offset = (offset + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
    mm_block_t* current = _mm_head;
    
    if (THR_require_write(&_malloc_lock, get_thread_num())) {
        while (current && current->magic == MM_BLOCK_MAGIC) {
            void* aligned_addr = (unsigned char*)current + sizeof(mm_block_t);
            unsigned int position = (unsigned int)((unsigned char*)aligned_addr - _buffer);
            if (current->free && current->size >= size && position >= offset) {
                if (current->size >= size + sizeof(mm_block_t)) {
                    mm_block_t* new_block = (mm_block_t*)((unsigned char*)current + sizeof(mm_block_t) + size);
                    new_block->magic = MM_BLOCK_MAGIC;
                    new_block->size = current->size - size - sizeof(mm_block_t);
                    new_block->free = 1;
                    new_block->next = current->next;

                    current->next = new_block;
                    current->size = size;
                }

                current->free = 0;
                _allocated += current->size + sizeof(mm_block_t);

                THR_release_write(&_malloc_lock, get_thread_num());
                print_mm("Allocated node [%p] with [%i] size / [%i]", (unsigned char*)current + sizeof(mm_block_t), current->size, _allocated);
                return (unsigned char*)current + sizeof(mm_block_t);
            }

            current = current->next;
        }
    }
    else {
        print_error("Can't lock _malloc_lock!");
        return NULL;
    }

    THR_release_write(&_malloc_lock, get_thread_num());
    return prepare_mem ? NULL : __malloc_s(size, offset, 1);
}

void* malloc_s(unsigned int size) {
    void* ptr = __malloc_s(size, NO_OFFSET, 0);
    if (!ptr) { print_mm("Allocation error! I can't allocate [%i]!", size); }
    return ptr;
}

void* malloc_off_s(unsigned int size, unsigned int offset) {
    void* ptr = __malloc_s(size, offset, 0);
    if (!ptr) { print_mm("Allocation error! I can't allocate [%i]!", size); }
    return ptr;
}

void* realloc_s(void* ptr, unsigned int elem) {
    void* mem = NULL;
    if (elem) {
        if (!ptr) return malloc_s(elem);
        mem = malloc_s(elem);
        if (mem) {
            str_memcpy(mem, ptr, elem);
            free_s(ptr);
        }
    }

    return mem;
}

int free_s(void* ptr) {
    if (!ptr || ptr < (void*)_buffer || ptr >= (void*)(_buffer + ALLOC_BUFFER_SIZE)) {
        print_mm("ptr=%p is not valid!", ptr);
        return 0;
    }
    
    mm_block_t* block = (mm_block_t*)((unsigned char*)ptr - sizeof(mm_block_t));
    if (block->magic != MM_BLOCK_MAGIC) {
        print_mm("ptr=%p point to block with incorrect magic=%u!", ptr, block->magic);
        return 0;
    }

    if (block->free) {
        print_mm("ptr=%p point to already freed block!", ptr);
        return 0;
    }

    block->free = 1;
    _allocated -= block->size + sizeof(mm_block_t);
    print_mm("Free [%p] with [%i] size / [%i]", ptr, block->size, _allocated);
    
    return 1;
}