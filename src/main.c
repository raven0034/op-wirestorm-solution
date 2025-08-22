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
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>
#include <netinet/in.h>
#include "main.h"
#include "ctmp.h"
#include "listener.h"
#include "client.h"


volatile bool on_state = true;
src_client src = {.fd = -1};  // file descriptor -1 indicates no connection
dst_client dsts[MAX_DSTS];  // MAX_DSTS set in main.h - reject dsts in excess of this

/**
 * @brief Close a source client connection and deregister with epoll
 *
 * @param epoll_fd epoll file descriptor
 * @param src source client to cleanup
 */
void close_src_client(int epoll_fd, src_client *src) {
    if (src->fd != -1) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, src->fd, NULL);
        close(src->fd);
        *src = (src_client){0};  // 0 out for safety
        src->fd = -1;
    }
}

/**
 * @brief Close a destination client connection and deregister with epoll
 *
 * @param epoll_fd epoll file descriptor
 * @param dst destination client to cleanup
 */
void close_dst_client(int epoll_fd, dst_client *dst) {
    if (dst->fd != -1) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, dst->fd, NULL);
        close(dst->fd);
        *dst = (dst_client){0};
        dst->fd = -1;
    }
}

/**
 * @brief Cleanly handles shutdown
 * 
 * @param sig The signal received - for us SIGINT since CTRL-C
 */
void int_handler(int sig) {
    printf("\nReceived %d to respond to\n", sig);
    on_state = false;
}

int main() {
    signal(SIGINT, int_handler); // handle ctrl-c

    for (int i = 0; i < MAX_DSTS; i++) {
        dsts[i].fd = -1; // initialise each destination
    }

    int src_listen_fd = init_tcp_listener(SRC_PORT, 128);  // listen on :33333 or other specified port
    // prev assumption doesn't work given we could get flooded with *bad* src connections - ie don't want kernel to reject legit src
    // similar problem for small MAX_DSTS
    int dst_listen_fd = init_tcp_listener(DST_PORT, 128);  // listen on :44444
    set_non_block(src_listen_fd);  // changed to non-blocking so we can poll rather than waiting and doing things sequentially
    set_non_block(dst_listen_fd);

    int epoll_fd = epoll_create1(0);  // file descriptor for polling
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event, events[MAX_DSTS + 3];  // capacity for MAX_DSTS destinations, 1 source, plus the 2 listeners
    event.events = EPOLLIN;  // initially only care about reading - with no read we have no write

    event.data.ptr = &src_listen_fd;  // register src listener socket with epoll - fd readable -> incoming connection
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, src_listen_fd, &event);

    event.data.ptr = &dst_listen_fd;  // same with dst
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, dst_listen_fd, &event);

    printf("Proxy started, waiting for events...\n");

    while (on_state) {
        int num_events = epoll_wait(epoll_fd, events, MAX_DSTS + 3, 20);  // wait for new events, block for up to a reasonable time
                                                                        // don't infinitely block so int_handler has an effect consistently

        if (num_events < 0 && errno != EINTR) {
            fprintf(stderr, "Unrecoverable error whilst polling: %s\n", strerror(errno));
            perror("epoll_wait");
            break; // unrecoverable state, exit, but cleanly
        }

        for (int i = 0; i < num_events; i++) {
            void *curr_fd_ptr = events[i].data.ptr;

            // events for connections the src listener needs to handle
            if (curr_fd_ptr == &src_listen_fd) {
                while (true) {
                    int src_fd = accept(src_listen_fd, NULL, NULL);

                    if (src_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;  // no more pending
                        }
                        
                        fprintf(stderr, "Error accepting source connection: %s\n", strerror(errno));
                        break;  // so we don't spin forever on errors
                    }

                    if (src.fd != -1) {
                        fprintf(stderr, "Rejecting attempted source connection on fd %d (already have a connected source)\n", src_fd);

                        close(src_fd);

                    } else {
                        set_non_block(src_fd);

                        event = (struct epoll_event){0};
                        event.events = EPOLLIN;
                        event.data.ptr = &src;

                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, src_fd, &event);
                        src = (src_client){.fd = src_fd};

                        printf("Accepted new source client on fd %d\n", src_fd);
                    }
                }
            
            // events for connections the dst listener needs to handle    
            } else if (curr_fd_ptr == &dst_listen_fd) {
                while (true) {
                    int dst_fd = accept(dst_listen_fd, NULL, NULL);
                    if (dst_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {  // no more connections to accept, non-fatal
                            break;
                        }

                        fprintf(stderr, "Failed to accept destination connection: %s\n", strerror(errno));
                        break;
                    }

                    int j;
                    for (j = 0; j < MAX_DSTS; j++) {
                        if (dsts[j].fd == -1) {
                            set_non_block(dst_fd);
                            event = (struct epoll_event){0};

                            event.events = EPOLLRDHUP | EPOLLHUP | EPOLLERR;
                            event.data.ptr = &dsts[j];

                            epoll_ctl(epoll_fd, EPOLL_CTL_ADD, dst_fd, &event);

                            dsts[j] = (dst_client){.fd = dst_fd, .last_active = time(NULL)};  // unsure if there's an edge case of dsts getting dc'ed from this when it's not their fault
                            
                            printf("Accepted new destination client on fd %d, slot %d\n", dsts[j].fd, j);
                            break;
                        }
                    }

                    if (j == MAX_DSTS) {
                        fprintf(
                            stderr,
                            "Rejecting attempted destination connection on fd %d (max allowed destinations reached)\n",
                            dst_fd);
                        close(dst_fd);
                    }
                }
            // incoming data from src    
            } else if (curr_fd_ptr == &src && src.fd != -1) {
                if (events[i].events & (EPOLLIN | EPOLLRDHUP)) {
                    bool cleanup = false;

                    while (src.bytes_in != BUFFER_SIZE) {  // drain buffer to reduce wakeups
                        ssize_t count = read(
                            src.fd,
                            src.read_buffer + src.bytes_in,
                            BUFFER_SIZE - src.bytes_in);

                        if (count < 0) {
                            if (errno != EAGAIN && errno != EWOULDBLOCK) {  // ie unrecoverable state, not that we just don't have more data to read right now
                                fprintf(stderr, "Error reading from source on fd %d: %s\nClosing source connection...\n",
                                        src.fd, strerror(errno));
                                cleanup = true;
                                break;
                            }

                            break; // drained
                        }

                        if (count == 0) {
                            printf("Source client on fd %d disconnected\nCleaning up...\n", src.fd);
                            cleanup = true;
                            break;
                        }

                        src.bytes_in += count;
                    }

                    if (cleanup) {  // todo: refactor out
                        close_src_client(epoll_fd, &src);
                        //epoll_ctl(epoll_fd, EPOLL_CTL_DEL, src.fd, NULL);
                        //close(src.fd);
                        //src = (src_client){0};
                        //src.fd = -1;
                    }
                }
            // outgoing data to dsts    
            } else {
                dst_client *dst = events[i].data.ptr;
                bool cleanup = false;

                if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {  // errored, hang up, half-close (close via peer, ie not writable)
                    int soerr = 0;
                    socklen_t len = sizeof(soerr);
                    getsockopt(dst->fd, SOL_SOCKET, SO_ERROR, &soerr, &len);
                    fprintf(stderr, "Destination on fd %d hung or disconnected (Status: %s)\nCleaning up...\n", dst->fd,
                            strerror(soerr));
                    cleanup = true;

                } else if (events[i].events & EPOLLOUT) {
                    while (dst->bytes_out != dst->bytes_left) {  // drain all that we can, to reduce wakeups needed - level triggered
                        ssize_t count = write(dst->fd, dst->write_buffer + dst->bytes_out,
                                              dst->bytes_left - dst->bytes_out);

                        if (count < 0) {
                            if (errno != EAGAIN && errno != EWOULDBLOCK) {  // actual error that isn't just no data currently
                                fprintf(
                                    stderr,
                                    "Error writing to destination on fd %d: %s\nClosing destination connection...\n",
                                    dst->fd, strerror(errno));
                                cleanup = true;
                                break;
                            }

                            break; // drained
                        }

                        dst->bytes_out += count;
                    }
                }

                if (cleanup) {  // clean-up dsts that are in an unrecoverable state
                    close_dst_client(epoll_fd, dst);
                    //epoll_ctl(epoll_fd, EPOLL_CTL_DEL, dst->fd, NULL);
                    //close(dst->fd);
                    //*dst = (dst_client){0};
                    //dst->fd = -1;

                } else {
                    if (dst->bytes_out == dst->bytes_left) {
                        dst->bytes_out = 0;
                        dst->bytes_left = 0;
                    }

                    if (dst->bytes_left > 0) {
                        if (!(dst->prev_mask & EPOLLOUT)) { // only want to set EPOLLOUT on mask change
                            struct epoll_event ev = {0};

                            ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
                            ev.data.ptr = dst;

                            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, dst->fd, &ev);
                            dst->prev_mask |= EPOLLOUT;
                        }
                    } else {
                        // remove EPOLLOUT so we don't get redundant wake-ups
                        if (dst->prev_mask & EPOLLOUT) {
                            dst->last_active = time(NULL);
                            struct epoll_event ev = {0};

                            ev.events = EPOLLIN | EPOLLRDHUP;
                            ev.data.ptr = dst;

                            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, dst->fd, &ev);
                            dst->prev_mask &= ~EPOLLOUT;
                        }
                    }
                }
            }
            
            // enqueue
            if (src.fd != -1) {
                bool backpressure = false;
                while (src.bytes_in >= sizeof(ctmp_header)) {
                    ctmp_header *header = (ctmp_header *) src.read_buffer;

                    if (!is_header_valid(header)) {
                        fprintf(stderr, "Invalid header from src on fd %d, closing connection...\n", src.fd);
                        
                        close_src_client(epoll_fd, &src);
                        //epoll_ctl(epoll_fd, EPOLL_CTL_DEL, src.fd, NULL);
                        //close(src.fd);
                        //src = (src_client){0};
                        //src.fd = -1;
                        
                        break;
                    }

                    size_t full_msg_len = sizeof(ctmp_header) + ntohs(header->length);  // convert length to host order so we set len correctly
                    if (src.bytes_in >= full_msg_len) {
                        // check for backpressure
                        for (int j = 0; j < MAX_DSTS; j++) {
                            if (dsts[j].fd != -1 && (BUFFER_SIZE - dsts[j].bytes_left < full_msg_len)) {
                                backpressure = true;
                                break;
                            }
                        }
                        if (backpressure) break;  // don't consume more data from src to stop overflows

                        // no bp --> no break --> we want to broadcast to each of the dsts
                        for (int j = 0; j < MAX_DSTS; j++) {
                            if (dsts[j].fd != -1) {
                                memcpy(dsts[j].write_buffer + dsts[j].bytes_left, src.read_buffer, full_msg_len);
                                dsts[j].bytes_left += full_msg_len;

                                uint32_t new_mask = EPOLLIN | EPOLLOUT;
                                if (new_mask != dsts[j].prev_mask) {
                                    event = (struct epoll_event){0};

                                    event.events = new_mask;
                                    event.data.ptr = &dsts[j];

                                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, dsts[j].fd, &event);
                                    dsts[j].prev_mask = new_mask;
                                }
                            }
                        }

                        memmove(src.read_buffer, src.read_buffer + full_msg_len, src.bytes_in - full_msg_len);
                        src.bytes_in -= full_msg_len;
                    } else {
                        break;
                    }
                }
                // add/remove bp
                uint32_t new_mask = backpressure ? EPOLLRDHUP : (EPOLLIN | EPOLLRDHUP);
                if (new_mask != src.prev_mask) {
                    event = (struct epoll_event){0};

                    event.events = new_mask;
                    event.data.ptr = &src;

                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, src.fd, &event);
                    src.prev_mask = new_mask;
                }
            }

            // check for dead dst clients, and remove them if they've exceeded the timeout
            // could also do for src, but less pertinent given single src
            time_t now = time(NULL);
            for (int l = 0; l < MAX_DSTS; l++) {
                if (dsts[l].fd != -1 && dsts[l].bytes_left > 0) {
                    if (now - dsts[l].last_active > CLIENT_TIMEOUT) {  // clean-up timed out clients - abstract out to its own method
                        close_dst_client(epoll_fd, &dsts[l]);
                        //epoll_ctl(epoll_fd, EPOLL_CTL_DEL, dsts[l].fd, NULL);
                        //close(dsts[l].fd);
                        //dsts[l] = (dst_client){0};
                        //dsts[l].fd = -1;
                    }
                }
            }
        }
    }

    // close connections
    close(src_listen_fd);
    close(dst_listen_fd);
    close(epoll_fd);
    close(src.fd);

    for (int j = 0; j < MAX_DSTS; j++) {

        close(dsts[j].fd);

    }

    printf("Proxy exiting...\n");
    return 0;

