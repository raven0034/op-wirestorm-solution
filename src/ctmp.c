//
// Created by raven on 19/08/2025.
//

#include "ctmp.h"
#include <stdbool.h>

int is_header_valid(const ctmp_header *header) {

    // separated returns to facilitate potential error codes later

    if (header->magic != 0xCC) {
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
