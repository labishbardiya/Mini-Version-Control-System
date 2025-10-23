#include <stdio.h>
#include "../include/repository.h"
#include "../include/hash.h"

int main() {
    Repository repo;
    initRepository(&repo);

    // Simulate commits
    addCommit(&repo, "Initial commit", "hash123");
    addCommit(&repo, "Added file tracking", "hash456");
    addCommit(&repo, "Optimized log function", "hash789");

    printLog(&repo);

    return 0;
}
