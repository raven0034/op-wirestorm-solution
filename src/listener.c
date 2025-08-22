//
// Created by raven on 19/08/2025.
//

#include "listener.h"

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>


int init_tcp_listener(int port, int queue) {
    printf("INFO: init_tcp_listener for port %d\n", port);
    int listen_fd;
    struct sockaddr_in listen_addr;
    const int opt = 1;

    // AF_INET = IPv4
    // SOCK_STREAM = TCP
    // 0 = IPPROTO_TCP (implied)
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        // any negative int = error/unexpected behaviour
        fprintf(stderr, "TCP IPv4 socket failed on port %d with queue %d\n", port, queue);
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // allow reuse of socket
    // not secure in prod, TIME_WAIT should be respected
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        fprintf(stderr, "Trying to reuse socket failed on port %d, queue %d\n", port, queue);
        exit(EXIT_FAILURE);
    }

    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY; // all interfaces
    listen_addr.sin_port = htons(port); // convert to network byte order

    // bind to addr and port
    if (bind(listen_fd, (struct sockaddr *) &listen_addr, sizeof(listen_addr)) < 0) {
        fprintf(stderr, "Bind failed for port %d\n", port);
        exit(EXIT_FAILURE);
    }

    // passive wait
    if (listen(listen_fd, queue) < 0) {
        fprintf(stderr, "Trying to listen on port %d failed!", port);
        exit(EXIT_FAILURE);
    }

    printf("Proxy is listening on port %d\n", port);

    return listen_fd;
}
