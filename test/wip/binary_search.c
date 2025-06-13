#include <stdio.h>

typedef struct {
    unsigned int hash;
    unsigned short l;
    unsigned short r;
    char is_end;
} __attribute__((packed)) entry_t;

/*
Hash larger -> lower index
*/
#define SPACE_SIZE 1024
static entry_t space[SPACE_SIZE];

#define INDEX_FILE ".index"
static int _dump_index() {
    FILE* fp = fopen(INDEX_FILE, "wb");
    if (fp) {
        fwrite(space, sizeof(entry_t), SPACE_SIZE, fp);
        fclose(fp);
    }

    return 1;
}

static int _load_index() {
    FILE* fp = fopen(INDEX_FILE, "rb");
    if (fp) {
        fread(space, sizeof(entry_t), SPACE_SIZE, fp);
        fclose(fp);
    }

    return 1;
}

static int _search(unsigned int hash, int index) {
    if (index + 2 > SPACE_SIZE) return -1;
    if (space[index].hash == hash) return index;
    if (hash < space[index].hash) return _search(hash, space[index].l);
    else return _search(hash, space[index].r);
    return -1;
}

#define END 0xFFFF
static int _insert(unsigned int hash, int index) {
    if (index >= SPACE_SIZE) return 0;
    if (space[index].is_end) {
        space[index].hash   = hash;
        space[index].is_end = 0;
        space[index].l = END;
        space[index].r = END;
        return 1;
    }

    if (hash < space[index].hash) {
        if (space[index].l == END) {
            for (int i = 0; i < SPACE_SIZE; i++) {
                if (space[i].is_end) {
                    space[index].l = i;
                    return _insert(hash, i);
                }
            }

            return 0;
        } 
        else {
            return _insert(hash, space[index].l);
        }
    } 
    else {
        if (space[index].r == END) {
            for (int i = 0; i < SPACE_SIZE; i++) {
                if (space[i].is_end) {
                    space[index].r = i;
                    return _insert(hash, i);
                }
            }

            return 0;
        } 
        else {
            return _insert(hash, space[index].r);
        }
    }
}

static int _index(entry_t* entries, int count) {
    for (int i = 0; i < count; i++) _insert(entries[i].hash, 0);
    return 1;
}

static int _remove(unsigned int hash) {
    int index = _search(hash, 0);
    if (index >= 0) space[index].is_end = 1;
    else return 0;
    return 1;
}

static int _cleanup() {
    for (int i = 0; i < SPACE_SIZE; i++) space[i].is_end = 1;
    return 1;
}

int main() {
    _cleanup();
    printf("Forming data...\n");
    for (int i = 0; i < SPACE_SIZE / 2; i++) {
        _insert(i * 123, 0);
    }

    printf("Trying to find data...\n");
    #define TARGET 5166
    printf("Data: %i\n", _search(TARGET, 0));
    _dump_index();
    return 1;
}