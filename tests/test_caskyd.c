#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <assert.h>

#define SERVER_PORT 5050
#define BUFFER_SIZE 4096

// Utility: legge una linea dal socket (terminata da \n)
static int read_line(int sock, char *buf, size_t size) {
  size_t pos = 0;
  while (pos < size - 1) {
    char c;
    ssize_t n = read(sock, &c, 1);
    if (n <= 0) break;
    if (c == '\n') break;
    buf[pos++] = c;
  }
  buf[pos] = '\0';
  return pos;
}

// Utility: invia un comando e legge la risposta
static void send_cmd(int sock, const char *cmd, char *resp, size_t resp_size) {
  write(sock, cmd, strlen(cmd));
  write(sock, "\n", 1);
  read_line(sock, resp, resp_size);
}

int main(void) {
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    execl("./build/caskyd", "caskyd", NULL);
    perror("execl");
    exit(1);
  }

  sleep(1);

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  assert(sock >= 0);

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(SERVER_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  int ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
  assert(ret == 0);

  char buf[BUFFER_SIZE];

  read_line(sock, buf, sizeof(buf));
  printf("Server: %s\n", buf);
  assert(strncmp(buf, "CASKY", 5) == 0);

  send_cmd(sock, "PUT foo bar", buf, sizeof(buf));
  assert(strcmp(buf, "OK") == 0);

  send_cmd(sock, "GET foo", buf, sizeof(buf));
  assert(strcmp(buf, "VALUE bar") == 0);

  send_cmd(sock, "GET unknown", buf, sizeof(buf));
  assert(strcmp(buf, "NOT_FOUND") == 0);

  send_cmd(sock, "DEL foo", buf, sizeof(buf));
  assert(strcmp(buf, "OK") == 0);

  send_cmd(sock, "DEL foo", buf, sizeof(buf));
  assert(strcmp(buf, "NOT_FOUND") == 0);

  send_cmd(sock, "FOO bar", buf, sizeof(buf));
  assert(strcmp(buf, "ERROR unknown command") == 0);

  send_cmd(sock, "PUT key_only", buf, sizeof(buf));
  assert(strncmp(buf, "ERROR usage", 11) == 0);

  send_cmd(sock, "QUIT", buf, sizeof(buf));
  assert(strcmp(buf, "BYE") == 0);

  close(sock);

  kill(pid, SIGTERM);
  waitpid(pid, NULL, 0);

  printf("✔ test_caskyd passed\n");
  return 0;
}
