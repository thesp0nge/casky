#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../src/casky.h"

#define CASKY_PORT 5050
#define BUFFER_SIZE 4096

// Utility: rimuove newline e carriage return
static void trim_newline(char *s) {
  size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r')) {
        s[len-1] = '\0';
        len--;
    }
}

// Gestione singolo client
static void handle_client(int client_fd, KeyDir *db) {
  FILE *client = fdopen(client_fd, "r+");
  if (!client) {
    close(client_fd);
    return;
  }

  char line[BUFFER_SIZE];
  fprintf(client, "CASKY %s READY\n", casky_version());
  fflush(client);

  while (fgets(line, sizeof(line), client)) {
    trim_newline(line);

    char cmd[16], key[256], value[2048];
    memset(cmd, 0, sizeof(cmd));
    memset(key, 0, sizeof(key));
    memset(value, 0, sizeof(value));

    int n = sscanf(line, "%15s %255s %2047[^\n]", cmd, key, value);
    trim_newline(value);

    if (n <= 0) {
      fprintf(client, "ERROR invalid command\n");
      fflush(client);
      continue;
    }

    if (strcasecmp(cmd, "QUIT") == 0) {
      fprintf(client, "BYE\n");
      fflush(client);
      break;
    }
    else if (strcasecmp(cmd, "PUT") == 0) {
      if (n < 3) {
        fprintf(client, "ERROR usage: PUT <key> <value>\n");
              } else {
              int ret = casky_put(db, key, value);
              if (ret == 0)
              fprintf(client, "OK\n");
              else
              fprintf(client, "ERROR %d\n", casky_errno);
              }
              }
              else if (strcasecmp(cmd, "GET") == 0) {
              if (n < 2) {
              fprintf(client, "ERROR usage: GET <key>\n");
              } else {
              char *v = casky_get(db, key);
              if (v) {
              fprintf(client, "VALUE %s\n", v);
              free(v);
              } else {
              fprintf(client, "NOT_FOUND\n");
              }
              }
              }
              else if (strcasecmp(cmd, "DEL") == 0) {
                    if (n < 2) {
                    fprintf(client, "ERROR usage: DEL <key>\n");
                    } else {
                    int ret = casky_delete(db, key);
                    if (ret == 0)
                    fprintf(client, "OK\n");
                    else
                    fprintf(client, "NOT_FOUND\n");
                    }
                    }
                    else {
                    fprintf(client, "ERROR unknown command\n");
                    }

                    fflush(client);
                    }

                    fclose(client);  // chiude anche client_fd
                    }

                    int main(void) {
                    int server_fd, client_fd;
                    struct sockaddr_in addr;
                    socklen_t addrlen = sizeof(addr);

                    printf("[caskyd] Starting server on port %d...\n", CASKY_PORT);

                    // Apri il database
                    KeyDir *db = casky_open("caskyd.db");
                    if (!db) {
                    fprintf(stderr, "[caskyd] failed to open database\n");
                    return EXIT_FAILURE;
                    }

                    server_fd = socket(AF_INET, SOCK_STREAM, 0);
                    if (server_fd < 0) {
                    perror("[caskyd] socket");
                    casky_close(db);
                    return EXIT_FAILURE;
                    }

                    int opt = 1;
                    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

                    addr.sin_family = AF_INET;
                    addr.sin_addr.s_addr = INADDR_ANY;
                    addr.sin_port = htons(CASKY_PORT);

                    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                    perror("[caskyd] bind");
                    casky_close(db);
                    close(server_fd);
                    return EXIT_FAILURE;
                    }

                    if (listen(server_fd, 16) < 0) {
                    perror("[caskyd] listen");
                    casky_close(db);
                    close(server_fd);
                    return EXIT_FAILURE;
                    }

                    printf("[caskyd] Listening...\n");

                    while (1) {
                    client_fd = accept(server_fd, (struct sockaddr*)&addr, &addrlen);
                    if (client_fd < 0) {
                    perror("[caskyd] accept");
                    continue;
                    }

                    printf("[caskyd] Client connected\n");
                    handle_client(client_fd, db);
                    }

                    casky_close(db);
                    close(server_fd);
                    return 0;
                    }
