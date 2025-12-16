// cs.c - Version control system core CLI
// All core logic implemented in C, using only standard and POSIX libraries.
// Focus: clarity and deterministic behavior, not performance.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define CS_DIR ".cs"
#define CS_OBJECTS_DIR ".cs/objects"
#define CS_BLOBS_DIR ".cs/objects/blobs"
#define CS_COMMITS_DIR ".cs/objects/commits"
#define CS_INDEX_PATH ".cs/index"
#define CS_HEAD_PATH ".cs/HEAD"
#define CS_CONFIG_PATH ".cs/config"

#define MAX_PATH_LEN 4096
#define MAX_HASH_LEN 65   /* 64 hex chars + null */
#define MAX_LINE_LEN 4096
#define MAX_MSG_LEN 1024

typedef struct {
    char path[MAX_PATH_LEN];
    char hash[MAX_HASH_LEN];
} FileEntry;

typedef struct {
    FileEntry *items;
    size_t len;
    size_t cap;
} FileMap;

typedef struct {
    char id[MAX_HASH_LEN];
    char parent[MAX_HASH_LEN];
    long timestamp;
    char message[MAX_MSG_LEN];
    FileMap files;
} Commit;

static void print_usage(void) {
    printf("cs - Version control system\n");
    printf("Usage:\n");
    printf("  ./cs init\n");
    printf("  ./cs add <path>|.\n");
    printf("  ./cs commit -m \"message\"\n");
    printf("  ./cs log\n");
    printf("  ./cs revert <commit_id|HEAD>\n");
    printf("  ./cs trace <filename>\n");
    printf("  ./cs integrity\n");
    printf("  ./cs timewarp <timestamp>\n");
}

// ------------------------------
// Utility helpers
// ------------------------------

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

static int is_regular_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

static int ensure_dir(const char *path) {
    if (path_exists(path)) {
        if (!is_directory(path)) {
            fprintf(stderr, "cs: %s exists and is not a directory\n", path);
            return -1;
        }
        return 0;
    }
    if (mkdir(path, 0700) != 0) {
        perror("mkdir");
        return -1;
    }
    return 0;
}

static int ensure_parent_dirs(const char *path) {
    char tmp[MAX_PATH_LEN];
    size_t i;

    snprintf(tmp, sizeof(tmp), "%s", path);
    for (i = 1; tmp[i] != '\0'; ++i) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (tmp[0] != '\0' && ensure_dir(tmp) != 0) return -1;
            tmp[i] = '/';
        }
    }
    return 0;
}

static int write_text_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("fopen");
        return -1;
    }
    if (fputs(content, f) == EOF) {
        perror("fputs");
        fclose(f);
        return -1;
    }
    if (fclose(f) != 0) {
        perror("fclose");
        return -1;
    }
    return 0;
}

// Read whole text file into memory (null terminated).
// Caller must free *out_buf.
static int read_text_file(const char *path, char **out_buf) {
    FILE *f = fopen(path, "r");
    long len;
    size_t read_len;

    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    len = ftell(f);
    if (len < 0) {
        fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    *out_buf = (char *)malloc((size_t)len + 1);
    if (!*out_buf) {
        fclose(f);
        return -1;
    }
    read_len = fread(*out_buf, 1, (size_t)len, f);
    (*out_buf)[read_len] = '\0';
    fclose(f);
    return 0;
}

static int write_binary_file(const char *path, const char *buf, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror("fopen");
        return -1;
    }
    if (fwrite(buf, 1, len, f) != len) {
        perror("fwrite");
        fclose(f);
        return -1;
    }
    if (fclose(f) != 0) {
        perror("fclose");
        return -1;
    }
    return 0;
}

// ------------------------------
// Hashing (simple FNV-1a 64-bit in hex)
// ------------------------------

static void hash_bytes(const unsigned char *data, size_t len, char out_hash[MAX_HASH_LEN]) {
    const unsigned long long FNV_OFFSET = 1469598103934665603ULL;
    const unsigned long long FNV_PRIME = 1099511628211ULL;
    unsigned long long h = FNV_OFFSET;
    size_t i;

    for (i = 0; i < len; ++i) {
        h ^= (unsigned long long)data[i];
        h *= FNV_PRIME;
    }

    // print 64-bit value as 16 hex chars, but we use 16*4 = 64 bits -> 16 hex digits.
    // To keep hashes visually longer and clearly separated, we repeat the value 4 times.
    // This is still deterministic and simple.
    snprintf(out_hash, MAX_HASH_LEN, "%016llx%016llx%016llx%016llx",
             (unsigned long long)h,
             (unsigned long long)h,
             (unsigned long long)h,
             (unsigned long long)h);
}

// Convenience to hash a C string (excluding the final null).
static void hash_string(const char *s, char out_hash[MAX_HASH_LEN]) {
    hash_bytes((const unsigned char *)s, strlen(s), out_hash);
}

// ------------------------------
// FileMap helpers
// ------------------------------

static void filemap_init(FileMap *m) {
    m->items = NULL;
    m->len = 0;
    m->cap = 0;
}

static void filemap_free(FileMap *m) {
    free(m->items);
    m->items = NULL;
    m->len = 0;
    m->cap = 0;
}

static FileEntry *filemap_find(FileMap *m, const char *path) {
    size_t i;
    for (i = 0; i < m->len; ++i) {
        if (strcmp(m->items[i].path, path) == 0) {
            return &m->items[i];
        }
    }
    return NULL;
}

static int filemap_reserve(FileMap *m, size_t needed) {
    if (needed <= m->cap) return 0;
    size_t new_cap = m->cap == 0 ? 8 : m->cap * 2;
    while (new_cap < needed) new_cap *= 2;
    FileEntry *n = realloc(m->items, new_cap * sizeof(FileEntry));
    if (!n) return -1;
    m->items = n;
    m->cap = new_cap;
    return 0;
}

static int filemap_set(FileMap *m, const char *path, const char *hash) {
    FileEntry *e = filemap_find(m, path);
    if (e) {
        snprintf(e->hash, sizeof(e->hash), "%s", hash);
        return 0;
    }
    if (filemap_reserve(m, m->len + 1) != 0) return -1;
    snprintf(m->items[m->len].path, sizeof(m->items[m->len].path), "%s", path);
    snprintf(m->items[m->len].hash, sizeof(m->items[m->len].hash), "%s", hash);
    m->len += 1;
    return 0;
}

static int fileentry_cmp(const void *a, const void *b) {
    const FileEntry *ea = (const FileEntry *)a;
    const FileEntry *eb = (const FileEntry *)b;
    return strcmp(ea->path, eb->path);
}

static void filemap_sort(FileMap *m) {
    qsort(m->items, m->len, sizeof(FileEntry), fileentry_cmp);
}

// ------------------------------
// Index helpers
// ------------------------------

static int load_index(FileMap *m) {
    FILE *f = fopen(CS_INDEX_PATH, "r");
    char line[MAX_LINE_LEN];
    filemap_init(m);

    if (!f) {
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        char path[MAX_PATH_LEN];
        char hash[MAX_HASH_LEN];
        if (sscanf(line, "%4095s %64s", path, hash) == 2) {
            if (filemap_set(m, path, hash) != 0) {
                fclose(f);
                return -1;
            }
        }
    }
    fclose(f);
    return 0;
}

static int save_index(FileMap *m) {
    FILE *f = fopen(CS_INDEX_PATH, "w");
    size_t i;
    if (!f) {
        perror("fopen");
        return -1;
    }
    filemap_sort(m);
    for (i = 0; i < m->len; ++i) {
        fprintf(f, "%s %s\n", m->items[i].path, m->items[i].hash);
    }
    if (fclose(f) != 0) {
        perror("fclose");
        return -1;
    }
    return 0;
}

static int clear_index(void) {
    return write_text_file(CS_INDEX_PATH, "");
}

// ------------------------------
// Repository primitives
// ------------------------------

static int repo_exists(void) {
    return path_exists(CS_DIR) && is_directory(CS_DIR);
}

static int cmd_init(void) {
    if (repo_exists()) {
        fprintf(stderr, "cs: repository already exists\n");
        return 1;
    }

    if (ensure_dir(CS_DIR) != 0) return 1;
    if (ensure_dir(CS_OBJECTS_DIR) != 0) return 1;
    if (ensure_dir(CS_BLOBS_DIR) != 0) return 1;
    if (ensure_dir(CS_COMMITS_DIR) != 0) return 1;

    if (write_text_file(CS_INDEX_PATH, "") != 0) return 1;
    if (write_text_file(CS_HEAD_PATH, "") != 0) return 1;
    if (write_text_file(CS_CONFIG_PATH, "# cs config\n") != 0) return 1;

    printf("Initialized empty cs repository in ./%s\n", CS_DIR);
    return 0;
}

// ------------------------------
// Blob helpers
// ------------------------------

static int read_file_content(const char *path, char **out_buf, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    size_t read_len;
    long len;
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    len = ftell(f);
    if (len < 0) {
        fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    *out_buf = (char *)malloc((size_t)len);
    if (!*out_buf) {
        fclose(f);
        return -1;
    }
    read_len = fread(*out_buf, 1, (size_t)len, f);
    *out_len = read_len;
    fclose(f);
    return 0;
}

static int store_blob(const char *path, char out_hash[MAX_HASH_LEN]) {
    char *buf = NULL;
    size_t len = 0;
    char blob_path[MAX_PATH_LEN];
    if (read_file_content(path, &buf, &len) != 0) {
        perror("read");
        return -1;
    }
    hash_bytes((const unsigned char *)buf, len, out_hash);
    snprintf(blob_path, sizeof(blob_path), "%s/%s", CS_BLOBS_DIR, out_hash);
    if (!path_exists(blob_path)) {
        if (write_binary_file(blob_path, buf, len) != 0) {
            free(buf);
            return -1;
        }
    }
    free(buf);
    return 0;
}

static int read_blob(const char *hash, char **out_buf, size_t *out_len) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", CS_BLOBS_DIR, hash);
    return read_file_content(path, out_buf, out_len);
}

// ------------------------------
// Commit helpers
// ------------------------------

static int load_commit(const char *commit_id, Commit *out) {
    char path[MAX_PATH_LEN];
    FILE *f;
    char line[MAX_LINE_LEN];
    filemap_init(&out->files);

    snprintf(path, sizeof(path), "%s/%s", CS_COMMITS_DIR, commit_id);
    f = fopen(path, "r");
    if (!f) return -1;

    snprintf(out->id, sizeof(out->id), "%s", commit_id);
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    if (sscanf(line, "parent %64s", out->parent) != 1) { fclose(f); return -1; }
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    if (sscanf(line, "timestamp %ld", &out->timestamp) != 1) { fclose(f); return -1; }
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    if (strncmp(line, "message ", 8) != 0) { fclose(f); return -1; }
    snprintf(out->message, sizeof(out->message), "%s", line + 8);
    size_t len = strlen(out->message);
    if (len > 0 && out->message[len - 1] == '\n') out->message[len - 1] = '\0';

    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    if (strncmp(line, "files", 5) != 0) { fclose(f); return -1; }

    while (fgets(line, sizeof(line), f)) {
        char pathbuf[MAX_PATH_LEN];
        char hashbuf[MAX_HASH_LEN];
        if (sscanf(line, "%4095s %64s", pathbuf, hashbuf) == 2) {
            if (filemap_set(&out->files, pathbuf, hashbuf) != 0) {
                fclose(f);
                return -1;
            }
        }
    }

    fclose(f);
    return 0;
}

static int save_commit(Commit *c) {
    char path[MAX_PATH_LEN];
    FILE *f;
    size_t i;

    filemap_sort(&c->files);
    snprintf(path, sizeof(path), "%s/%s", CS_COMMITS_DIR, c->id);
    f = fopen(path, "w");
    if (!f) {
        perror("fopen");
        return -1;
    }
    fprintf(f, "parent %s\n", c->parent[0] ? c->parent : "null");
    fprintf(f, "timestamp %ld\n", c->timestamp);
    fprintf(f, "message %s\n", c->message);
    fprintf(f, "files\n");
    for (i = 0; i < c->files.len; ++i) {
        fprintf(f, "%s %s\n", c->files.items[i].path, c->files.items[i].hash);
    }
    if (fclose(f) != 0) {
        perror("fclose");
        return -1;
    }
    return 0;
}

static int get_head(char out[MAX_HASH_LEN]) {
    char *buf = NULL;
    if (read_text_file(CS_HEAD_PATH, &buf) != 0) return -1;
    while (buf && (*buf == '\n' || *buf == ' ' || *buf == '\t')) buf++;
    if (buf && *buf) {
        snprintf(out, MAX_HASH_LEN, "%s", buf);
        size_t len = strlen(out);
        if (len > 0 && out[len - 1] == '\n') out[len - 1] = '\0';
    } else {
        out[0] = '\0';
    }
    free(buf);
    return 0;
}

static int set_head(const char *commit_id) {
    return write_text_file(CS_HEAD_PATH, commit_id);
}

static int load_head_commit(Commit *c) {
    char head[MAX_HASH_LEN];
    if (get_head(head) != 0 || head[0] == '\0') return -1;
    return load_commit(head, c);
}

static int build_commit_id(const char *parent, long ts, const char *message, FileMap *files, char out_hash[MAX_HASH_LEN]) {
    size_t i;
    char *buffer = NULL;
    size_t buf_size = 0;
    filemap_sort(files);

    // Estimate buffer
    buf_size = strlen(parent ? parent : "null") + 32 + strlen(message) + files->len * (MAX_PATH_LEN + MAX_HASH_LEN + 2) + 64;
    buffer = (char *)malloc(buf_size);
    if (!buffer) return -1;

    snprintf(buffer, buf_size, "parent:%s\ntimestamp:%ld\nmessage:%s\n", parent ? parent : "null", ts, message);
    for (i = 0; i < files->len; ++i) {
        strcat(buffer, files->items[i].path);
        strcat(buffer, " ");
        strcat(buffer, files->items[i].hash);
        strcat(buffer, "\n");
    }

    hash_string(buffer, out_hash);
    free(buffer);
    return 0;
}

// ------------------------------
// Diff helpers for analytics
// ------------------------------

static void split_lines(const char *buf, size_t len, char ***out_lines, size_t *out_count) {
    size_t count = 0;
    size_t i;
    for (i = 0; i < len; ++i) {
        if (buf[i] == '\n') count++;
    }
    count++; // last line
    *out_lines = (char **)malloc(sizeof(char *) * count);
    *out_count = 0;

    size_t start = 0;
    size_t idx = 0;
    for (i = 0; i < len; ++i) {
        if (buf[i] == '\n') {
            size_t line_len = i - start;
            char *line = (char *)malloc(line_len + 1);
            memcpy(line, buf + start, line_len);
            line[line_len] = '\0';
            (*out_lines)[idx++] = line;
            start = i + 1;
        }
    }
    // last line
    if (start <= len) {
        size_t line_len = len - start;
        char *line = (char *)malloc(line_len + 1);
        memcpy(line, buf + start, line_len);
        line[line_len] = '\0';
        (*out_lines)[idx++] = line;
    }
    *out_count = idx;
}

static void free_lines(char **lines, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) free(lines[i]);
    free(lines);
}

// Simple line diff: compare line by line; differences count as one added and one removed.
static void diff_counts(const char *old_buf, size_t old_len, const char *new_buf, size_t new_len, int *out_added, int *out_removed) {
    char **old_lines = NULL, **new_lines = NULL;
    size_t old_c = 0, new_c = 0;
    size_t i, min_c;
    *out_added = 0;
    *out_removed = 0;

    split_lines(old_buf ? old_buf : "", old_buf ? old_len : 0, &old_lines, &old_c);
    split_lines(new_buf ? new_buf : "", new_buf ? new_len : 0, &new_lines, &new_c);

    min_c = old_c < new_c ? old_c : new_c;
    for (i = 0; i < min_c; ++i) {
        if (strcmp(old_lines[i], new_lines[i]) != 0) {
            (*out_added)++;
            (*out_removed)++;
        }
    }
    if (new_c > old_c) {
        *out_added += (int)(new_c - old_c);
    } else if (old_c > new_c) {
        *out_removed += (int)(old_c - new_c);
    }

    free_lines(old_lines, old_c);
    free_lines(new_lines, new_c);
}

// ------------------------------
// add command
// ------------------------------

static int stage_file(const char *path, FileMap *index) {
    char hash[MAX_HASH_LEN];
    if (!is_regular_file(path)) {
        fprintf(stderr, "cs add: %s is not a regular file\n", path);
        return -1;
    }
    if (store_blob(path, hash) != 0) return -1;
    if (filemap_set(index, path, hash) != 0) return -1;
    return 0;
}

static int stage_dir_recursive(const char *dir, FileMap *index) {
    DIR *d;
    struct dirent *ent;
    d = opendir(dir);
    if (!d) {
        perror("opendir");
        return -1;
    }
    while ((ent = readdir(d)) != NULL) {
        char child[MAX_PATH_LEN];
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (strcmp(ent->d_name, CS_DIR) == 0) continue;

        snprintf(child, sizeof(child), "%s/%s", dir, ent->d_name);
        if (is_directory(child)) {
            if (stage_dir_recursive(child, index) != 0) {
                closedir(d);
                return -1;
            }
        } else if (is_regular_file(child)) {
            if (stage_file(child, index) != 0) {
                closedir(d);
                return -1;
            }
        }
    }
    closedir(d);
    return 0;
}

static int cmd_add(int argc, char **argv) {
    FileMap index;
    int rc = 0;
    if (argc < 3) {
        fprintf(stderr, "cs add: missing path\n");
        return 1;
    }
    if (!repo_exists()) {
        fprintf(stderr, "cs: no repository found. Run ./cs init\n");
        return 1;
    }
    if (load_index(&index) != 0) filemap_init(&index);

    const char *target = argv[2];
    if (strcmp(target, ".") == 0) {
        rc = stage_dir_recursive(".", &index);
    } else {
        if (path_exists(target) && is_directory(target)) {
            rc = stage_dir_recursive(target, &index);
        } else {
            rc = stage_file(target, &index);
        }
    }

    if (rc == 0) {
        save_index(&index);
        printf("staged\n");
    }
    filemap_free(&index);
    return rc == 0 ? 0 : 1;
}

// ------------------------------
// commit command
// ------------------------------

static int cmd_commit(int argc, char **argv) {
    FileMap index;
    FileMap final_map;
    Commit parent;
    int has_parent = 0;
    char head_id[MAX_HASH_LEN];
    char commit_id[MAX_HASH_LEN];
    const char *msg = NULL;
    int i;
    int files_changed = 0;
    int lines_added_total = 0;
    int lines_removed_total = 0;
    size_t commits_count = 0;

    if (argc < 4) {
        fprintf(stderr, "cs commit: usage ./cs commit -m \"message\"\n");
        return 1;
    }
    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            msg = argv[i + 1];
            break;
        }
    }
    if (!msg || msg[0] == '\0') {
        fprintf(stderr, "cs commit: commit message required\n");
        return 1;
    }
    if (!repo_exists()) {
        fprintf(stderr, "cs: no repository found. Run ./cs init\n");
        return 1;
    }
    if (load_index(&index) != 0) {
        fprintf(stderr, "cs commit: index missing\n");
        return 1;
    }
    if (index.len == 0) {
        fprintf(stderr, "cs commit: index is empty\n");
        filemap_free(&index);
        return 1;
    }

    // Load parent commit map if exists.
    if (get_head(head_id) == 0 && head_id[0] != '\0') {
        if (load_commit(head_id, &parent) == 0) {
            has_parent = 1;
        } else {
            filemap_free(&index);
            fprintf(stderr, "cs commit: failed to load HEAD commit\n");
            return 1;
        }
    }

    filemap_init(&final_map);
    // start from parent map
    if (has_parent) {
        size_t j;
        for (j = 0; j < parent.files.len; ++j) {
            filemap_set(&final_map, parent.files.items[j].path, parent.files.items[j].hash);
        }
    }
    // apply index
    for (i = 0; i < (int)index.len; ++i) {
        filemap_set(&final_map, index.items[i].path, index.items[i].hash);
    }

    // Build commit id
    long ts = time(NULL);
    char parent_id[MAX_HASH_LEN];
    parent_id[0] = '\0';
    if (has_parent) snprintf(parent_id, sizeof(parent_id), "%s", parent.id);
    if (build_commit_id(has_parent ? parent_id : "null", ts, msg, &final_map, commit_id) != 0) {
        filemap_free(&final_map);
        filemap_free(&index);
        if (has_parent) filemap_free(&parent.files);
        fprintf(stderr, "cs commit: failed to build commit id\n");
        return 1;
    }

    Commit c;
    snprintf(c.id, sizeof(c.id), "%s", commit_id);
    if (has_parent) snprintf(c.parent, sizeof(c.parent), "%s", parent.id); else c.parent[0] = '\0';
    c.timestamp = ts;
    snprintf(c.message, sizeof(c.message), "%s", msg);
    c.files = final_map;

    if (save_commit(&c) != 0) {
        filemap_free(&c.files);
        filemap_free(&index);
        if (has_parent) filemap_free(&parent.files);
        return 1;
    }
    set_head(commit_id);
    clear_index();

    // Analytics: files changed, lines added/removed, total commits
    for (i = 0; i < (int)c.files.len; ++i) {
        const char *path = c.files.items[i].path;
        const char *new_hash = c.files.items[i].hash;
        const char *old_hash = NULL;
        char *old_buf = NULL, *new_buf = NULL;
        size_t old_len = 0, new_len = 0;
        int added = 0, removed = 0;

        if (has_parent) {
            FileEntry *pe = filemap_find(&parent.files, path);
            if (pe) old_hash = pe->hash;
        }
        if (old_hash && strcmp(old_hash, new_hash) == 0) continue;

        files_changed++;
        if (old_hash) read_blob(old_hash, &old_buf, &old_len);
        read_blob(new_hash, &new_buf, &new_len);
        diff_counts(old_buf, old_len, new_buf, new_len, &added, &removed);
        lines_added_total += added;
        lines_removed_total += removed;
        free(old_buf);
        free(new_buf);
    }

    // count commits
    DIR *d = opendir(CS_COMMITS_DIR);
    struct dirent *ent;
    if (d) {
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            commits_count++;
        }
        closedir(d);
    }

    printf("Commit %s\n", commit_id);
    printf("Files changed: %d\n", files_changed);
    printf("Lines added: %d\n", lines_added_total);
    printf("Lines removed: %d\n", lines_removed_total);
    printf("Total commits: %zu\n", commits_count);

    filemap_free(&c.files);
    filemap_free(&index);
    if (has_parent) filemap_free(&parent.files);
    return 0;
}

// ------------------------------
// log command
// ------------------------------

static int cmd_log(void) {
    char head[MAX_HASH_LEN];
    Commit c;
    if (!repo_exists()) {
        fprintf(stderr, "cs: no repository found. Run ./cs init\n");
        return 1;
    }
    if (get_head(head) != 0 || head[0] == '\0') {
        fprintf(stderr, "cs log: no commits\n");
        return 1;
    }
    if (load_commit(head, &c) != 0) {
        fprintf(stderr, "cs log: failed to load HEAD\n");
        return 1;
    }
    while (1) {
        printf("commit %s\n", c.id);
        printf("timestamp %ld\n", c.timestamp);
        printf("message %s\n", c.message);
        if (c.parent[0] == '\0' || strcmp(c.parent, "null") == 0) {
            filemap_free(&c.files);
            break;
        }
        Commit parent;
        if (load_commit(c.parent, &parent) != 0) {
            filemap_free(&c.files);
            fprintf(stderr, "cs log: broken parent reference %s\n", c.parent);
            return 1;
        }
        filemap_free(&c.files);
        c = parent;
    }
    return 0;
}

// ------------------------------
// revert command
// ------------------------------

static int restore_commit_files(FileMap *target, FileMap *current) {
    size_t i;
    // remove files not in target but present in current
    if (current) {
        for (i = 0; i < current->len; ++i) {
            if (!filemap_find(target, current->items[i].path)) {
                remove(current->items[i].path);
            }
        }
    }

    // write target files
    for (i = 0; i < target->len; ++i) {
        char *buf = NULL;
        size_t len = 0;
        if (read_blob(target->items[i].hash, &buf, &len) != 0) {
            fprintf(stderr, "cs revert: missing blob %s\n", target->items[i].hash);
            free(buf);
            return -1;
        }
        ensure_parent_dirs(target->items[i].path);
        FILE *f = fopen(target->items[i].path, "wb");
        if (!f) {
            perror("fopen");
            free(buf);
            return -1;
        }
        fwrite(buf, 1, len, f);
        fclose(f);
        free(buf);
    }
    return 0;
}

static int cmd_revert(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "cs revert: missing target commit\n");
        return 1;
    }
    if (!repo_exists()) {
        fprintf(stderr, "cs: no repository found. Run ./cs init\n");
        return 1;
    }
    char target_id[MAX_HASH_LEN];
    if (strcmp(argv[2], "HEAD") == 0) {
        if (get_head(target_id) != 0 || target_id[0] == '\0') {
            fprintf(stderr, "cs revert: no commits\n");
            return 1;
        }
    } else {
        snprintf(target_id, sizeof(target_id), "%s", argv[2]);
    }

    Commit target;
    if (load_commit(target_id, &target) != 0) {
        fprintf(stderr, "cs revert: commit not found\n");
        return 1;
    }
    Commit current;
    FileMap *current_map = NULL;
    if (load_head_commit(&current) == 0) {
        current_map = &current.files;
    }
    if (restore_commit_files(&target.files, current_map) != 0) {
        filemap_free(&target.files);
        if (current_map) filemap_free(current_map);
        return 1;
    }
    set_head(target_id);
    clear_index();
    printf("Reverted to %s\n", target_id);
    filemap_free(&target.files);
    if (current_map) filemap_free(current_map);
    return 0;
}

// ------------------------------
// trace command
// ------------------------------

static int cmd_trace(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "cs trace: missing filename\n");
        return 1;
    }
    if (!repo_exists()) {
        fprintf(stderr, "cs: no repository found. Run ./cs init\n");
        return 1;
    }
    char head_id[MAX_HASH_LEN];
    if (get_head(head_id) != 0 || head_id[0] == '\0') {
        fprintf(stderr, "cs trace: no commits\n");
        return 1;
    }

    // gather commits from head backward
    Commit *history = NULL;
    size_t count = 0;
    Commit current;
    if (load_commit(head_id, &current) != 0) {
        fprintf(stderr, "cs trace: failed to load HEAD\n");
        return 1;
    }
    while (1) {
        history = realloc(history, sizeof(Commit) * (count + 1));
        history[count] = current;
        count++;
        if (current.parent[0] == '\0' || strcmp(current.parent, "null") == 0) break;
        Commit parent;
        if (load_commit(current.parent, &parent) != 0) {
            fprintf(stderr, "cs trace: broken parent %s\n", current.parent);
            return 1;
        }
        current = parent;
    }

    // traverse oldest to newest
    const char *path = argv[2];
    const char *last_hash = NULL;
    int found = 0;
    for (ssize_t i = (ssize_t)count - 1; i >= 0; --i) {
        FileEntry *e = filemap_find(&history[i].files, path);
        if (!e) continue;
        if (!last_hash || strcmp(last_hash, e->hash) != 0) {
            printf("commit %s\n", history[i].id);
            printf("timestamp %ld\n", history[i].timestamp);
            printf("message %s\n", history[i].message);
            last_hash = e->hash;
            found = 1;
        }
    }
    for (size_t i = 0; i < count; ++i) filemap_free(&history[i].files);
    free(history);
    if (!found) {
        fprintf(stderr, "cs trace: file never existed\n");
        return 1;
    }
    return 0;
}

// ------------------------------
// integrity command
// ------------------------------

static int cmd_integrity(void) {
    char head_id[MAX_HASH_LEN];
    if (!repo_exists()) {
        fprintf(stderr, "cs: no repository found. Run ./cs init\n");
        return 1;
    }
    if (get_head(head_id) != 0) {
        fprintf(stderr, "cs integrity: cannot read HEAD\n");
        return 1;
    }
    if (head_id[0] == '\0') {
        printf("Integrity OK (no commits)\n");
        return 0;
    }
    // walk reachable commits
    char current_id[MAX_HASH_LEN];
    snprintf(current_id, sizeof(current_id), "%s", head_id);
    while (1) {
        Commit c;
        if (load_commit(current_id, &c) != 0) {
            fprintf(stderr, "Integrity error: missing commit %s\n", current_id);
            return 1;
        }
        // check blobs
        for (size_t i = 0; i < c.files.len; ++i) {
            char blob_path[MAX_PATH_LEN];
            snprintf(blob_path, sizeof(blob_path), "%s/%s", CS_BLOBS_DIR, c.files.items[i].hash);
            if (!path_exists(blob_path)) {
                fprintf(stderr, "Integrity error: missing blob %s\n", c.files.items[i].hash);
                filemap_free(&c.files);
                return 1;
            }
        }
        if (c.parent[0] == '\0' || strcmp(c.parent, "null") == 0) {
            filemap_free(&c.files);
            break;
        }
        snprintf(current_id, sizeof(current_id), "%s", c.parent);
        filemap_free(&c.files);
    }
    printf("Integrity OK\n");
    return 0;
}

// ------------------------------
// timewarp command
// ------------------------------

static int cmd_timewarp(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "cs timewarp: missing timestamp\n");
        return 1;
    }
    if (!repo_exists()) {
        fprintf(stderr, "cs: no repository found. Run ./cs init\n");
        return 1;
    }
    long target = atol(argv[2]);
    char head_id[MAX_HASH_LEN];
    if (get_head(head_id) != 0 || head_id[0] == '\0') {
        fprintf(stderr, "cs timewarp: no commits\n");
        return 1;
    }

    Commit best;
    int has_best = 0;
    Commit current;
    if (load_commit(head_id, &current) != 0) {
        fprintf(stderr, "cs timewarp: failed to load HEAD\n");
        return 1;
    }
    while (1) {
        if (current.timestamp <= target) {
            if (!has_best || current.timestamp > best.timestamp) {
                best = current;
                has_best = 1;
            }
        }
        if (current.parent[0] == '\0' || strcmp(current.parent, "null") == 0) break;
        Commit parent;
        if (load_commit(current.parent, &parent) != 0) {
            fprintf(stderr, "cs timewarp: broken parent %s\n", current.parent);
            return 1;
        }
        filemap_free(&current.files);
        current = parent;
    }
    filemap_free(&current.files);

    if (!has_best) {
        fprintf(stderr, "cs timewarp: no commit before timestamp\n");
        return 1;
    }

    Commit current_head;
    if (restore_commit_files(&best.files, load_head_commit(&current_head) == 0 ? &current_head.files : NULL) != 0) {
        filemap_free(&best.files);
        return 1;
    }
    set_head(best.id);
    clear_index();
    printf("Timewarped to %s\n", best.id);
    filemap_free(&best.files);
    if (load_head_commit(&current_head) == 0) filemap_free(&current_head.files);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "init") == 0) {
        return cmd_init();
    } else if (strcmp(cmd, "add") == 0) {
        return cmd_add(argc, argv);
    } else if (strcmp(cmd, "commit") == 0) {
        return cmd_commit(argc, argv);
    } else if (strcmp(cmd, "log") == 0) {
        return cmd_log();
    } else if (strcmp(cmd, "revert") == 0) {
        return cmd_revert(argc, argv);
    } else if (strcmp(cmd, "trace") == 0) {
        return cmd_trace(argc, argv);
    } else if (strcmp(cmd, "integrity") == 0) {
        return cmd_integrity();
    } else if (strcmp(cmd, "timewarp") == 0) {
        return cmd_timewarp(argc, argv);
    } else {
        print_usage();
        return 1;
    }
}


