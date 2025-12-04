// caskyd.c - thin TCP server for Casky (no internal locking)
// Features added: logging (levels), banner shows THREAD_SAFE, graceful shutdown,
// active client tracking with timeout.
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include "../src/casky.h"
#include "../src/utils.h"
#include "../src/version.h"

#define CASKY_PORT 5050
#define BUFFER_SIZE 4096
#define BACKLOG 32
#define SHUTDOWN_WAIT_SEC 5  // seconds to wait for clients to finish

/* Logging level */
typedef enum { LOG_DEBUG=0, LOG_INFO=1, LOG_WARN=2, LOG_ERROR=3 } log_level_t;

static log_level_t g_log_level = LOG_INFO;
static int server_fd = -1;
static volatile sig_atomic_t running = 1;
static atomic_int active_clients = 0;

/* ===== utils ===== */
static void set_log_level_from_env(void) {
  const char *env = getenv("CASKYD_LOG_LEVEL");
  if (!env) return;
  if (strcasecmp(env, "DEBUG") == 0) g_log_level = LOG_DEBUG;
  else if (strcasecmp(env, "INFO") == 0) g_log_level = LOG_INFO;
  else if (strcasecmp(env, "WARN") == 0) g_log_level = LOG_WARN;
  else if (strcasecmp(env, "ERROR") == 0) g_log_level = LOG_ERROR;
}

/* simple thread-safe logging with timestamp */
static void log_msg(log_level_t lvl, const char *fmt, ...) {
  if (lvl < g_log_level) return;
  const char *lvl_s = (lvl==LOG_DEBUG) ? "DEBUG" :
    (lvl==LOG_INFO)  ? "INFO"  :
    (lvl==LOG_WARN)  ? "WARN"  : "ERROR";
  time_t t = time(NULL);
  struct tm tm;
  localtime_r(&t, &tm);
  char ts[32];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

  va_list ap;
  va_start(ap, fmt);
  fprintf(stdout, "[%s] %s: ", ts, lvl_s);
  vfprintf(stdout, fmt, ap);
  fprintf(stdout, "\n");
  fflush(stdout);
  va_end(ap);
}

static void trim_newline(char *s) {
  size_t len = strlen(s);
  while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r')) {
    s[len-1] = '\0';
    len--;
  }
}

/* ===== signal handling ===== */
static void handle_signal(int sig) {
  (void)sig;
  running = 0;
  if (server_fd >= 0) {
    /* Closing the listening socket will interrupt accept() */
    close(server_fd);
    server_fd = -1;
  }
}

/* ===== client handling ===== */
typedef struct {
  int client_fd;
  KeyDir *db;
} client_arg_t;

static void *handle_client(void *arg) {
  client_arg_t *carg = (client_arg_t*)arg;
  int client_fd = carg->client_fd;
  KeyDir *db = carg->db;
  free(carg);

  atomic_fetch_add(&active_clients, 1);
  FILE *client = fdopen(client_fd, "r+");
  if (!client) {
    close(client_fd);
    atomic_fetch_sub(&active_clients, 1);
    return NULL;
  }

  char line[BUFFER_SIZE];
#ifdef THREAD_SAFE
  const char *ts = " (thread-safe)";
#else
  const char *ts = "";
#endif
  /* banner */
  fprintf(client, "CASKY %s READY%s\n", casky_version(), ts);
  fflush(client);

  while (fgets(line, sizeof(line), client)) {
    trim_newline(line);
    if (line[0] == '\0') {
      fprintf(client, "ERROR invalid command\n");
      fflush(client);
      continue;
    }

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
    if (strcasecmp(cmd, "VER") == 0) {
#ifdef THREAD_SAFE
      const char *ts = "(thread-safe)";
#else
      const char *ts = "";
#endif
      fprintf(client, "%s %s\n", casky_version(), ts);
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
        if (ret == 0) {
          fprintf(client, "OK\n");
          log_msg(LOG_DEBUG, "PUT key='%s' ok", key);
        } else {
          fprintf(client, "ERROR %d\n", casky_errno);
          log_msg(LOG_WARN, "PUT key='%s' failed err=%d", key, casky_errno);
        }
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
          log_msg(LOG_DEBUG, "GET key='%s' hit", key);
        } else {
          fprintf(client, "NOT_FOUND\n");
          log_msg(LOG_DEBUG, "GET key='%s' miss", key);
        }
      }
    }
    else if (strcasecmp(cmd, "DEL") == 0) {
      if (n < 2) {
        fprintf(client, "ERROR usage: DEL <key>\n");
      } else {
        int ret = casky_delete(db, key);
        if (ret == 0) {
          fprintf(client, "OK\n");
          log_msg(LOG_DEBUG, "DEL key='%s' ok", key);
        } else {
          fprintf(client, "NOT_FOUND\n");
          log_msg(LOG_DEBUG, "DEL key='%s' not found", key);
        }
      }
    }
    else if (strcasecmp(cmd, "COMPACT") == 0) {
      /* expose compaction via server command */
#ifdef THREAD_SAFE
      log_msg(LOG_INFO, "COMPACT requested by client");
      int cret = casky_compact(db);
      if (cret == 0) fprintf(client, "OK\n");
      else fprintf(client, "ERROR %d\n", casky_errno);
#else
      fprintf(client, "ERROR not supported (compile-with -DTHREAD_SAFE to allow COMPACT)\n");
#endif
    }
    else if (strcasecmp(cmd, "STATS") == 0) {
      casky_stat_t stats = casky_stats_get();
      fprintf(client, "STATS\n total keys=%zu\n total gets=%zu\n total puts=%zu\n total deletes=%zu\n occupied memory=%zu\n", 
              stats.total_keys,
              stats.num_gets,
              stats.num_puts,
              stats.num_deletes,
              stats.memory_bytes);
    }
    else {
      fprintf(client, "ERROR unknown command\n");
    }

    fflush(client);
  }

  fclose(client); /* closes client_fd */
  atomic_fetch_sub(&active_clients, 1);
  return NULL;
}

/* ===== server main ===== */
int main(void) {
  set_log_level_from_env();
  log_msg(LOG_INFO, "caskyd starting (pid=%d)", getpid());

  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  /* open DB */
  KeyDir *db = casky_open("caskyd.db");
  if (!db) {
    log_msg(LOG_ERROR, "failed to open database");
    return EXIT_FAILURE;
  }

  /* install signal handlers */
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_signal;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  /* create listening socket */
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    log_msg(LOG_ERROR, "socket() failed");
    casky_close(db);
    return EXIT_FAILURE;
  }

  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    log_msg(LOG_WARN, "setsockopt(SO_REUSEADDR) failed");
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(CASKY_PORT);

  if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    log_msg(LOG_ERROR, "bind() failed");
    close(server_fd);
    casky_close(db);
    return EXIT_FAILURE;
  }

  if (listen(server_fd, BACKLOG) < 0) {
    log_msg(LOG_ERROR, "listen() failed");
    close(server_fd);
    casky_close(db);
    return EXIT_FAILURE;
  }

#ifdef THREAD_SAFE
  log_msg(LOG_INFO, "caskyd listening on port %d (thread-safe build)", CASKY_PORT);
#else
  log_msg(LOG_INFO, "caskyd listening on port %d (paper-compatible build)", CASKY_PORT);
#endif

  /* accept loop */
  while (running) {
    int client_fd = accept(server_fd, (struct sockaddr*)&addr, &addrlen);
    if (client_fd < 0) {
      if (!running) break; /* interrupted by signal, exiting */
      log_msg(LOG_WARN, "accept() failed (errno=%d)", errno);
      continue;
    }

    log_msg(LOG_INFO, "client connected (fd=%d)", client_fd);

    client_arg_t *carg = malloc(sizeof(client_arg_t));
    if (!carg) {
      log_msg(LOG_WARN, "malloc client_arg failed");
      close(client_fd);
      continue;
    }
    carg->client_fd = client_fd;
    carg->db = db;

    pthread_t tid;
    if (pthread_create(&tid, NULL, handle_client, carg) != 0) {
      log_msg(LOG_WARN, "pthread_create failed");
      close(client_fd);
      free(carg);
      continue;
    }
    pthread_detach(tid);
  }

  /* shutdown sequence */
  log_msg(LOG_INFO, "shutdown requested, waiting up to %d seconds for clients...", SHUTDOWN_WAIT_SEC);
  for (int i = 0; i < SHUTDOWN_WAIT_SEC * 10; ++i) {
    int ac = atomic_load(&active_clients);
    if (ac == 0) break;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100 * 1000 * 1000; // 100ms in nanosecondi
    nanosleep(&ts, NULL);
  }
  log_msg(LOG_INFO, "active clients remaining: %d", atomic_load(&active_clients));

  /* close listening socket if still open */
  if (server_fd >= 0) {
    close(server_fd);
    server_fd = -1;
  }

  /* close DB */
  casky_close(db);
  log_msg(LOG_INFO, "caskyd stopped");
  return 0;
}
