#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "crc.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <logfile>\n", argv[0]);
        return 1;
    }

    const char *logfile = argv[1];
    FILE *f = fopen(logfile, "rb");
    if (!f) {
        perror("Failed to open log file");
        return 1;
    }

    printf("Debug log file: %s\n", logfile);

    while (1) {
        uint32_t crc_stored, timestamp, key_len, value_len;

        if (fread(&crc_stored, sizeof(crc_stored), 1, f) != 1) break;
        if (fread(&timestamp, sizeof(timestamp), 1, f) != 1) break;
        if (fread(&key_len, sizeof(key_len), 1, f) != 1) break;
        if (fread(&value_len, sizeof(value_len), 1, f) != 1) break;

        char *key = malloc(key_len + 1);
        char *value = malloc(value_len + 1);

        if (fread(key, 1, key_len, f) != key_len) break;
        key[key_len] = '\0';

        if (value_len > 0) {
            if (fread(value, 1, value_len, f) != value_len) break;
            value[value_len] = '\0';
        } else {
            value[0] = '\0';
        }

        // compute CRC over [timestamp][key_len][value_len][key][value]
        size_t buf_len = sizeof(timestamp) + sizeof(key_len) + sizeof(value_len) + key_len + value_len;
        unsigned char *buf = malloc(buf_len);
        unsigned char *p = buf;
        memcpy(p, &timestamp, sizeof(timestamp)); p += sizeof(timestamp);
        memcpy(p, &key_len, sizeof(key_len)); p += sizeof(key_len);
        memcpy(p, &value_len, sizeof(value_len)); p += sizeof(value_len);
        memcpy(p, key, key_len); p += key_len;
        if (value_len > 0) memcpy(p, value, value_len);

        uint32_t crc_calc = casky_crc32(buf, buf_len);
        free(buf);

        printf("Record: CRC=0x%08X%s, TS=%u, Key='%s', Value='%s'\n",
               crc_stored,
               (crc_stored != crc_calc) ? " [CRC MISMATCH]" : "",
               timestamp,
               key,
               value);

        free(key);
        free(value);
    }

    fclose(f);
    return 0;
}

