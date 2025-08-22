//
// Created by raven on 19/08/2025.
//

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef CTMP_H
#define CTMP_H

#define CTMP_OPTION_SENSITIVE 0b01000000
#define CTMP_MAGIC 0xCC
#define CTMP_HEADER_SIZE 8

/**
* @brief Defines the structure for a Stage 1 CTMP header, as per the below reference from the challenge.
*
* STAGE 1 HEADER STRUCTURE
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
* Inherent data limit is 65535 bytes
*/
typedef struct __attribute__((__packed__)) {
    uint8_t magic;
    uint8_t mpad;
    uint16_t length;
    uint32_t lpad;
} ctmp1_header;

int is_st1_header_valid(const ctmp1_header *header);

/**
* @brief Defines the structure of a stage 2 CTMP header, as per the below reference from the challenge.
*
* STAGE 2 HEADER STRUCTURE
*    0               1               2               3
*    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
*    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*    | MAGIC 0xCC    | OPTIONS       | LENGTH                      |
*    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*    | CHECKSUM                      | PADDING                     |
*    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*    | DATA ...................................................... |
*    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
*
*   OPTIONS:
*
*      Bits   0:  Reserved for Future Use.
*      Bit    1:  0 = Normal,        1 = Sensitive.
*      Bit  2-7:  Padding.
*
*         0     1     2     3     4     5     6     7
*      +-----+-----+-----+-----+-----+-----+-----+-----+
*      |     |     |                                   |
*      | RES | SEN |              PADDING              |
*      |     |     |                                   |
*      +-----+-----+-----+-----+-----+-----+-----+-----+
*
*  Main thing is that first PADDING has become OPTIONS (but same size)
*  Basically just do same as before, enforce PADDING of 0 for all but bit 1
*  
*  CHECKSUM replaces half of the 2nd PADDING
*/
typedef struct __attribute__((__packed__)) {
    uint8_t magic;
    uint8_t options;
    uint16_t length;
    uint16_t checksum;
    uint16_t cpad;
} ctmp_header;

int is_header_valid(const ctmp_header *header);

uint16_t compute_checksum(const uint8_t *buffer, size_t len);

int is_valid_checksum(const ctmp_header *header, const uint8_t *payload);


#endif //CTMP_H