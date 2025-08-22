//
// Created by raven on 19/08/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "listener.h"

/**
 * @brief Initialises TCP IPv4 listener socket on the given port, currently binds to all available interfaces
 * 
 * @param port port to listen on
 * @param queue max pending connections in backlog
 * @return int - file descriptor for listener socket
 */
int init_tcp_listener(const char *ip, int port, int queue) {
    int listen_fd;
    struct sockaddr_in listen_addr;
    const int opt = 1;

    // AF_INET = IPv4
    // SOCK_STREAM = TCP
    // 0 = IPPROTO_TCP (implied)
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        // any negative int = error/unexpected behaviour
        fprintf(stderr, "TCP IPv4 socket failed on %s:%d with queue %d\n", ip ? ip : "127.0.0.1", port, queue);
        exit(EXIT_FAILURE);
    }

    // allow reuse of socket
    // not secure in prod, TIME_WAIT should be respected
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        fprintf(stderr, "Trying to reuse socket failed on %s:%d, queue %d\n", ip ? ip : "127.0.0.1", port, queue);
        exit(EXIT_FAILURE);
    }

    listen_addr.sin_family = AF_INET;
    // don't go for all interfaces unnecessarily
    //listen_addr.sin_addr.s_addr = INADDR_ANY; // all interfaces
    listen_addr.sin_port = htons(port); // convert to network byte order

    if (ip == NULL || strlen(ip) == 0) {
        ip = "127.0.0.1"; // default to localhost
    }

    if (inet_pton(AF_INET, ip, &listen_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", ip);
        exit(EXIT_FAILURE);
    }

    // bind to addr and port
    if (bind(listen_fd, (struct sockaddr *) &listen_addr, sizeof(listen_addr)) < 0) {
        fprintf(stderr, "Bind failed for %s:%d\n", ip, port);
        exit(EXIT_FAILURE);
    }

    // passive wait
    if (listen(listen_fd, queue) < 0) {
        fprintf(stderr, "Trying to listen on %s:%d failed!", ip, port);
        exit(EXIT_FAILURE);
    }

    printf("Proxy is listening on %s:%d\n", ip, port);

    return listen_fd;
}
