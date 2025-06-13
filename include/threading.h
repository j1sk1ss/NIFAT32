/*
 *  License:
 *  Copyright (C) 2024 Nikolaj Fot
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of 
 *  the GNU General Public License as published by the Free Software Foundation, version 3.
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along with this program. 
 *  If not, see https://www.gnu.org/licenses/.
 * 
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */

#ifndef THREADING_H_
#define THREADING_H_

#define LOCKED       0xAB
#define UNLOCKED     0x00
#define NO_OWNER     0xFF
#define LOCKED_WRITE 1

#define REQUIRE_TIME 99999
#define MAX_READERS  255

#define OWNER_MASK        0x3FFF
#define STATUS_MASK       0x0001
#define LOCK_READERS_MASK 0x000000FF
#define LOCK_WRITE_FLAG   0x00000100
#define LOCK_OWNER_MASK   0xFFFF0000

#define LOCK_IS_WRITE(lock)    (((lock) & STATUS_MASK) == LOCKED_WRITE)
#define LOCK_GET_STATUS(lock)  (((lock) >> 8) & 0x1)
#define LOCK_GET_READERS(lock) (((lock) >> 0) & 0xFF)
#define LOCK_GET_OWNER(lock)   (((lock) & LOCK_OWNER_MASK) >> 16)

#define LOCK_PACK(readers, is_write, owner) \
    (((readers) & 0xFF) | ((is_write ? 1 : 0) << 8) | ((owner & 0xFFFF) << 16))

#define NULL_LOCK LOCK_PACK(0, 0, NO_OWNER)

/*
This function is platform-specific. If NIFAT32 planned to work in thread context,
re-define this function for getting thread uniq id.
*/
#define get_thread_num() 0
#define sched_yield()

typedef volatile unsigned int lock_t;
typedef unsigned short owner_t;

int THR_require_read(lock_t* lock);
int THR_release_read(lock_t* lock);
int THR_require_write(lock_t* lock, owner_t owner);
int THR_release_write(lock_t* lock, owner_t owner);

#endif