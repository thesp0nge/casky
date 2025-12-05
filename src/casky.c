#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include "casky.h"
#include "crc.h"
#include "utils.h"
#include "version.h"



CaskyError casky_errno = CASKY_OK;

KeyDir *casky_init_kd_from_file(const char *file, int open_log) {
  if (!file) {
    casky_errno = CASKY_ERR_INVALID_PATH;
    return NULL;
  }

  FILE *f = fopen(file, "rb");  // prova ad aprire in lettura
  if (!f && open_log) {
    // File inesistente? crea un file vuoto
    f = fopen(file, "wb");
    if (!f) {
      casky_errno = CASKY_ERR_IO;
      return NULL;
    }
    fclose(f);
    f = fopen(file, "rb");
  }

  KeyDir *kd = calloc(1, sizeof(KeyDir));
  if (!kd) {
    if (f) fclose(f);
    casky_errno = CASKY_ERR_MEMORY;
    return NULL;
  }

  kd->num_buckets = CASKY_INITIAL_BUCKETS_NUM;
  kd->root = calloc(kd->num_buckets, sizeof(EntryNode*));
  if (!kd->root) {
    free(kd);
    if (f) fclose(f);
    casky_errno = CASKY_ERR_MEMORY;
    return NULL;
  }

  kd->log = NULL;
  kd->sync_on_write = open_log ? 1 : 0;  // default sync for main log
  kd->filename = strdup(file); 
  if (!kd->filename) {
    casky_errno = CASKY_ERR_MEMORY;
    free(kd);
    return NULL;
  }
#ifdef THREAD_SAFE
  pthread_mutex_init(&kd->lock, NULL);
#endif

  // Load existing entries
  if (f) {
    while (!feof(f)) {
      uint32_t crc, key_len, value_len;
      uint64_t timestamp, expires;

      if (fread(&crc, sizeof(crc), 1, f) != 1) break;
      if (fread(&timestamp, sizeof(timestamp), 1, f) != 1) break;
      if (fread(&expires, sizeof(expires), 1, f) != 1) break;
      if (fread(&key_len, sizeof(key_len), 1, f) != 1) break;
      if (fread(&value_len, sizeof(value_len), 1, f) != 1) break;

      char *key = malloc(key_len + 1);
      char *value = malloc(value_len + 1);
      if (!key || (!value && value_len > 0)) { free(key); free(value); break; }

      if (fread(key, 1, key_len, f) != key_len) { free(key); free(value); break; }
      key[key_len] = '\0';
      if (value_len > 0) {
        if (fread(value, 1, value_len, f) != value_len) { free(key); free(value); break; }
        value[value_len] = '\0';
      } else {
        free(value);
        value = NULL;
      }

      // Only load valid (non-expired) entries
      if (value_len == 0) {
        // DELETE record → non inserire nulla in memoria
        casky_delete_from_memory(kd, key);
      } else if (expires == 0 || expires > (uint64_t)time(NULL)) {
        // PUT record non scaduto → inserisci o aggiorna
        casky_put_in_memory(kd, key, value, timestamp, expires);
      }

      free(key);
      if (value) free(value);
    }
    fclose(f);
  }

  // Open the log for further writes (casky_put)
  if (open_log) {
    FILE *log_fp = fopen(file, "ab+");
    if (!log_fp) {
      // Se non esiste, crealo
      log_fp = fopen(file, "wb+");
      if (!log_fp) {
        free(kd->root);
        free(kd);
        casky_errno = CASKY_ERR_IO;
        return NULL;
      }
    }
    kd->log = log_fp;
  }

  casky_errno = CASKY_OK;
  return kd;
}

/**
 * Opens a Bitcask-style log-structured key-value database.
 *
 * - Loads the database from the given log file into memory.
 * - Initializes a new KeyDir structure with buckets, number of entries, and filename.
 * - Sets `sync_on_write` to 0 by default (can be enabled later for full fsync on each write).
 * - Reads existing records from the file:
 *     - For each record, validates the CRC.
 *     - If a record is corrupted:
 *         - Stops processing further records (Bitcask-style behavior).
 *         - Sets kd->corrupted_dir = 1 to indicate a compact is recommended.
 *         - Sets casky_errno = CASKY_ERR_CORRUPT.
 *     - Otherwise, reconstructs the in-memory key directory:
 *         - PUT records: inserted/updated via casky_put_in_memory().
 *         - DELETE records: removed via casky_delete_from_memory().
 * - Returns a pointer to the KeyDir structure on success, NULL on failure.
 *
 * Error codes (set in casky_errno):
 * - CASKY_OK: successful open, all read records are valid.
 * - CASKY_ERR_INVALID_PATH: file path is NULL or cannot be opened.
 * - CASKY_ERR_MEMORY: memory allocation failure.
 * - CASKY_ERR_CORRUPT: a corrupted record was encountered (database partially loaded).
 *
 * Note:
 * - After encountering the first corrupted record, all subsequent records in the log are ignored.
 * - Users can call casky_compact() to remove corrupted records and reclaim a clean log file.
 * - Even if corruption is detected, the returned KeyDir may contain valid entries read before the corruption.
 *
 * @param path Path to the log file.
 * @return Pointer to KeyDir structure on success, NULL on failure.
 */
KeyDir* casky_open(const char *path) {
  static int initialized = 0;
  if (!initialized) {
    casky_stats_init();
    initialized = 1;
  }
  return casky_init_kd_from_file(path, 1);
}

/**
 * casky_close - Frees all memory associated with a KeyDir.
 *
 * @kd: Poi_nter to the KeyDir to close.
 *
 * This function releases all resources allocated by Casky for the given
 * KeyDir, including:
 *   - All EntryNode nodes and their dynamically allocated keys and values
 *   - The array of buckets (root)
 *   - The KeyDir structure itself
 *
 * After calling this function, the KeyDir pointer should not be used.
 * It ensures there are no memory leaks and allows Casky to be safely
 * used as a library within other applications.
 */
void casky_close(KeyDir *kd) {
  if (!kd) {
    casky_errno = CASKY_ERR_INVALID_POINTER;
    return;
  }

  for (size_t i=0; i<kd->num_buckets; i++) {
    EntryNode *node = kd->root[i];
    while(node) {
      EntryNode *next = node->next;
      free(node->entry.key);
      free(node->entry.value);
      free(node);
      node = next;
    }
  }
#ifdef THREAD_SAFE
  pthread_mutex_destroy(&kd->lock);
#endif
  casky_flush_log(kd);
  fclose(kd->log);
  free(kd->root);
  if (kd->filename) free(kd->filename);
  free(kd);

  casky_errno = CASKY_OK;
  return ;
}

/**
 * casky_put - Insert or update a key-value pair in the database
 *
 * This function stores a key-value pair in the given KeyDir (hash table).
 * If the key already exists, its value and timestamp are updated.
 * If the key does not exist, a new EntryNode is created and appended
 * to the corresponding bucket's linked list.
 *
 * Memory ownership:
 *   - The function makes a copy of both key and value using strdup.
 *   - The caller can safely modify or free the original key/value after the call.
 *
 * Bucket selection:
 *   - The bucket is determined by hashing the key with casky_djb2_hash_xor
 *     and taking the modulo with the number of buckets.
 *
 * @kd:    Pointer to the KeyDir (hash table) where the key-value pair is stored
 * @key:   Null-terminated string representing the key
 * @value: Null-terminated string representing the value
 * @#ttl:  the time to live for this entry. If set to 0 than the record doesn't
 *         expire.
 *
 * Returns:
 *   0 on success,
 *  -1 on failure (e.g., invalid pointer or memory allocation error)
 *
 * Sets casky_errno to reflect the operation result.
 */
int casky_put(KeyDir *kd, const char *key, const char *value, uint32_t ttl) {
  if (!kd) {
    casky_errno = CASKY_ERR_INVALID_POINTER;
    return -1;
  }
  if (!key || !value) {
    casky_errno = CASKY_ERR_INVALID_KEY;
    return -1;
  }

  LOCK(kd);
  uint64_t timestamp = time(NULL);
  uint64_t expires = (ttl>0) ? (uint64_t)time(NULL) + ttl : 0;
  casky_put_in_memory(kd, key, value, timestamp, expires);

  // Write record to log file
  if (casky_write_data_to_file(kd->log, kd->sync_on_write, key, value, timestamp, expires) != 0) {
    casky_errno = CASKY_ERR_IO;
    UNLOCK(kd);
    return -1;
  }
  UNLOCK(kd);

  casky_errno = CASKY_OK;
  return 0;
}

/**
 * casky_get - Retrieve a value by key from the database
 *
 * Searches the KeyDir (hash table) for the given key.
 * If the key exists, returns a newly allocated copy of the value.
 * The caller is responsible for freeing the returned string.
 *
 * @kd: Pointer to the KeyDir (hash table)
 * @key: Null-terminated string representing the key
 *
 * Returns:
 *   Pointer to a newly allocated string containing the value,
 *   or NULL if the key is not found or if kd/key are invalid.
 *
 * Sets casky_errno:
 *   CASKY_OK if the key was found,
 *   CASKY_ERR_KEY_NOT_FOUND if the key does not exist in the database,
 *   CASKY_ERR_INVALID_POINTER if kd is NULL,
 *   CASKY_ERR_INVALID_KEY if key is NULL.
 */

char*  casky_get(KeyDir *kd, const char *key){
  if (!kd) {
    casky_errno = CASKY_ERR_INVALID_POINTER;
    return NULL;
  }
  if (!key) {
    casky_errno = CASKY_ERR_INVALID_KEY;
    return NULL;
  }
  LOCK(kd);
  char *value = casky_get_from_memory(kd, key);
  UNLOCK(kd);

  return value;
}

/**
 * casky_delete - Remove a key-value pair from the database
 *
 * Searches for the given key in the KeyDir (hash table) and deletes
 * the corresponding EntryNode if it exists. Frees the key and value
 * buffers and updates the linked list and num_entries count.
 *
 * @kd: Pointer to the KeyDir (hash table)
 * @key: Null-terminated string representing the key to delete
 *
 * Returns:
 *   0 if the key was found and deleted successfully,
 *  -1 if kd or key is invalid, or if the key does not exist.
 *
 * Sets casky_errno:
 *   CASKY_OK if deletion was successful,
 *   CASKY_ERR_INVALID_POINTER if kd is NULL,
 *   CASKY_ERR_INVALID_KEY if key is NULL,
 *   CASKY_ERR_KEY_NOT_FOUND if the key does not exist.
 */
int    casky_delete(KeyDir *kd, const char *key) {
  if (!kd) {
    casky_errno = CASKY_ERR_INVALID_POINTER;
    return -1;
  }
  if (!key) {
    casky_errno = CASKY_ERR_INVALID_KEY;
    return -1;
  }

  LOCK(kd);
  // Remove from memory
  int found = casky_delete_from_memory(kd, key);
  if (!found) {
    casky_errno = CASKY_ERR_KEY_NOT_FOUND;
    return -1;
  }  

  uint64_t timestamp = time(NULL);

  // Append deletion record to log file (value = NULL)
  if (casky_write_data_to_file(kd->log, kd->sync_on_write, key, NULL, timestamp, 0) != 0) {
    casky_errno = CASKY_ERR_IO;
    UNLOCK(kd);
    return -1;
  }
  UNLOCK(kd);

  casky_errno = CASKY_OK;
  return 0;

}

/**
 * Returns the current version of the Casky library.
 *
 * This function provides a human-readable version string following
 * Semantic Versioning (MAJOR.MINOR.PATCH). Useful for logging,
 * debugging, or verifying the library version at runtime.
 *
 * @return const char* - the version string, e.g., "0.10.0"
 */
const char* casky_version(void) {
  return CASKY_VERSION_STRING;
}

/**
 * casky_compact - Compacts the database by writing all valid in-memory records
 *                  to a new temporary log file and replacing the original log.
 *
 * Parameters:
 *   kd - pointer to KeyDir containing all valid records loaded from log
 *
 * Returns:
 *   0 on success, -1 on failure (sets casky_errno)
 *
 * Notes:
 *   - Only valid records in memory are written; corrupted records are discarded.
 *   - The operation is atomic: first a temp file is written, then renamed.
 */
int casky_compact(KeyDir *kd) {
  if (!kd || !kd->filename) {
    casky_errno = CASKY_ERR_INVALID_POINTER;
    return -1;
  }
  LOCK(kd);
  // Create a safe temporary file
  char tmpfile_template[PATH_MAX];
  snprintf(tmpfile_template, sizeof(tmpfile_template), "%s.XXXXXX", kd->filename);

  int fd = mkstemp(tmpfile_template);
  if (fd == -1) {
    casky_errno = CASKY_ERR_IO;
    UNLOCK(kd);
    return -1;
  }

  FILE *f = fdopen(fd, "wb");
  if (!f) {
    close(fd);
    casky_errno = CASKY_ERR_IO;
    UNLOCK(kd);
    return -1;
  }

  // Iterate all buckets and nodes to write current in-memory entries
  if (kd->root && kd->num_entries > 0) {
    for (size_t i = 0; i < kd->num_buckets; i++) {
      EntryNode *node = kd->root[i];
      while (node) {
        // Append record to temp file
        if (casky_write_data_to_file(f, kd->sync_on_write,
                                     node->entry.key, node->entry.value,
                                     node->entry.timestamp,
                                     node->entry.expiration_ts) != 0) {
          fclose(f);
          remove(tmpfile_template);
          casky_errno = CASKY_ERR_IO;
          UNLOCK(kd);
          return -1;
        }
        node = node->next;
      }
    }
  }
  fflush(f);
  if (kd->sync_on_write) 
    fsync(fileno(f));
  fclose(f);

  // Atomically replace old log file with compacted temp file
  if (rename(tmpfile_template, kd->filename) != 0) {
    remove(tmpfile_template);
    casky_errno = CASKY_ERR_IO;
    UNLOCK(kd);
    return -1;
  }

  kd->log = fopen(kd->filename, "ab+");
  UNLOCK(kd);
  casky_errno = CASKY_OK;
  return 0;
}


void casky_expire(KeyDir *kd) {
  if (!kd) {
    casky_errno = CASKY_ERR_INVALID_POINTER;
    return ;
  }
  uint64_t now = (uint64_t)time(NULL);

  LOCK(kd);

  for (size_t i = 0; i < kd->num_buckets; i++) {
    EntryNode *prev = NULL;
    EntryNode *node = kd->root[i];

    while (node) {
      if (node->entry.expiration_ts > 0 &&
        node->entry.expiration_ts <= now) {

        EntryNode *next = node->next;

        if (prev)
          prev->next = next;
        else
          kd->root[i] = next;

        casky_stats_inc_delete(strlen(node->entry.key) + strlen(node->entry.value));

        kd->num_entries--;

        free(node->entry.key);
        free(node->entry.value);
        free(node);

        // Continua con il prossimo
        node = next;
      } else {
        prev = node;
        node = node->next;
      }
    }
  }

  UNLOCK(kd);
}
