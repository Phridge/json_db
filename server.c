#include "server.h"

#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>

#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <poll.h>

#define READ_BUF_SIZE 1024

struct {
    int fd;
    struct sockaddr_in server_sock, client_sock;
    socklen_t client_sock_len;
    struct pollfd poll_fd[1];
    
    char * buffer, * buf_ptr;
    size_t buf_cap;

    int term_request;

    struct jsondb_server_msg msg;
} jsondb_server;

int jsondb_server_init(void) {
    jsondb_server.buf_cap = 10 * 1024;
    jsondb_server.buffer = jsondb_server.buf_ptr = malloc(jsondb_server.buf_cap);

    jsondb_server.server_sock.sin_addr.s_addr = htonl(INADDR_ANY);
    jsondb_server.server_sock.sin_port = htons(3500);
    jsondb_server.server_sock.sin_family = AF_INET;

    jsondb_server.client_sock_len = sizeof(struct sockaddr_in);

    if((jsondb_server.fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("server initialization failed");
        goto error;
    }

    if (bind(jsondb_server.fd, (struct sockaddr *) &jsondb_server.server_sock, sizeof(jsondb_server.server_sock)) < 0) {
        perror("server binding failed");
        goto error;
    }

    if (listen(jsondb_server.fd, 10) < 0) {
        perror("server listening failed");
        goto error;
    }

    jsondb_server.poll_fd->fd = jsondb_server.fd;
    jsondb_server.poll_fd->events = POLLIN;

    return 0;
error:
    return -1;
}

struct jsondb_server_msg * jsondb_server_poll(int timeout_ms) {
    int client_fd;
    int amount_read;
    size_t total_read = 0;

    if(poll(jsondb_server.poll_fd, 1, timeout_ms) == 0) {
        /* then we got nothing */
        return NULL;
    }

    /* we got something */

    if((client_fd = accept(jsondb_server.fd, (struct sockaddr *)&jsondb_server.client_sock, &jsondb_server.client_sock_len)) < 0) {
        perror("client connection accept");
        goto error;
    }

    jsondb_server.buf_ptr = jsondb_server.buffer;
    
    while (1) {
        amount_read = recv(client_fd, jsondb_server.buf_ptr, jsondb_server.buf_cap - total_read, 0);
        if(amount_read < 0) {
            perror("client connection read");
            goto error;
        } else if(amount_read == 0 /*&& jsondb_server.poll_fd->revents & POLLHUP*/) {
            /* finished reading */
            close(client_fd);
            jsondb_server.msg.buf = jsondb_server.buffer;
            jsondb_server.msg.size = total_read;
            break;
        }
        

        if(jsondb_server.buf_ptr + amount_read >= jsondb_server.buffer + jsondb_server.buf_cap) {
            /* time to resize */
            jsondb_server.buf_cap *= 2;
            jsondb_server.buffer = realloc(jsondb_server.buffer, jsondb_server.buf_cap);
            jsondb_server.buf_ptr = jsondb_server.buffer + total_read;
        }

        total_read += amount_read;
        jsondb_server.buf_ptr += amount_read;
    }

    return &jsondb_server.msg;
error:
    return NULL;
}

void jsondb_server_deinit(void) {
    close(jsondb_server.fd);
    free(jsondb_server.buffer);
}