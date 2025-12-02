#include "../src/casky.h"
#include "../src/utils.h"
#include "../src/crc.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ------------------------ Test Open/Close ------------------------
void test_open_close(const char *file) {
  remove(file);
  KeyDir *db = casky_open(file);
  if (!db) {
    fprintf(stderr, "casky_open failed\n");
    return;
  }
  assert(db->num_entries == 0);
  casky_close(db);
  printf("✔ test_open_close passed\n");
}

void test_open_close_fail() {
  KeyDir *db = casky_open(NULL);
  assert(db == NULL);
  printf("✔ test_open_close_fail passed\n");
}

// ------------------------ Test Hash Function ------------------------
void test_hashes() {
  unsigned long h1 = casky_djb2_hash_xor((unsigned char *)"foo");
  unsigned long h2 = casky_djb2_hash_xor((unsigned char *)"foo");
  unsigned long h3 = casky_djb2_hash_xor((unsigned char *)"alice");

  assert(h1 == h2);
  assert(h1 != h3);
  printf("✔ test_hashes passed\n");
}

// ------------------------ Test PUT ------------------------
void test_put() {
  KeyDir *db = casky_open("testdb");
  assert(casky_put(db, "foo", "bar") == 0);
  assert(db->num_entries == 1);

  // Overwrite
  assert(casky_put(db, "foo", "baz") == 0);
  assert(db->num_entries == 1);

  // Altra chiave
  assert(casky_put(db, "alice", "bob") == 0);
  assert(db->num_entries == 2);

  casky_close(db);
  printf("✔ test_put passed\n");
}

// ------------------------ Test GET ------------------------
void test_get() {
  KeyDir *db = casky_open("testdb");
  casky_put(db, "foo", "bar");
  casky_put(db, "alice", "bob");

  char *val = casky_get(db, "foo");
  assert(val != NULL && strcmp(val, "bar") == 0);
  free(val);

  val = casky_get(db, "alice");
  assert(val != NULL && strcmp(val, "bob") == 0);
  free(val);

  val = casky_get(db, "unknown");
  assert(val == NULL);
  assert(casky_errno == CASKY_ERR_KEY_NOT_FOUND);

  // Test NULL key
  val = casky_get(db, NULL);
  assert(val == NULL);
  assert(casky_errno == CASKY_ERR_INVALID_KEY);

  casky_close(db);
  printf("✔ test_get passed\n");
}

// ------------------------ Test DELETE ------------------------
void test_delete() {
  remove("testdb");
  KeyDir *db = casky_open("testdb");
  casky_put(db, "foo", "bar");
  casky_put(db, "alice", "bob");

  assert(casky_delete(db, "foo") == 0);
  char *val = casky_get(db, "foo");
  assert(val == NULL);
  assert(db->num_entries == 1);

  // Delete chiave non esistente
  assert(casky_delete(db, "nonexistent") == -1);
  assert(casky_errno == CASKY_ERR_KEY_NOT_FOUND);
  assert(db->num_entries == 1);

  // Delete NULL key
  assert(casky_delete(db, NULL) == -1);
  assert(casky_errno == CASKY_ERR_INVALID_KEY);

  // Check that num_entries matches actual nodes
  size_t count = 0;
  for (size_t i = 0; i < db->num_buckets; i++) {
    EntryNode *node = db->root[i];
    while (node) {
      count++;
      node = node->next;
    }
  }
  assert(count == db->num_entries);

  casky_close(db);
  printf("✔ test_delete passed\n");
}

// ------------------------ Test Collisions ------------------------
void test_collisions() {
  remove("testdb");
  KeyDir *db = casky_open("testdb");

  // For simplicity, force keys into same bucket by using same hash mod
  // (here we assume small bucket size for demonstration)
  casky_put(db, "a", "1");
  casky_put(db, "b", "2");
  casky_put(db, "c", "3");

  assert(db->num_entries == 3);

  char *val = casky_get(db, "b");
  assert(val != NULL && strcmp(val, "2") == 0);
  free(val);

  casky_delete(db, "a");
  assert(casky_get(db, "a") == NULL);

  casky_close(db);
  printf("✔ test_collisions passed\n");
}

void test_open_creates_or_reads_log() {
  const char *logfile = "testdb2.log";

  remove(logfile);

  KeyDir *db = casky_open(logfile);
  assert(db != NULL);
  assert(db->num_entries == 0);
  casky_close(db);

  db = casky_open(logfile);
  assert(db != NULL);
  casky_close(db);

  printf("✔ test_open_creates_or_reads_log passed\n");
}

void test_put_writes_log() {
  const char *logfile = "testdb2.log";
  remove(logfile);

  KeyDir *db = casky_open(logfile);
  assert(db != NULL);

  int ret = casky_put(db, "foo", "bar");
  assert(ret == 0);
  assert(db->num_entries == 1);

  casky_close(db);
  db = casky_open(logfile);
  char *val = casky_get(db, "foo");
  assert(val != NULL); 
  assert(strcmp(val, "bar") == 0);
  free(val);

  casky_close(db);
  printf("✔ test_put_writes_log passed\n");
}

void test_delete_writes_log() {
  const char *logfile = "testdb2.log";
  remove(logfile);

  KeyDir *db = casky_open(logfile);
  casky_put(db, "foo", "bar");

  int ret = casky_delete(db, "foo");
  assert(ret == 0);
  assert(db->num_entries == 0);

  // Riapri → chiave non deve esistere
  casky_close(db);
  db = casky_open(logfile);
  char *val = casky_get(db, "foo");
  assert(val == NULL);
  assert(casky_errno == CASKY_ERR_KEY_NOT_FOUND);

  casky_close(db);
  printf("✔ test_delete_writes_log passed\n");
}

void test_log_integrity() {
  const char *logfile = "testdb2.log";
  remove(logfile);

  KeyDir *db = casky_open(logfile);
  casky_put(db, "foo", "bar");
  casky_close(db);

  // Simula corruzione
  FILE *f = fopen(logfile, "r+b");
  fseek(f, 0, SEEK_SET);
  fputc('X', f); // corrompe primo byte
  fclose(f);

  db = casky_open(logfile);
  // L'apertura deve segnalare errore di log corrotto
  assert(db == NULL);
  assert(casky_errno == CASKY_ERR_CORRUPT);

  printf("✔ test_log_integrity passed\n");
}

void test_multiple_operations_persist() {
  const char *logfile = "testdb2.log";
  remove(logfile);

  KeyDir *db = casky_open(logfile);
  casky_put(db, "foo", "bar");
  casky_put(db, "alice", "bob");
  casky_delete(db, "foo");
  casky_put(db, "carol", "dan");
  casky_close(db);

  db = casky_open(logfile);
  assert(casky_get(db, "foo") == NULL);
  assert(strcmp(casky_get(db, "alice"), "bob") == 0);
  assert(strcmp(casky_get(db, "carol"), "dan") == 0);
  casky_close(db);

  printf("✔ test_multiple_operations_persist passed\n");
}

// ------------------------ Main ------------------------
int main(void) {
  const char *testfile = "testdb";

  test_open_close_fail();
  test_open_close(testfile);
  test_hashes();
  test_put();
  test_get();
  test_delete();
  test_collisions();

  test_open_creates_or_reads_log();
  test_put_writes_log();
  test_delete_writes_log();
  test_log_integrity();
  test_multiple_operations_persist();
  if (remove(testfile) == 0) {
    printf("✔ test file '%s' removed\n", testfile);
  } else {
    perror("Failed to remove test file");
  }

  printf("\ntest completed\n");
  return 0;
}
