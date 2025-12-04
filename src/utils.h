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
int           casky_write_data_to_file(const char *logfile, int sync_on_write, const char *key, const char *value, uint32_t timestamp);
void          casky_put_in_memory(KeyDir *kd, const char *key, const char *value, uint32_t timestamp);
int           casky_delete_from_memory(KeyDir *kd, const char *key);
char*         casky_get_from_memory(KeyDir *kd, const char *key);

void casky_stats_init();
void casky_stats_inc_put(size_t bytes);
void casky_stats_inc_delete(size_t bytes);
casky_stat_t casky_stats_get(void);
void casky_stats_inc_entries(void);
void casky_stats_dec_entries(void);
void casky_stats_inc_get(void);
#endif // !__UTILS_H
