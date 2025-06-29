#include <stdio.h>
#include <stdlib.h>

#define TOTAL_SECTORS 170000
#define HASH_A1 2654435761U
#define HASH_A2 2246822519U

#define GET_HASH1(x) (((x + 11) * HASH_A1) % TOTAL_SECTORS)
#define GET_HASH2(x) (((x + 957) * HASH_A2) % TOTAL_SECTORS)

int main() {
    int h1[11], h2[11];

    printf("x | Hash1 | Hash2 | |Hash1 - Hash2|\n");
    printf("--+--------+--------+----------------\n");

    for (int x = 0; x <= 10; x++) {
        h1[x] = GET_HASH1(x);
        h2[x] = GET_HASH2(x);
        printf("%2d | %6d | %6d | %6d\n", x, h1[x], h2[x], abs(h1[x] - h2[x]));
    }

    for (int i = 0; i <= 10; i++) {
        for (int j = i+1; j <= 10; j++) {
            if (h1[i] == h1[j]) printf("Collision in HASH1 at x=%d and x=%d\n", i, j);
            if (h2[i] == h2[j]) printf("Collision in HASH2 at x=%d and x=%d\n", i, j);
            if (h1[i] == h2[j] && h2[i] == h1[j]) printf("Cross-collision at x=%d and x=%d\n", i, j);
        }
        if (h1[i] == h2[i]) printf("Direct collision at x=%d\n", i);
    }

    return 0;
}
