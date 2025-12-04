#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "casky.h"
#include "crc.h"
#include "utils.h"

static casky_stat_t casky_statistics;

#ifdef THREAD_SAFE
#include <pthread.h>
#define LOCK_STATS(s) pthread_mutex_lock(&(s).lock)
#define UNLOCK_STATS(s) pthread_mutex_unlock(&(s).lock)
#else
#define LOCK_STATS(s)
#define UNLOCK_STATS(s)
#endif


/**
 * Checks whether the given path refers to a regular file.
 *
 * Parameters:
 *   path - a null-terminated string containing the filesystem path
 *
 * Returns:
 *   1 if the path exists and is a regular file
 *   0 otherwise (path does not exist, cannot be accessed, or is not a regular file)
 */
int casky_is_regular_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0)
        return 0; 
    if (S_ISREG(st.st_mode))
        return 1;
    return 0;
}

/**
 * casky_strerror - Returns a human-readable string for a Casky error code.
 *
 * @err: The CaskyError value to translate.
 *
 * This function takes an error code returned by any Casky API function
 * and returns a descriptive string suitable for logging or displaying
 * to the user. It helps applications using Casky to interpret errors
 * without having to maintain their own mapping of error codes.
 *
 * Returns: A constant null-terminated string describing the error.
 */
const char* casky_strerror(CaskyError err) {
  switch (err) {
    case CASKY_OK: return "OK";
    case CASKY_ERR_INVALID_PATH: return "Invalid path";
    case CASKY_ERR_INVALID_POINTER: return "Invalid pointer";  
    case CASKY_ERR_IO: return "I/O error";
    case CASKY_ERR_MEMORY: return "Out of memory";
    case CASKY_ERR_CORRUPT: return "Data corrupt";
    case CASKY_ERR_INVALID_KEY: return "Invalid key";
    case CASKY_ERR_KEY_NOT_FOUND: return "Key not found";
    default: return "Unknown error";
  }
}

/**
 * djb2 hash function (XOR variant)
 *
 * Computes a hash value for a string using the magic constant 33 
 * and XOR operation. This variant is widely used for hash tables 
 * and has proven effective in practice.
 *
 * Formula:
 *     hash(i) = hash(i-1) * 33 ^ str[i]
 *
 * @str: Input string to hash
 *
 * Returns: Computed hash value as an unsigned long
 */
unsigned long casky_djb2_hash_xor(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = (hash * 33) ^ c;  // XOR variant
    }

    return hash;
}

/**
 * casky_write_data_to_file
 *
 * Writes a key/value record to the append-only log file.
 * 
 * Record format (Bitcask style):
 *  - PUT:    [CRC][Timestamp][ExpirationTs][KeyLen][ValueLen][Key][Value]
 *  - DELETE: [CRC][Timestamp][ExpirationTs][KeyLen][0][Key]
 *
 * Parameters:
 *  - logfile: path to the log file
 *  - sync_on_write: if non-zero, forces an fsync() after writing to ensure
 *                   crash-resilient persistence
 *  - key: the key to store or delete
 *  - value: the value to store; NULL if this is a DELETE record
 *
 * Returns:
 *  - 0 on success
 *  - -1 on error (errno set in casky_errno)
 *
 * Notes:
 *  - Calculates CRC32 over the record (excluding the CRC field itself)
 *  - Allocates a temporary buffer for CRC calculation
 *  - Writes in binary append mode
 */
int casky_write_data_to_file(const char *logfile, int sync_on_write, 
                             const char *key, const char *value, 
                             uint64_t timestamp, uint64_t expires) {

  // PUT  record: [CRC][Timestamp][KeyLen][ValueLen][Key][Value]
  // DELETE record: [CRC][Timestamp][KeyLen][0][Key]

  FILE *f = fopen(logfile, "ab");       // apri in append-binary
  if (!f) {
    casky_errno =  CASKY_ERR_INVALID_PATH;
    return -1;
  }

  if (!key) {
    // value string can be NULL in case of DELETE
    casky_errno = CASKY_ERR_INVALID_POINTER;
    return -1;
  }

  uint32_t key_len   = strlen(key);
  uint32_t value_len = value ? strlen(value) : 0;

  size_t buf_len = sizeof(timestamp) + sizeof(expires) + sizeof(key_len) + sizeof(value_len) + key_len + value_len;
  unsigned char *buf = malloc(buf_len);
  if (!buf) {
    fclose(f);
    casky_errno = CASKY_ERR_MEMORY;
    return -1;
  }

  unsigned char *p = buf;
  memcpy(p, &timestamp, sizeof(timestamp)); p += sizeof(timestamp);
  memcpy(p, &expires, sizeof(expires)); p+= sizeof(expires);
  memcpy(p, &key_len, sizeof(key_len)); p += sizeof(key_len);
  memcpy(p, &value_len, sizeof(value_len)); p += sizeof(value_len);
  memcpy(p, key, key_len); p += key_len;
  if (value)
    memcpy(p, value, value_len);

  uint32_t crc = casky_crc32(buf, buf_len);
  free(buf);

  fwrite(&crc, sizeof(crc), 1, f);
  fwrite(&timestamp, sizeof(timestamp), 1, f);
  fwrite(&expires, sizeof(expires), 1, f);
  fwrite(&key_len, sizeof(key_len), 1, f);
  fwrite(&value_len, sizeof(value_len), 1, f);
  fwrite(key, 1, key_len, f);
  if (value_len > 0)
    fwrite(value, 1, value_len, f);

  fflush(f);
  if (sync_on_write == 1)
    fsync(fileno(f));
  fclose(f);

  return 0;
}
/**
 * Inserts or updates a key-value pair **in memory** (KeyDir only),
 * without writing to the log file. Used internally when loading
 * the database from disk.
 *
 * If the key exists, updates the value and timestamp.
 * If the key does not exist, appends a new EntryNode in the correct bucket.
 *
 * @param kd        Pointer to KeyDir
 * @param key       Key string (null-terminated)
 * @param value     Value string (null-terminated)
 * @param timestamp Optional timestamp to set (e.g., from log)
 * @param expires   The timestamp where this entry is expired and no longer
 *                  valid
 */
void casky_put_in_memory(KeyDir *kd, const char *key, const char *value, uint64_t timestamp, uint64_t expires) {
  if (!kd || !key || !value) return;

  unsigned long hash = casky_djb2_hash_xor((unsigned char*)key);
  size_t bucket_index = hash % kd->num_buckets;

  EntryNode *node = kd->root[bucket_index];
  EntryNode *prev = NULL;

  while (node) {
    if (strcmp(key, node->entry.key) == 0) {
      // update existing value
      free(node->entry.value);
      node->entry.value = strdup(value);
      node->entry.timestamp = timestamp;
      node->entry.expiration_ts = expires;

      casky_stats_inc_put(strlen(node->entry.key) + strlen(node->entry.value));
      return;
    }
    prev = node;
    node = node->next;
  }

  // key not found → create new node
  EntryNode *new_node = calloc(1, sizeof(EntryNode));
  new_node->entry.key = strdup(key);
  new_node->entry.value = strdup(value);
  new_node->entry.timestamp = timestamp;
  new_node->entry.expiration_ts = expires;
  new_node->next = NULL;

  casky_stats_inc_entries();
  casky_stats_inc_put(strlen(new_node->entry.key) + strlen(new_node->entry.value));

  if (!prev) {
    // empty bucket
    kd->root[bucket_index] = new_node;
  } else {
    prev->next = new_node;
  }

  kd->num_entries++;
}

/**
 * Deletes a key from memory (KeyDir only), without writing to the log file.
 * Used internally when replaying DELETE records from the log.
 *
 * If the key is not found, does nothing.
 *
 * @param kd  Pointer to KeyDir
 * @param key Key string to delete
 * @return 1 if the key was found and deleted, 0 otherwise
 */
int casky_delete_from_memory(KeyDir *kd, const char *key) {
  if (!kd || !key)
    return 0;

  unsigned long hash = casky_djb2_hash_xor((unsigned char *)key);
  size_t bucket_index = hash % kd->num_buckets;

  EntryNode *node = kd->root[bucket_index];
  EntryNode *prev = NULL;

  while (node) {
    if (strcmp(node->entry.key, key) == 0) {
      // Found the key, remove it
      if (prev == NULL) {
        kd->root[bucket_index] = node->next;
      } else {
        prev->next = node->next;
      }
      casky_stats_inc_delete(strlen(node->entry.key) + strlen(node->entry.value));
      casky_stats_dec_entries();

      free(node->entry.key);
      free(node->entry.value);
      free(node);

      kd->num_entries--;
      return 1; // key was found and deleted
    }
    prev = node;
    node = node->next;
  }

  return 0; // key not found
}

/**
 * casky_get_from_memory - Core function to retrieve a value from the in-memory KeyDir
 * @kd: pointer to the KeyDir structure
 * @key: key to look up
 *
 * This function searches the in-memory hash table for the given key and returns
 * a dynamically allocated copy of its value if found. It does NOT perform any
 * locking and is therefore NOT thread-safe. Use casky_get() if you need thread
 * safety (it wraps this core function with a mutex when compiled with -DTHREAD_SAFE).
 *
 * Return: strdup'ed value on success, NULL on error (sets casky_errno)
 */
char* casky_get_from_memory(KeyDir *kd, const char *key) {
  if (!kd) {
    casky_errno = CASKY_ERR_INVALID_POINTER;
    return NULL;
  }
  if (!key) {
    casky_errno = CASKY_ERR_INVALID_KEY;
    return NULL;
  }

  unsigned long hash = casky_djb2_hash_xor((unsigned char *)key);
  size_t bucket_index = hash % kd->num_buckets;

  EntryNode *node = kd->root[bucket_index];

  uint64_t now = (uint64_t)time(NULL);

  while (node) {
    if (strcmp(key, node->entry.key) == 0) {
      if (node->entry.expiration_ts > 0 && node->entry.expiration_ts <= now) {
        // expired key
        casky_stats_inc_delete(strlen(node->entry.key) + strlen(node->entry.value));
        casky_delete_from_memory(kd, key);
      } else {
        casky_errno = CASKY_OK;
        casky_stats_inc_get();
        return strdup(node->entry.value);
      }
    }
    node = node->next;
  }

  casky_errno = CASKY_ERR_KEY_NOT_FOUND;
  return NULL;
}

// STAT utility routines
/**
 * Initialize a casky_stat_t structure.
 * Sets all counters to zero and initializes the mutex if thread-safety is enabled.
 *
 * Parameters:
 *  - stats: pointer to the casky_stat_t structure to initialize 
 */
void casky_stats_init() {
  LOCK_STATS(casky_statistics);
  casky_statistics.total_keys = 0;
  casky_statistics.memory_bytes = 0;
  casky_statistics.num_puts = 0;
  casky_statistics.num_gets = 0;
  casky_statistics.num_deletes = 0;
  UNLOCK_STATS(casky_statistics);
  
}

void casky_stats_inc_put(size_t bytes) {
  LOCK_STATS(casky_statistics);
  casky_statistics.num_puts++;
  casky_statistics.memory_bytes += bytes;
  UNLOCK_STATS(casky_statistics);
}

void casky_stats_inc_delete(size_t bytes) {
  LOCK_STATS(casky_statistics);
  casky_statistics.num_deletes++;
  if (bytes > 0 && casky_statistics.memory_bytes >= bytes)
    casky_statistics.memory_bytes -= bytes;
  UNLOCK_STATS(casky_statistics);

}
casky_stat_t casky_stats_get(void) {
  LOCK_STATS(casky_statistics);
  casky_stat_t copy;
  copy = casky_statistics;
  UNLOCK_STATS(casky_statistics);
  return copy;
}
void casky_stats_inc_entries(void) {
  LOCK_STATS(casky_statistics);
  casky_statistics.total_keys++;
  UNLOCK_STATS(casky_statistics);
}
void casky_stats_dec_entries(void) {
  LOCK_STATS(casky_statistics);
  if (casky_statistics.total_keys > 0)
    casky_statistics.total_keys--;
  UNLOCK_STATS(casky_statistics);
}
void casky_stats_inc_get(void) {
  LOCK_STATS(casky_statistics);
  casky_statistics.num_gets++;
  UNLOCK_STATS(casky_statistics);
}
