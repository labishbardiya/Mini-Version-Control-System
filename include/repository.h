#ifndef REPOSITORY_H
#define REPOSITORY_H

#include "commit.h"

typedef struct Repository {
    Commit* head;   // Points to the latest commit
    int commitCount;
} Repository;

void initRepository(Repository* repo);
void addCommit(Repository* repo, const char* message, const char* hash);
void printLog(const Repository* repo);


#endif
