// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "jdsimple.h"
#include "jdspi.h"

#define SAMPLE_RATE 44100

static bool sendReport;
static uint32_t bufferPending;
static uint32_t lastReport;

void jd_arcade_sound_incoming(jd_packet_t *pkt) {
    if (pkt->service_command == JD_ARCADE_SOUND_CMD_PLAY)
        bufferPending += pkt->service_size;
}

void jd_arcade_sound_process() {
    if (jd_display_frame_start)
        sendReport = true;
}

void jd_arcade_sound_outgoing(int serviceNo) {
    if (!sendReport)
        return;

    // simulate data consumption
    uint32_t now = (uint32_t)tim_get_micros();
    if (lastReport) {
        uint32_t delta = now - lastReport;
        // this is approximate, should be really sizeof(int16_t) * SAMPLE_RATE * delta / 1_000_000
        uint32_t dataConsumed = sizeof(int16_t) * ((SAMPLE_RATE * (delta >> 10)) >> 10);
        if (dataConsumed < bufferPending) {
            bufferPending -= dataConsumed;
        } else {
            bufferPending = 0;
        }
    }
    lastReport = now;

    uint32_t sampleRate = SAMPLE_RATE << 10;
    if (jdspi_send(serviceNo, JD_GET(JD_ARCADE_SOUND_REG_SAMPLE_RATE), &sampleRate,
                   sizeof(sampleRate)) &&
        jdspi_send(serviceNo, JD_GET(JD_ARCADE_SOUND_REG_BUFFER_PENDING), &bufferPending,
                   sizeof(bufferPending)))
        sendReport = false;
}
