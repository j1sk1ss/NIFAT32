#include "../include/threading.h"

int THR_require_read(lock_t* lock) {
    if (!lock) return 0;
    int delay = REQUIRE_TIME;

    while (delay-- > 0) {
        lock_t old_val = *lock;
        if (LOCK_IS_WRITE(old_val)) {
            sched_yield();
            continue;
        }

        unsigned short readers = LOCK_GET_READERS(old_val);
        if (readers >= MAX_READERS) return 0;
        lock_t new_val = LOCK_PACK(readers + 1, UNLOCKED, 0);
        if (__sync_bool_compare_and_swap(lock, old_val, new_val))
            return 1;
    }

    return 0;
}

int THR_release_read(lock_t* lock) {
    if (!lock) return 0;
    while (1) {
        lock_t old_val = *lock;
        if (LOCK_IS_WRITE(old_val)) return 0;

        unsigned short readers = LOCK_GET_READERS(old_val);
        if (readers == 0) return 0;

        lock_t new_val = LOCK_PACK(readers - 1, UNLOCKED, 0);
        if (__sync_bool_compare_and_swap(lock, old_val, new_val))
            return 1;
    }
}

int THR_require_write(lock_t* lock, owner_t owner) {
    if (!lock) return 0;

    int delay = REQUIRE_TIME;
    while (delay-- > 0) {
        lock_t old_val = *lock;
        if (LOCK_GET_STATUS(old_val) == UNLOCKED && !LOCK_GET_READERS(old_val)) {
            lock_t new_val = LOCK_PACK(0, LOCKED_WRITE, owner);
            if (__sync_bool_compare_and_swap(lock, old_val, new_val))
                return 1;
        }

        sched_yield();
    }

    return 0;
}

int THR_release_write(lock_t* lock, owner_t owner) {
    if (!lock) return 0;
    while (1) {
        lock_t old_val = *lock;
        if (!LOCK_GET_STATUS(old_val)) return 0;
        if (LOCK_GET_OWNER(old_val) != owner) return 0;
        if (__sync_bool_compare_and_swap(lock, old_val, NULL_LOCK))
            return 1;
    }
}
