//
// Created by Raven on 18/08/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "main.h"
#include "ctmp.h"

int main() {
    int src_listen_fd, src_client_fd;
    struct sockaddr_in src_listen_addr;
    const int opt = 1;

    // AF_INET = IPv4
    // SOCK_STREAM = TCP
    // 0 = IPPROTO_TCP (implied)
    if ((src_listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        // any negative int = error/unexpected behaviour
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // allow reuse of socket
    // not secure in prod, TIME_WAIT should be respected
    if (setsockopt(src_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    src_listen_addr.sin_family = AF_INET;
    src_listen_addr.sin_addr.s_addr = INADDR_ANY; // all interfaces
    src_listen_addr.sin_port = htons(PORT); // convert to network byte order

    // bind to addr and port
    if (bind(src_listen_fd, (struct sockaddr *) &src_listen_addr, sizeof(src_listen_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // passive, wait for src, max 1 src
    if (listen(src_listen_fd, 1) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Proxy is listening on port %d\n", PORT);
    printf("Waiting for a connection...\n");

    // accept incoming
    // blocking until connect
    if ((src_client_fd = accept(src_listen_fd, NULL, NULL)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    printf("Connection accepted!\n");

    ctmp_header header;
    // read
    //ssize_t bytes_read = read(src_fd, buffer, BUFFER_SIZE);
    ssize_t bytes_read = read(src_client_fd, &header, sizeof(ctmp_header));   // for robustness, need to loop read realistically, for now just drop if we don't get sufficient bytes for simplicity
    if (bytes_read > 0) {
        if (bytes_read != sizeof(ctmp_header)) {
            perror("header read failed (insufficient bytes)");
        } else {
            int validity = is_header_valid(&header);

            printf("MAGIC: 0x%02X\n", header.magic);
            printf("PADDING_A: 0x%02X\n", header.mpad);
            printf("LENGTH: %hu\n", header.length);
            printf("PADDING_B: 0x%08X\n", header.lpad);
            printf("VALIDITY: %d\n", validity);
        }

        char buffer[header.length + 1]; // +1 to allow null terminator for now to avoid unsafety with string funcs

        ssize_t data_read =  read(src_client_fd, &buffer, header.length);

        buffer[data_read] = '\0';
        printf("Read from source (%zd) bytes: %s\n", data_read, buffer);
    } else if (bytes_read == 0) {
        printf("Source disconnected!\n");
    } else {
        perror("read");
    }

    // close connections
    close(src_client_fd);
    close(src_listen_fd);

    printf("Proxy exiting...\n");
    return 0;
}
