#include <stdint.h>
#include <stddef.h>
#include "crc.h"

static uint32_t crc32_table[256];
static int crc32_table_initialized = 0;

static void init_crc32_table(void) {
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (int j = 0; j < 8; j++)
      c = (c & 1) ? 0xEDB88320 ^ (c >> 1) : (c >> 1);
    crc32_table[i] = c;
  }
  crc32_table_initialized = 1;
}

/**
 * casky_crc32
 *
 * Computes the CRC32 checksum of a given buffer.
 *
 * Parameters:
 *  - buf: pointer to the data buffer
 *  - len: length of the buffer in bytes
 *
 * Returns:
 *  - 32-bit CRC of the buffer
 *
 * Notes:
 *  - Uses a precomputed CRC32 table for performance
 *  - Can be called repeatedly on different buffers or combined with streaming
 */
uint32_t casky_crc32(const unsigned char *buf, size_t len) {
  if (!crc32_table_initialized)
    init_crc32_table();

  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++)
    crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
  return crc ^ 0xFFFFFFFF;
}
