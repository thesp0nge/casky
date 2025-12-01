#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "casky.h"
#include "utils.h"
#include "version.h"

CaskyError casky_errno = CASKY_OK;

/**
 * Opens a new casky database or create one if a NULL argument is specified.
 * Returns NULL if the specified path doesn't exist or it is not a file.
 */
KeyDir* casky_open(const char *path) {

  if (!path) {
    casky_errno = CASKY_ERR_INVALID_PATH;
    return NULL;
  }

  if (!casky_is_regular_file(path)) {
    FILE *f = fopen(path, "wb");
    if (!f) {
      casky_errno = CASKY_ERR_IO;
      return NULL;
    }
    fclose(f);
  }

  KeyDir *kd;
  kd = malloc(sizeof(KeyDir));
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

  unsigned long hash = casky_djb2_hash_xor((unsigned char *)key);
  size_t bucket_index = hash % kd->num_buckets;

  EntryNode *node = kd->root[bucket_index];
  EntryNode *prev = NULL;

  // Check if the key exists
  while (node) {
    if (strcmp(key, node->entry.key) == 0) {
      // update value
      free(node->entry.value);
      node->entry.value = strdup(value);
      node->entry.timestamp = time(NULL);

      casky_errno = CASKY_OK;
      return 0;
    }
    prev = node;
    node = node->next;
  }

  // Key not  found  here...
  EntryNode *new_node = calloc(1, sizeof(EntryNode));
  new_node->entry.key = strdup(key);
  new_node->entry.value = strdup(value);
  new_node->entry.timestamp = time(NULL);
  new_node->next = NULL;

  if (prev == NULL) {
    // empty bucket
    kd->root[bucket_index] = new_node;
  } else {
    prev->next = new_node;
  }

  kd->num_entries++;
  casky_errno = CASKY_OK;
  return casky_errno;
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
  unsigned long hash = casky_djb2_hash_xor((unsigned char *)key);
  size_t bucket_index = hash % kd->num_buckets;

  EntryNode *node = kd->root[bucket_index];
  EntryNode *prev = NULL;

  // Check if the key exists
  while (node) {
    if (strcmp(key, node->entry.key) == 0) {
      // free all the buffers and delete the node
      if (prev == NULL) 
        kd->root[bucket_index] = node->next;
      else 
        prev->next = node->next;

      free(node->entry.key);
      free(node->entry.value);
      free(node);

      kd->num_entries--;
      casky_errno = CASKY_OK;
      return 0;
    }
    prev = node;
    node = node->next;
  }
  casky_errno = CASKY_ERR_KEY_NOT_FOUND;
  return -1;

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
