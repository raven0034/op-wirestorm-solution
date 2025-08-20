//
// Created by Raven on 18/08/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "main.h"
#include "ctmp.h"
#include "listener.h"


volatile bool on_state = true;

// derived from UNIX Network Programming Vol. 1, 3rd Edition
// src can send data in fragments, which isn't sufficiently handled by a single read
ssize_t readn(const int fd, void *buffer, const ssize_t expected_bytes) {
    ssize_t bytes_read = 0;

    while (bytes_read < expected_bytes) {
        // move forward number of bytes read from buffer, read up to the remaining number of bytes in new call
        const ssize_t curr_num_bytes = read(fd, buffer + bytes_read, expected_bytes - bytes_read);
        // store number of bytes read this iteration

        if (curr_num_bytes < 0) {
            if (errno == EINTR) {
                // just an interrupt ie continue reading
                continue;
            }

            perror("readn");
            return -1; // anything else is unrecoverable
        }

        if (curr_num_bytes == 0) {
            return bytes_read; // consistent with read
        }

        bytes_read += curr_num_bytes;
    }

    return bytes_read; // total read across all iterations - should match expected_bytes at this statement
}

ssize_t writen(const int fd, const void *buffer, const ssize_t expected_bytes) {
    ssize_t bytes_written = 0;
    while (bytes_written < expected_bytes) {
        const ssize_t curr_num_bytes = write(fd, buffer + bytes_written, expected_bytes - bytes_written);
        if (curr_num_bytes < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("writen");
            return -1;
        }

        if (curr_num_bytes == 0) {
            return bytes_written;
        }

        bytes_written += curr_num_bytes;
    }

    return bytes_written;
}


void int_handler(int sig) {
    printf("Received %d to respond to\n", sig);
    on_state = false;
}

int main() {
    int dst_listen_fd = init_tcp_listener(DST_PORT, MAX_DSTS);
    // set max_dsts here (at the very least, for now) so that in case MAX_DSTS all connect almost instantaneously
    // if left at 1 like src, a flood of dst connections can cause a race condition between accept and the kernel whereby some queued connections never wakeup accept to unblock it
    // todo is look at select, poll, epoll
    int dst_client_fds[MAX_DSTS];
    int num_dsts = 0;
    char buffer[BUFFER_SIZE + 1]; // allow null terminator

    printf("Waiting for %d destination connections...\n", MAX_DSTS);

    while (num_dsts < MAX_DSTS) {
        const int new_dst = accept(dst_listen_fd, NULL, NULL);
        printf("Return value of accept was %d for dst %d", new_dst, num_dsts + 1);
        if (new_dst < 0) {
            printf("Failed to accept connection from dst %d\n", num_dsts + 1);
            perror("failed to accept connection");
            continue;
        }

        printf("Accepted new destination client #%d (fd: %d)\n", num_dsts + 1, new_dst);
        dst_client_fds[num_dsts] = new_dst;
        num_dsts++;
    }

    printf("%d destination connections accepted! Waiting on the source connection...\n", num_dsts);

    // accept src
    // blocking until connect
    const int src_listen_fd = init_tcp_listener(SRC_PORT, 1);
    int src_client_fd;
    if ((src_client_fd = accept(src_listen_fd, NULL, NULL)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    printf("Source connection accepted! Waiting for messages from source...\n");

    ctmp_header header;

    while (on_state) {
        printf("Waiting for next message...");
        // 0 out contents so garbage data doesn't get transmitted
        // matters less for header given fixed length etc, but data read B being shorter than data read A could result in leftover data
        // garbage data bad generally in this implementation, but also potentially dangerous in future potential where new dst connects between A and B and was not intended to receive any of A
        memset(&header, 0, sizeof(header));
        memset(buffer, 0, sizeof(buffer));

        // read header from src
        const ssize_t bytes_read = readn(src_client_fd, &header, sizeof(ctmp_header));
        if (bytes_read > 0) {
            if (bytes_read != sizeof(ctmp_header)) {
                perror("header read failed (insufficient bytes)");
                exit(EXIT_FAILURE);
            }

            const int validity = is_header_valid(&header);
            if (validity != 1) {
                // this should cover most short read cases
                // msg A claims 64 bytes, sends 56 bytes, some msg B of size >= 8 bytes fills out msg A, msg B gets detected as having a protocol violation --> kill
                // two things are not ideal though:
                //   - not sure there's a reasonable way to avoid transmitting a "bad" msg A before realising msg B is "bad"
                //      esp difficult since protocol the spec does not define a delineation of the end of a msg
                //   - there's a potential edge case in which a protocol violation is not detected until much later down the line (or perhaps not at all!)
                //      this is since proxy's msg A could finish with source msg B's header, and proxy's msg B then starts with source msg B's data with a valid CTMP header
                perror("header validity failed");
                exit(EXIT_FAILURE); // exit since there's no reasonable way to recover predictably
            }

            printf("MAGIC: 0x%02X\n", header.magic);
            printf("PADDING_A: 0x%02X\n", header.mpad);
            printf("LENGTH: %hu\n", header.length);
            printf("PADDING_B: 0x%08X\n", header.lpad);
            printf("VALIDITY: %d\n", validity);

            const ssize_t data_read = readn(src_client_fd, buffer, header.length);
            if (data_read >= 0) {
                // possibly check against 0 byte sends before writing data - protocol doesn't specify they're disallowed, but it is redundant
                if (data_read != header.length) {
                    // don't read more than specified in the header length
                    // previous length+1 assumption doesn't hold easily since it puts the proxy in a potentially unpredictable state
                    printf("warn: length specified as %hu bytes, read %zd bytes\n", header.length, data_read);
                    perror("data read failed");
                    exit(EXIT_FAILURE);  // treat as unrecoverable and exit
                }
                buffer[data_read] = '\0';
                printf("Read from source (%zd) bytes: %s\n", data_read, buffer);

                printf("Message received, forwarding to destinations...\n");

                for (int i = 0; i < num_dsts; i++) {
                    printf("Sending to destination %d...", i + 1);
                    writen(dst_client_fds[i], &header, sizeof(ctmp_header));  // handling for write fails?
                    writen(dst_client_fds[i], buffer, data_read);
                    printf("Done\n");
                }
                
                printf("Finished sending to all destinations!\n");
            } else {
                perror("read");
                exit(EXIT_FAILURE);
            }
        } else if (bytes_read == 0) {
            printf("Source disconnected!\n");
            break;
        } else {
            // readn returning -1 --> unrecoverable error
            perror("read");
            exit(EXIT_FAILURE);
        }
    }

    // close connections
    close(src_client_fd);
    close(src_listen_fd);
    for (int j = 0; j < num_dsts; j++) {
        close(dst_client_fds[j]);
    }
    close(dst_listen_fd);

    printf("Proxy exiting...\n");
    return 0;
}
