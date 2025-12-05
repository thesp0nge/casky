#ifndef __UTILS_H
#define __UTILS_H

typedef struct {
    uint64_t total_keys;
    uint64_t memory_bytes;
    uint64_t num_puts;
    uint64_t num_gets;
    uint64_t num_deletes;
#ifdef THREAD_SAFE
    pthread_mutex_t lock;
#endif
} casky_stat_t;

const char*   casky_strerror(CaskyError err);
int           casky_is_regular_file(const char *path);
unsigned long casky_djb2_hash_xor(unsigned char *str);
int           casky_write_data_to_file(FILE *fp, int sync_on_write, const char *key, const char *value, uint64_t timestamp, uint64_t expires);
void          casky_put_in_memory(KeyDir *kd, const char *key, const char *value, uint64_t timestamp, uint64_t expires);
int           casky_delete_from_memory(KeyDir *kd, const char *key);
char*         casky_get_from_memory(KeyDir *kd, const char *key);

void casky_flush_log(KeyDir *kd);

void casky_stats_init();
void casky_stats_inc_put(size_t bytes);
void casky_stats_inc_delete(size_t bytes);
casky_stat_t casky_stats_get(void);
void casky_stats_inc_entries(void);
void casky_stats_dec_entries(void);
void casky_stats_inc_get(void);

int casky_do_snapshot(KeyDir *kd, const char *snapshot_file);
KeyDir *casky_load_snapshot(const char *snapshot_file);

// int casky_do_incremental_backup(KeyDir *kd,
//                                 const char *snapshot_file,
//                                 const char *incremental_file);
// int casky_apply_incremental(KeyDir *kd, const char *incremental_file);
// int casky_check_snapshot(const char *snapshot_file);
// uint64_t casky_get_last_snapshot_timestamp(const char *snapshot_file);
// int casky_update_snapshot_timestamp(const char *snapshot_file, uint64_t ts);
#endif // !__UTILS_H
