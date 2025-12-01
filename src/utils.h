#ifndef __UTILS_H
#define __UTILS_H

const char* casky_strerror(CaskyError err);
int casky_is_regular_file(const char *path);
unsigned long casky_djb2_hash_xor(unsigned char *str);



#endif // !__UTILS_H
