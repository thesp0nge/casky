#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "casky.h"
#include "crc.h"
#include "utils.h"
#include "version.h"

CaskyError casky_errno = CASKY_OK;

/**
 * Opens a Casky database.
 *
 * If the log file exists, reads all records and reconstructs the in-memory
 * KeyDir. Each record is verified with CRC32.
 *
 * If the log file does not exist, creates an empty KeyDir and prepares for
 * writes. The KeyDir keeps track of the number of entries, buckets, root list,
 * and the filename for persistence.
 *
 * Returns a pointer to a newly allocated KeyDir on success, or NULL on error.
 * Sets casky_errno appropriately.
 */
KeyDir* casky_open(const char *path) {
  if (!path) {
    casky_errno = CASKY_ERR_INVALID_PATH;
    return NULL;
  }

  KeyDir *kd = malloc(sizeof(KeyDir));
  if (!kd) {
    casky_errno = CASKY_ERR_MEMORY;
    return NULL;
  }


  kd->num_entries = 0;
  kd->num_buckets = CASKY_INITIAL_BUCKETS_NUM;
  kd->root = calloc(kd->num_buckets, sizeof(EntryNode *));
  if (!kd->root) {
    free(kd);
    casky_errno = CASKY_ERR_MEMORY;
    return NULL;
  }

  kd->filename = strdup(path);
  if (!kd->filename) {
    free(kd->root);
    free(kd);
    casky_errno = CASKY_ERR_MEMORY;
    return NULL;
  }

  /**
   * This flag enables or disables a fsync call in casky_write_data_to_file()
   * If set to 1 on every fflush() after a disk operation, a fsync() is
   * performed to ensure data is really pushed out from OS buffers. This have
   * performance impact but it maximizes the crash tolerance.
   *
   * Default: no fsync, preserve performances
   */

  kd->sync_on_write = 0;
  FILE *f = fopen(path, "ab+");
  if (!f) {
    free(kd->filename);
    free(kd->root);
    free(kd);
    casky_errno = CASKY_ERR_INVALID_PATH;
    return NULL;
  }

  // Rewind to start for reading existing records
  rewind(f);

  while (!feof(f)) {
    uint32_t crc, timestamp, key_len, value_len;

    if (fread(&crc, sizeof(crc), 1, f) != 1) break;
    if (fread(&timestamp, sizeof(timestamp), 1, f) != 1) break;
    if (fread(&key_len, sizeof(key_len), 1, f) != 1) break;
    if (fread(&value_len, sizeof(value_len), 1, f) != 1) break;

    char *key = malloc(key_len + 1);
    char *value = value_len > 0 ? malloc(value_len + 1) : NULL;

    if (!key || (value_len > 0 && !value)) {
      free(key);
      free(value);
      continue; // skip invalid record
    }

    if (fread(key, 1, key_len, f) != key_len) {
      free(key);
      free(value);
      break;
    }
    key[key_len] = '\0';

    if (value_len > 0) {
      if (fread(value, 1, value_len, f) != value_len) {
        free(key);
        free(value);
        break;
      }
      value[value_len] = '\0';
    }

    // Verify CRC
    size_t buf_len = sizeof(timestamp) + sizeof(key_len) + sizeof(value_len) + key_len + value_len;
    unsigned char *buf = malloc(buf_len);
    if (buf) {
      unsigned char *p = buf;
      memcpy(p, &timestamp, sizeof(timestamp)); p += sizeof(timestamp);
      memcpy(p, &key_len, sizeof(key_len)); p += sizeof(key_len);
      memcpy(p, &value_len, sizeof(value_len)); p += sizeof(value_len);
      memcpy(p, key, key_len); p += key_len;
      if (value_len > 0) memcpy(p, value, value_len);

      uint32_t computed_crc = casky_crc32(buf, buf_len);
      free(buf);

      if (computed_crc != crc) {
        // skip corrupted record
        free(key);
        free(value);
        continue;
      }
    }

    if (value_len > 0) {
      // PUT record
      casky_put_in_memory(kd, key, value, timestamp);
    } else {
      // DELETE record
      casky_delete_from_memory(kd, key);
    }

    free(key);
    free(value);
  }

  fclose(f);
  casky_errno = CASKY_OK;
  return kd;
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
  free(kd->root);
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
 * @kd: Pointer to the KeyDir (hash table) where the key-value pair is stored
 * @key: Null-terminated string representing the key
 * @value: Null-terminated string representing the value
 *
 * Returns:
 *   0 on success,
 *  -1 on failure (e.g., invalid pointer or memory allocation error)
 *
 * Sets casky_errno to reflect the operation result.
 */
int casky_put(KeyDir *kd, const char *key, const char *value) {
  if (!kd) {
    casky_errno = CASKY_ERR_INVALID_POINTER;
    return -1;
  }
  if (!key || !value) {
    casky_errno = CASKY_ERR_INVALID_KEY;
    return -1;
  }

  uint32_t timestamp = time(NULL);
  casky_put_in_memory(kd, key, value, timestamp);

  // Write record to log file
  if (casky_write_data_to_file(kd->filename, kd->sync_on_write, key, value, timestamp) != 0) {
    casky_errno = CASKY_ERR_IO;
    return -1;
  }

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
  unsigned long hash = casky_djb2_hash_xor((unsigned char *)key);
  size_t bucket_index = hash % kd->num_buckets;

  EntryNode *node = kd->root[bucket_index];
  while (node) {
    if (strcmp(key, node->entry.key) == 0) {
      casky_errno = CASKY_OK;
      return strdup(node->entry.value);
    }
    node = node->next;
  }

  casky_errno = CASKY_ERR_KEY_NOT_FOUND;
  return NULL;

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

  // Remove from memory
  int found = casky_delete_from_memory(kd, key);
  if (!found) {
    casky_errno = CASKY_ERR_KEY_NOT_FOUND;
    return -1;
  }  

  uint32_t timestamp = time(NULL);

  // Append deletion record to log file (value = NULL)
  if (casky_write_data_to_file(kd->filename, kd->sync_on_write, key, NULL, timestamp) != 0) {
    casky_errno = CASKY_ERR_IO;
    return -1;
  }

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
