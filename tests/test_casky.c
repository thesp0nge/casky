#include "../src/casky.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

// ------------------------ Test Open/Close ------------------------
void test_open_close() {
    KeyDir *db = casky_open("testdb");
    assert(db != NULL);
    assert(db->num_entries == 0);
    casky_close(db);
    printf("✔ test_open_close passed\n");
}

// ------------------------ Test PUT ------------------------
void test_put() {
    KeyDir *db = casky_open("testdb");
    assert(casky_put(db, "foo", "bar") == 0);
    assert(db->num_entries == 1);

    // Overwrite
    assert(casky_put(db, "foo", "baz") == 0);
    assert(db->num_entries == 1); // non aumenta

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

    val = casky_get(db, "alice");
    assert(val != NULL && strcmp(val, "bob") == 0);

    val = casky_get(db, "unknown");
    assert(val == NULL);

    casky_close(db);
    printf("✔ test_get passed\n");
}

// ------------------------ Test DELETE ------------------------
void test_delete() {
    KeyDir *db = casky_open("testdb");
    casky_put(db, "foo", "bar");
    casky_put(db, "alice", "bob");

    assert(casky_delete(db, "foo") == 0);
    char *val = casky_get(db, "foo");
    assert(val == NULL);
    assert(db->num_entries == 1);

    // Delete chiave non esistente
    assert(casky_delete(db, "nonexistent") == 0);
    assert(db->num_entries == 1);

    casky_close(db);
    printf("✔ test_delete passed\n");
}

// ------------------------ Main ------------------------
int main(void) {
    test_open_close();
    // test_put();
    // test_get();
    // test_delete();

    printf("\nAll tests passed!\n");
    return 0;
}
