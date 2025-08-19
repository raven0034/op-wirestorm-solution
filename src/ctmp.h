//
// Created by raven on 19/08/2025.
//

#include <stddef.h>
#include <stdint.h>

#ifndef CTMP_H
#define CTMP_H


/**
* PACKET STRUCTURE
*
* 0               1               2               3
* 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | MAGIC         | PADDING       | LENGTH                      |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | PADDING                                                     |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* | DATA ...................................................... |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
*
* MAGIC - 0xCC
* PADDING - 0x00 FILL
*
* Inherent data limit is 65536 bytes
**/

typedef struct __attribute__((__packed__)) {
    uint8_t magic;
    uint8_t mpad;
    uint16_t length;
    uint32_t lpad;
} ctmp_header;

int is_header_valid(const ctmp_header *header);

#endif //CTMP_H