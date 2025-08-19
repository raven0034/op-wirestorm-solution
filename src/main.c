//
// Created by Raven on 18/08/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include "main.h"

#include <stdbool.h>
#include <string.h>

#include "ctmp.h"
#include "listener.h"


volatile bool on_state = true;

void int_handler(int sig) {
    printf("Received %d to respond to\n", sig);
    on_state = false;
}


int main() {

    int src_listen_fd = init_tcp_listener(SRC_PORT);
    int src_client_fd;

    int dst_listen_fd = init_tcp_listener(DST_PORT);
    int dst_client_fd;

    char buffer[BUFFER_SIZE + 1]; // allow null terminator

    // accept dst - do first to make sure we actually have a dst before src sends anything
    // also sets up for next step - "connection phase" whereby some configuration and/or user input allows dst connections up to a point, then move on to get the source
    // from there can look at more dynamic options
    // block until connect
    printf("Waiting for a destination connection...\n");
    if ((dst_client_fd = accept(dst_listen_fd, NULL, NULL)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    printf("Destination connection accepted! Waiting on the source connection...\n");

    // accept src
    // blocking until connect
    if ((src_client_fd = accept(src_listen_fd, NULL, NULL)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    printf("Source connection accepted! Waiting for messages from source...\n");

    ctmp_header header;
    //ssize_t bytes_read = read(src_fd, buffer, BUFFER_SIZE);

    while (on_state) {
        printf("Waiting for next message...");
        // 0 out contents so garbage data doesn't get transmitted
        // matters less for header given fixed length etc, but data read B being shorter than data read A could result in leftover data
        // garbage data bad generally in this implementation, but also potentially dangerous in future potential where new dst connects between A and B and was not intended to receive any of A
        memset(&header, 0, sizeof(header));
        memset(buffer, 0, sizeof(buffer));

        // read header from src
        ssize_t bytes_read = read(src_client_fd, &header, sizeof(ctmp_header));   // for robustness, need to loop read realistically, for now just drop if we don't get sufficient bytes for simplicity
        if (bytes_read > 0) {
            if (bytes_read != sizeof(ctmp_header)) {
                perror("header read failed (insufficient bytes)");  // can't strictly rely on one read, ie later abstract out, for now just exit, simpler
                exit(EXIT_FAILURE);
            } else {
                int validity = is_header_valid(&header);

                printf("MAGIC: 0x%02X\n", header.magic);
                printf("PADDING_A: 0x%02X\n", header.mpad);
                printf("LENGTH: %hu\n", header.length);
                printf("PADDING_B: 0x%08X\n", header.lpad);
                printf("VALIDITY: %d\n", validity);

                ssize_t data_read =  read(src_client_fd, buffer, header.length);
                if (data_read >= 0) {  // possibly check against 0 byte sends before writing data - protocol doesn't specify they're disallowed, but it is redundant
                    if (data_read != header.length) {
                        printf("warn: length specified as %hu bytes, read only %zd bytes", header.length, data_read);
                    }
                    buffer[data_read] = '\0';
                    printf("Read from source (%zd) bytes: %s\n", data_read, buffer);

                    printf("Message received, forwarding to destination...");
                    write(dst_client_fd, &header, sizeof(ctmp_header));
                    write(dst_client_fd, buffer, data_read);
                    printf("Done!\n");
                } else {
                    perror("read");
                    exit(EXIT_FAILURE);
                }
            }

        } else if (bytes_read == 0) {
            printf("Source disconnected!\n");
            break;
        } else {
            perror("read");
        }
    }

    // close connections
    close(src_client_fd);
    close(src_listen_fd);
    close(dst_client_fd);
    close(dst_listen_fd);

    printf("Proxy exiting...\n");
    return 0;
}
