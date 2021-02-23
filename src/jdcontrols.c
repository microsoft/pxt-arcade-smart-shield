// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "jdsimple.h"
#include "jdspi.h"

static const uint8_t buttonPins[] = {
    0xff, // no button
#ifdef PROTO_V2
    PB_5, // LEFT
    PB_3, // UP
    PB_4, // RIGHT
    PB_6, // DOWN,
    PF_0, // A
    PF_1, // B
    PB_7, // MENU
#endif
#ifndef PSCREEN
    PB_5,  // LEFT
    PB_3,  // UP
    PB_4,  // RIGHT
    PA_15, // DOWN,
    PA_1,  // A
#ifdef BB_V0
    PA_12, // B
#else
    PA_2, // B
#endif
    PA_0, // MENU
#endif
};

static uint16_t buttonAd[] = { //
    JD_ARCADE_GAMEPAD_BUTTON_LEFT, JD_ARCADE_GAMEPAD_BUTTON_UP, JD_ARCADE_GAMEPAD_BUTTON_RIGHT,
    JD_ARCADE_GAMEPAD_BUTTON_DOWN, JD_ARCADE_GAMEPAD_BUTTON_A,  JD_ARCADE_GAMEPAD_BUTTON_B,
    JD_ARCADE_GAMEPAD_BUTTON_MENU};

#define NUM_PINS sizeof(buttonPins)

static bool sendReport, advertise, inited;

void jd_arcade_gamepad_incoming(jd_packet_t *pkt) {
    if (pkt->service_command == JD_GET(JD_ARCADE_GAMEPAD_REG_BUTTONS))
        advertise = true;
    else
        // not expecting anything, but make sure we respond with something
        sendReport = true;
}

void jd_arcade_gamepad_process() {
    if (!inited) {
        inited = true;
        for (int i = 0; i < NUM_PINS; ++i) {
            pin_setup_input(buttonPins[i], 1);
        }
    }
    if (jd_display_frame_start)
        sendReport = true;
}

void jd_arcade_gamepad_outgoing(int serviceNo) {
    if (advertise) {
        advertise = false;
        jdspi_send(serviceNo, JD_GET(JD_ARCADE_GAMEPAD_REG_BUTTONS), buttonAd, sizeof(buttonAd));
    }

    if (!sendReport)
        return;

    jd_arcade_gamepad_buttons_t reports[NUM_PINS], *report;
    report = reports;

    // TODO send events?

    for (int i = 0; i < NUM_PINS; ++i) {
        if (pin_get(buttonPins[i]) == 0) {
            report->button = i;
            report->pressure = 0xff;
            // DMESG("btn %d", i);
            report++;
        }
    }

    if (jdspi_send(serviceNo, JD_GET(JD_REG_READING), reports,
                   (uint8_t *)report - (uint8_t *)reports))
        sendReport = false;
}
