// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "jdsimple.h"

#define ALIGN(x) (((x) + 3) & ~3)

int jd_shift_frame(jd_frame_t *frame) {
    int psize = frame->size;
    jd_packet_t *pkt = (jd_packet_t *)frame;
    int oldsz = pkt->service_size + 4;
    if (ALIGN(oldsz) >= psize)
        return 0; // nothing to shift

    int ptr;
    if (frame->data[oldsz] == 0xff) {
        ptr = frame->data[oldsz + 1];
        if (ptr >= psize)
            return 0; // End-of-frame
        if (ptr <= oldsz) {
            DMESG("invalid super-frame %d %d", ptr, oldsz);
            return 0; // don't let it go back, must be some corruption
        }
    } else {
        ptr = ALIGN(oldsz);
    }

    // assume the first one got the ACK sorted
    frame->flags &= ~JD_FRAME_FLAG_ACK_REQUESTED;

    uint8_t *src = &frame->data[ptr];
    int newsz = *src + 4;
    if (ptr + newsz > psize) {
        DMESG("invalid super-frame %d %d %d", ptr, newsz, psize);
        return 0;
    }
    uint32_t *dst = (uint32_t *)frame->data;
    uint32_t *srcw = (uint32_t *)src;
    // don't trust memmove()
    for (int i = 0; i < newsz; i += 4)
        *dst++ = *srcw++;
    // store ptr
    ptr += ALIGN(newsz);
    frame->data[newsz] = 0xff;
    frame->data[newsz + 1] = ptr;

    return 1;
}

void jd_reset_frame(jd_frame_t *frame) {
    frame->size = 0x00;
}

void *jd_push_in_frame(jd_frame_t *frame, unsigned service_num, unsigned service_cmd,
                       unsigned service_size) {
    if (service_num >> 8)
        panic();
    if (service_cmd >> 16)
        panic();
    uint8_t *dst = frame->data + frame->size;
    unsigned szLeft = (uint8_t *)frame + sizeof(*frame) - dst;
    if (service_size + 4 > szLeft)
        return NULL;
    *dst++ = service_size;
    *dst++ = service_num;
    *dst++ = service_cmd & 0xff;
    *dst++ = service_cmd >> 8;
    frame->size += ALIGN(service_size + 4);
    return dst;
}

