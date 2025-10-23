#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/repository.h"

void initRepository(Repository* repo) {
    repo->head = NULL;
    repo->commitCount = 0;
    printf("Repository initialized successfully.\n");
}

void addCommit(Repository* repo, const char* message, const char* hash) {
    Commit* newCommit = (Commit*) malloc(sizeof(Commit));
    if (!newCommit) {
        printf("Memory allocation failed.\n");
        return;
    }

    newCommit->id = ++repo->commitCount;
    strncpy(newCommit->message, message, MESSAGE_SIZE);
    strncpy(newCommit->hash, hash, HASH_SIZE);

    // Get current timestamp
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(newCommit->timestamp, sizeof(newCommit->timestamp), "%Y-%m-%d %H:%M:%S", t);

    newCommit->next = repo->head;
    repo->head = newCommit;

    printf("Commit %d added successfully.\n", newCommit->id);
}

void printLog(const Repository* repo) {
    if (!repo->head) {
        printf("No commits found.\n");
        return;
    }

    Commit* temp = repo->head;
    printf("\nCommit History:\n");
    printf("===============================\n");
    while (temp) {
        printf("Commit ID: %d\n", temp->id);
        printf("Message  : %s\n", temp->message);
        printf("Hash     : %s\n", temp->hash);
        printf("Timestamp: %s\n", temp->timestamp);
        printf("-------------------------------\n");
        temp = temp->next;
    }
}
