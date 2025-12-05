#include "../src/casky.h"
#include "../src/utils.h"
#include "../src/crc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Helper to clean temporary files
static void cleanup(const char *f) {
  unlink(f);
}

// Test: snapshot creation
void test_snapshot_creation() {
  const char *logfile = "test_snapshot.log";
  const char *snapshot = "test_snapshot.snap";
  cleanup(logfile);
  cleanup(snapshot);

  KeyDir *db = casky_open(logfile);
  assert(db != NULL);

  casky_put(db, "alpha", "1", 0);
  casky_put(db, "beta", "2", 0);

  assert(casky_do_snapshot(db, snapshot) == 0);

  // Reopen DB from snapshot only
  cleanup(logfile);
  KeyDir *db2 = casky_load_snapshot(snapshot);
  assert(db2 != NULL);

  char *v1 = casky_get(db2, "alpha");
  char *v2 = casky_get(db2, "beta");

  assert(v1 && strcmp(v1, "1") == 0);
  assert(v2 && strcmp(v2, "2") == 0);

  casky_close(db);
  casky_close(db2);
  printf("✔ test_open_close_fail passed\n");
}

// Test: incremental backup generation
// void test_incremental_backup() {
  // const char *logfile = "test_inc.log";
  // const char *snapshot = "test_inc.snap";
  // const char *incremental = "test_inc.delta";

  // cleanup(logfile);
  // cleanup(snapshot);
  // cleanup(incremental);

  // KeyDir *db = casky_open(logfile);
  // assert(db);

  // casky_put(db, "k1", "A", 0);
  // casky_put(db, "k2", "B", 0);

  // assert(casky_snapshot(db, snapshot) == 0);

  // Write more keys after snapshot
  // casky_put(db, "k3", "C", 0);
  // casky_put(db, "k4", "D", 0);

  // assert(casky_incremental_backup(db, incremental) == 0);

  // Now recreate DB from snapshot + incremental
  // KeyDir *db2 = casky_load_snapshot(snapshot);
  // assert(db2);

  // assert(casky_apply_incremental(db2, incremental) == 0);

  // assert(strcmp(casky_get(db2, "k1"), "A") == 0);
  // assert(strcmp(casky_get(db2, "k2"), "B") == 0);
  // assert(strcmp(casky_get(db2, "k3"), "C") == 0);
  // assert(strcmp(casky_get(db2, "k4"), "D") == 0);

  // casky_close(db);
  // casky_close(db2);
  // printf("✔ test_open_close_fail passed\n");
// }

// Test: incremental backup only includes changes
// void test_incremental_contains_only_new_data() {
  // const char *logfile = "test_inc2.log";
  // const char *snapshot = "test_inc2.snap";
  // const char *incremental = "test_inc2.delta";

  // cleanup(logfile);
  // cleanup(snapshot);
  // cleanup(incremental);

  // KeyDir *db = casky_open(logfile);
  // assert(db);

  // casky_put(db, "base", "X", 0);
  // casky_snapshot(db, snapshot);

  // nothing changed → incremental should be empty
  // assert(casky_incremental_backup(db, incremental) == 0);

  // FILE *fp = fopen(incremental, "r");
  // assert(fp);
  // fseek(fp, 0, SEEK_END);
  // long sz = ftell(fp);
  // fclose(fp);

  // assert(sz == 0);

  // casky_close(db);
  // printf("✔ test_open_close_fail passed\n");
// }

int main() {
  test_snapshot_creation();
  // test_incremental_backup();
  // test_incremental_contains_only_new_data();

  printf("\ntest completed\n");
  return 0;
}
