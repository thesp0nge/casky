#ifndef __CASKY_H
#define __CASKY_H

#include <stddef.h>
#include <stdint.h>

typedef struct Entry {
    char key[256];
    char value[1024];
    uint64_t timestamp;
    long offset;
    size_t value_size;
} Entry;

typedef struct EntryNode {
    Entry entry;
    struct EntryNode *next;
} EntryNode;

typedef struct {
    size_t num_buckets;
    EntryNode **root;
} KeyDir;

KeyDir* casky_open(const char *path);
void   casky_close(Casky *db);

int    casky_put(Casky *db, const char *key, const char *value);
char*  casky_get(Casky *db, const char *key);
int    casky_delete(Casky *db, const char *key);

#endif

