#ifndef REPOSITORY_H
#define REPOSITORY_H

#include "commit.h"

typedef struct Repository {
    Commit* head;   // Points to the latest commit
    int commitCount;
} Repository;

#endif
