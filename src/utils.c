#include <sys/stat.h>
#include "casky.h"
/**
 * Checks whether the given path refers to a regular file.
 *
 * Parameters:
 *   path - a null-terminated string containing the filesystem path
 *
 * Returns:
 *   1 if the path exists and is a regular file
 *   0 otherwise (path does not exist, cannot be accessed, or is not a regular file)
 */
int casky_is_regular_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0)
        return 0; 
    if (S_ISREG(st.st_mode))
        return 1;
    return 0;
}

/**
 * casky_strerror - Returns a human-readable string for a Casky error code.
 *
 * @err: The CaskyError value to translate.
 *
 * This function takes an error code returned by any Casky API function
 * and returns a descriptive string suitable for logging or displaying
 * to the user. It helps applications using Casky to interpret errors
 * without having to maintain their own mapping of error codes.
 *
 * Returns: A constant null-terminated string describing the error.
 */
const char* casky_strerror(CaskyError err) {
  switch (err) {
    case CASKY_OK: return "OK";
    case CASKY_ERR_INVALID_PATH: return "Invalid path";
    case CASKY_ERR_INVALID_POINTER: return "Invalid pointer";  
    case CASKY_ERR_IO: return "I/O error";
    case CASKY_ERR_MEMORY: return "Out of memory";
    case CASKY_ERR_CORRUPT: return "Data corrupt";
    case CASKY_ERR_INVALID_KEY: return "Invalid key";
    case CASKY_ERR_KEY_NOT_FOUND: return "Key not found";
    default: return "Unknown error";
  }
}

/**
 * djb2 hash function (XOR variant)
 *
 * Computes a hash value for a string using the magic constant 33 
 * and XOR operation. This variant is widely used for hash tables 
 * and has proven effective in practice.
 *
 * Formula:
 *     hash(i) = hash(i-1) * 33 ^ str[i]
 *
 * @str: Input string to hash
 *
 * Returns: Computed hash value as an unsigned long
 */
unsigned long casky_djb2_hash_xor(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = (hash * 33) ^ c;  // XOR variant
    }

    return hash;
}
