#include <stdio.h>
#include <string.h>
#include "../include/hash.h"

unsigned long computeHash(const char* filename) {
    // Placeholder hash: sum of ASCII characters
    unsigned long hash = 0;
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: cannot open %s\n", filename);
        return 0;
    }

    int c;
    while ((c = fgetc(file)) != EOF) {
        hash = (hash * 31 + c) % 1000000007; // simple polynomial hash
    }
    fclose(file);
    return hash;
}
