#ifndef __UTILS_H
#define __UTILS_H

const char*   casky_strerror(CaskyError err);
int           casky_is_regular_file(const char *path);
unsigned long casky_djb2_hash_xor(unsigned char *str);
int           casky_write_data_to_file(const char *logfile, int sync_on_write, const char *key, const char *value, uint32_t timestamp);
void          casky_put_in_memory(KeyDir *kd, const char *key, const char *value, uint32_t timestamp);
int           casky_delete_from_memory(KeyDir *kd, const char *key);

#endif // !__UTILS_H
