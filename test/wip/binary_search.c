#include <stdio.h>

typedef struct {
    unsigned int hash;
    unsigned short l;
    unsigned short r;
    char is_end;
    unsigned char data[28];
} entry_t;

/*
Hash larger -> lower index
*/
#define SPACE_SIZE 1024
static entry_t space[SPACE_SIZE];

static int _search(unsigned int hash, int index) {
    if (index + 2 > SPACE_SIZE) return -1;
    if (space[index].hash == hash) return index;
    if (hash < space[index].hash) return _search(hash, index + 1);
    else return _search(hash, index + 2);
    return -1;
}

static int _insert(unsigned int hash, int index, unsigned char data) {
    if (index >= SPACE_SIZE) return 0;
    if (space[index].is_end) {
        space[index].hash = hash;
        space[index].is_end = 0;
        space[index].l = 0xFFFF;
        space[index].r = 0xFFFF;
        space[index].data[0] = data;
        return 1;
    }

    if (hash < space[index].hash) {
        if (space[index].l == 0xFFFF) {
            for (int i = 0; i < SPACE_SIZE; i++) {
                if (space[i].is_end) {
                    space[index].l = i;
                    return _insert(hash, i, data);
                }
            }

            return 0;
        } 
        else {
            return _insert(hash, space[index].l, data);
        }
    } else {
        if (space[index].r == 0xFFFF) {
            for (int i = 0; i < SPACE_SIZE; i++) {
                if (space[i].is_end) {
                    space[index].r = i;
                    return _insert(hash, i, data);
                }
            }

            return 0;
        } 
        else {
            return _insert(hash, space[index].r, data);
        }
    }
}

static int _remove(unsigned int hash) {
    int index = _search(hash, 0);
    if (index >= 0) space[index].is_end = 1;
    else return 0;
    return 1;
}

int main() {
    for (int i = 0; i < SPACE_SIZE; i++) space[i].is_end = 1;

    printf("Forming data...\n");
    for (int i = 0; i < SPACE_SIZE / 2; i++) {
        if (i != 42) _insert(i * 123, 0, i * 2);
        else _insert(i * 123, 0, 0xFF);
    }

    printf("Trying to find data...\n");
    #define TARGET 5166
    printf("Data: %i\n", space[_search(TARGET, 0)].data[0]);
    return 1;
}