#ifndef COMMIT_H
#define COMMIT_H

#define HASH_SIZE 64
#define MESSAGE_SIZE 256

typedef struct Commit {
    int id; // A unique integer assigned to each commit. Acts like Git’s SHA-1 hash, but simpler.
    char message[MESSAGE_SIZE]; // Commit message
    char hash[HASH_SIZE]; // A fingerprint of the file contents, used to detect changes.
    char timestamp[30]; // When the commit was made.
    struct Commit* next;  // Linked list for commit history
} Commit;

#endif
