#ifndef __CASKY_H
#define __CASKY_H

#define CASKY_INITIAL_BUCKETS_NUM   1024

#include <stddef.h>
#include <stdint.h>

/**
 * Thread-safety and compile-time configuration
 *
 * Casky API is designed to be lightweight and adhere closely to the original
 * Bitcask paper by default. In this mode, there is no internal locking, and
 * concurrent access to the same KeyDir structure from multiple threads must
 * be managed externally by the caller.
 *
 * To provide optional thread-safety, a compile-time flag THREAD_SAFE can be
 * defined (-DTHREAD_SAFE). When enabled:
 *   - All API functions (casky_put, casky_get, casky_delete, casky_compact)
 *     acquire a mutex internally before accessing the KeyDir memory structure
 *     or writing to the log file.
 *   - This ensures safe concurrent access across multiple threads within the
 *     same process.
 *   - Performance overhead is minimal when THREAD_SAFE is disabled.
 *
 * This approach gives users the flexibility to choose between:
 *   1) strict Bitcask compatibility (no locks, maximum throughput), or
 *   2) a resilient, thread-safe API suitable for multithreaded applications.
 */
#ifdef THREAD_SAFE
#include <pthread.h>
#endif

#include "version.h"


typedef struct Entry {
    char *key;
    char *value;
    uint64_t timestamp;
    long offset;
    size_t value_size;
} Entry;

typedef struct EntryNode {
    Entry entry;
    struct EntryNode *next;
} EntryNode;

typedef struct KeyDir {
    size_t num_entries;   // total num of keys
    size_t num_buckets;   // total num of items in root array
    EntryNode **root;     // the directory root
    char *filename;       // path to the log file
    int sync_on_write;    // if set to 1 forces an fsync on *every* write on
                          // disk. Useful for maximum resilience but it has
                          // impact on performances
    int corrupted_dir;    // if set to 1 casky_open() found a corrupted entry and
                          // a COMPACT operation is suggested
#ifdef THREAD_SAFE
    pthread_mutex_t lock; // mutex for thread-safe access
#endif
} KeyDir;

typedef enum {
    CASKY_OK = 0,
    CASKY_ERR_INVALID_PATH,
    CASKY_ERR_INVALID_POINTER,
    CASKY_ERR_IO,
    CASKY_ERR_MEMORY,
    CASKY_ERR_CORRUPT,
    CASKY_ERR_INVALID_KEY,
    CASKY_ERR_KEY_NOT_FOUND,
} CaskyError;

extern CaskyError casky_errno;

KeyDir* casky_open(const char *path);
void    casky_close(KeyDir *kd);

int     casky_put(KeyDir *kd, const char *key, const char *value);
char*   casky_get(KeyDir *kd, const char *key);
int     casky_delete(KeyDir *kd, const char *key);
int     casky_compact(KeyDir *kd);

const char* casky_version(void);
#endif

