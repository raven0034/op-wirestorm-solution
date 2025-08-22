//
// Created by raven on 19/08/2025.
//

#ifndef MAIN_H
#define MAIN_H

#define SRC_PORT 33333
#define DST_PORT 44444

#define MAX_DSTS 50

ssize_t readn(int fd, void *buffer, ssize_t expected_bytes);
ssize_t writen(int fd, const void *buffer, ssize_t expected_bytes);
void int_handler(int sig);
int main();

#endif //MAIN_H
