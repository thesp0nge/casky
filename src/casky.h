#ifndef __CASKY_H
#define __CASKY_H

#define CASKY_INITIAL_BUCKETS_NUM   1024

#include <stddef.h>
#include <stdint.h>

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
    size_t num_entries; // total num of keys
    size_t num_buckets; // total num of items in root array
    EntryNode **root;   // the directory root
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
void   casky_close(KeyDir *kd);

int    casky_put(KeyDir *kd, const char *key, const char *value);
char*  casky_get(KeyDir *kd, const char *key);
int    casky_delete(KeyDir *kd, const char *key);

const char* casky_version(void);
#endif

