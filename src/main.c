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

    int dst_listen_fd = init_tcp_listener(DST_PORT, MAX_DSTS);  // set max_dsts here (at the very least, for now) so that in case MAX_DSTS all connect almost instantaneously
                                                                         // if left at 1 like src, a flood of dst connections can cause a race condition between accept and the kernel whereby some queued connections never wakeup accept to unblock it
                                                                         // todo is look at select, poll, epoll
    int dst_client_fds[MAX_DSTS];
    int num_dsts = 0;
    char buffer[BUFFER_SIZE + 1]; // allow null terminator

    printf("Waiting for %d destination connections...\n", MAX_DSTS);

    while (num_dsts < MAX_DSTS) {
        int new_dst = accept(dst_listen_fd, NULL, NULL);
        printf("Return value of accept was %d for dst %d", new_dst, num_dsts+1);
        if (new_dst < 0) {
            printf("Failed to accept connection from dst %d\n", num_dsts+1);
            perror("failed to accept connection");
            continue;
        }

        printf("Accepted new destination client #%d (fd: %d)\n", num_dsts + 1, new_dst);
        dst_client_fds[num_dsts] = new_dst;
        num_dsts++;
    }

    printf("%d destination connections accepted! Waiting on the source connection...\n", num_dsts);


    /**
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
    **/
    // accept src
    // blocking until connect
    int src_listen_fd = init_tcp_listener(SRC_PORT, 1);
    int src_client_fd;
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
            }
            int validity = is_header_valid(&header);
            if (validity != 1) {
                perror("header validity failed");  // for now, also exit
                exit(EXIT_FAILURE);
            }

            printf("MAGIC: 0x%02X\n", header.magic);
            printf("PADDING_A: 0x%02X\n", header.mpad);
            printf("LENGTH: %hu\n", header.length);
            printf("PADDING_B: 0x%08X\n", header.lpad);
            printf("VALIDITY: %d\n", validity);

            ssize_t data_read =  read(src_client_fd, buffer, header.length);
            if (data_read >= 0) {  // possibly check against 0 byte sends before writing data - protocol doesn't specify they're disallowed, but it is redundant
                if (data_read != header.length) {  // so currently, we don't read more than the length specified in the header
                                                    // the spec explicitly says to drop the message if the data exceeds the length specified - strat? suspect possibly read up to header.length + 1 - if it exceeds we don't need to care *how much* it exceeds by, ie 1 byte over is sufficient to tell
                                                    // spec doesn't say about dropping msgs that are *smaller* than the specified length - don't currently get dropped anyway by virtue of single read
                    printf("warn: length specified as %hu bytes, read only %zd bytes\n", header.length, data_read);
                }
                buffer[data_read] = '\0';
                printf("Read from source (%zd) bytes: %s\n", data_read, buffer);

                printf("Message received, forwarding to destinations...\n");

                for (int i = 0; i < num_dsts; i++) {
                    printf("Sending to destination %d...", i+1);
                    write(dst_client_fds[i], &header, sizeof(ctmp_header));
                    write(dst_client_fds[i], buffer, data_read);
                    printf("Done\n");
                }

                //write(dst_client_fd, &header, sizeof(ctmp_header));
                //write(dst_client_fd, buffer, data_read);
                printf("Finished sending to all destinations!\n");
            } else {
                perror("read");
                exit(EXIT_FAILURE);
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
    //close(dst_client_fd);
    for (int j = 0; j < num_dsts; j++) {
        close(dst_client_fds[j]);
    }
    close(dst_listen_fd);

    printf("Proxy exiting...\n");
    return 0;
}
