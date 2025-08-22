//
// Created by raven on 19/08/2025.
//

#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include "ctmp.h"

/**
 * @brief Checks for the validity of a CTMP header as defined in Stage 1
 * 
 * @param header a CTMP header conforming to the protocol in Stage 1
 *              worth noting I took a strict interpretation of the protocol
 *              so whilst Stage 1 messages should work fine on Stage 2,
                Stage 2 messages will not work fine with Stage 1 (strictly 0s in padding)
 * @return int - 1 for valid, 0 for invalid
 */
int is_st1_header_valid(const ctmp1_header *header) {

    // separated returns to facilitate potential error codes later

    if (header->magic != CTMP_MAGIC) {
        return 0;  // invalid if magic byte doesn't match
    }

    if (header->mpad != 0) {
        return 0;  // technically inconsequential on the face of it, but enforce protocol spec for consistency/debugging & compat
    }

    // not much to consider esp at this point for length
    // 2 bytes --> max length 65535 - possible for rsrc mgmt + cap but not relevant to whether the header itself is valid
    // 0 length messages are almost certainly pointless, but by the spec should be valid

    if (header->lpad != 0) {
        return 0;
    }

    return 1;
}

/**
 * @brief Checks for the validity of a CTMP header as defined in Stage 2 of the challenge
 * 
 * @param header a CTMP header conforming to the protocol in Stage 2
 * @return int - 1 for valid, 0 for invalid
 */
int is_header_valid(const ctmp_header *header) {
    if (header->magic != CTMP_MAGIC) {
        return 0;
    }    

    if (header->options != 0 && header->options != CTMP_OPTION_SENSITIVE) {
        return 0;
    }

    if (header->cpad != 0) {
        return 0;
    }    

    //ignore length again, we also don't care about checksum here

    return 1;
}

/**
 * @brief Computes the 16 bit one's complement of the one's complement sum of all 16 bit words in the provider buffer
 * 
 * @param buffer buffer to read the words from
 * @param len length to read
 * @return uint16_t the resulting checksum
 */
uint16_t compute_checksum(const uint8_t *buffer, size_t len) {
    uint32_t sum = 0;

    // sum all 16-bit words
    for (size_t i = 0; i + 1 < len; i += 2) {
        sum += (buffer[i] << 8) | buffer[i + 1]; // big-endian word
    }

    // if odd length, pad last byte with 0
    if (len & 1) {
        sum += buffer[len - 1] << 8;
    }

    // fold carry bits into lower 16 bits
    sum = (sum & 0xFFFF) + (sum >> 16);
    sum += (sum >> 16);  // in case previous fold generated another carry

    return ~sum;  // flip
}

/**
 * @brief Checks a computed checksum against the checksum received, for messages marked as sensitive.
 * Uses 0xCC in place of the checksum field.
 *
 * @param header - CTMP header of the message
 * @param payload - payload of the CTMP message
 * @return int - 1 if valid, 0 if not
 */
int is_valid_checksum(const ctmp_header *header, const uint8_t *payload) {
    if (header->options != CTMP_OPTION_SENSITIVE) {
        return 1;  // fine to go if not sensitive
    }

    size_t total_len = sizeof(ctmp_header) + ntohs(header->length);
    uint8_t buf[total_len];
    
    // copy header & payload
    memcpy(buf, header, sizeof(ctmp_header));
    memcpy(buf + sizeof(ctmp_header), payload, ntohs(header->length));

    buf[4] = 0xCC; buf[5] = 0xCC; // as per spec, treat field as 0xCC filled

    uint16_t calc = compute_checksum(buf, total_len);  // compute using the modified buffer
    uint16_t received = ntohs(header->checksum);  // convert received from big to little endian

    return calc == received;
}
