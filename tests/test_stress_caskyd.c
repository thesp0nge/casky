#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include "../src/casky.h"

#define CASKY_PORT 5050
#define BUFFER_SIZE 4096
#define NUM_CLIENTS 5
#define OPS_PER_CLIENT 10

typedef struct {
    int client_id;
    double put_time;
    double get_time;
    double del_time;
} client_arg_t;

// Trim newline and carriage return
static void trim_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r')) {
        s[len-1] = '\0';
        len--;
    }
}

// Send a single command and get the first response line
static int send_command(const char *cmd, char *reply, size_t reply_size) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) return -1;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CASKY_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock_fd);
        return -1;
    }

    FILE *sock_file = fdopen(sock_fd, "r+");
    if (!sock_file) {
        close(sock_fd);
        return -1;
    }

    // Read welcome message
    if (fgets(reply, reply_size, sock_file) == NULL) {
        fclose(sock_file);
        return -1;
    }
    trim_newline(reply);

    // Send command
    fprintf(sock_file, "%s\n", cmd);
    fflush(sock_file);

    // Read first line of response
    if (fgets(reply, reply_size, sock_file) == NULL) {
        fclose(sock_file);
        return -1;
    }
    trim_newline(reply);

    fclose(sock_file);
    return 0;
}

// Client thread: PUT, GET, DEL
void *client_thread(void *arg) {
    client_arg_t *carg = (client_arg_t*)arg;
    char cmd[256], reply[BUFFER_SIZE];
    struct timespec start, end;

    carg->put_time = carg->get_time = carg->del_time = 0.0;

    for (int i = 0; i < OPS_PER_CLIENT; i++) {
        snprintf(cmd, sizeof(cmd), "PUT key%d_%d value%d_%d", carg->client_id, i, carg->client_id, i);
        clock_gettime(CLOCK_MONOTONIC, &start);
        int ok = send_command(cmd, reply, sizeof(reply));
        clock_gettime(CLOCK_MONOTONIC, &end);
        carg->put_time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9;
        if (ok != 0 || strncmp(reply, "OK", 2) != 0) {
            fprintf(stderr, "[client %d] PUT failed: '%s'\n", carg->client_id, reply);
            exit(1);
        }

        snprintf(cmd, sizeof(cmd), "GET key%d_%d", carg->client_id, i);
        clock_gettime(CLOCK_MONOTONIC, &start);
        ok = send_command(cmd, reply, sizeof(reply));
        clock_gettime(CLOCK_MONOTONIC, &end);
        carg->get_time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9;
        if (ok != 0 || strncmp(reply, "VALUE", 5) != 0) {
            fprintf(stderr, "[client %d] GET failed: '%s'\n", carg->client_id, reply);
            exit(1);
        }
    }

    for (int i = 0; i < OPS_PER_CLIENT/2; i++) {
        snprintf(cmd, sizeof(cmd), "DEL key%d_%d", carg->client_id, i);
        clock_gettime(CLOCK_MONOTONIC, &start);
        int ok = send_command(cmd, reply, sizeof(reply));
        clock_gettime(CLOCK_MONOTONIC, &end);
        carg->del_time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9;
        if (ok != 0 || strncmp(reply, "OK", 2) != 0) {
            fprintf(stderr, "[client %d] DEL failed: '%s'\n", carg->client_id, reply);
            exit(1);
        }
    }

    return NULL;
}

// Start server in background
pid_t start_server() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("./build/caskyd", "./build/caskyd", NULL);
        perror("execl");
        exit(1);
    }
    // parent returns child pid
    sleep(1); // give server time to start
    return pid;
}

int main(void) {
    printf("[test_stress_caskyd] Starting server...\n");
#ifdef THREAD_SAFE
    pid_t server_pid = start_server();

    pthread_t threads[NUM_CLIENTS];
    client_arg_t args[NUM_CLIENTS];

    for (int i = 0; i < NUM_CLIENTS; i++) {
        args[i].client_id = i;
        pthread_create(&threads[i], NULL, client_thread, &args[i]);
    }

    for (int i = 0; i < NUM_CLIENTS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("✔ test_stress_caskyd passed\n");

    // Stop server
    kill(server_pid, SIGTERM);
    waitpid(server_pid, NULL, 0);
#else
    printf("✔ test_stress_caskyd skipped. casky is not compiled with thread safeness\n");
#endif
    return 0;
}
