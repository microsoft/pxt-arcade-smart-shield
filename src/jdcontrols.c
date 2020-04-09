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

#define BUTTON_FLAGS 0
#define NUM_PLAYERS 1

static uint16_t buttonAd[] = { //
    BUTTON_FLAGS | (NUM_PLAYERS << 8), JD_ARCADE_CONTROLS_BUTTON_LEFT, JD_ARCADE_CONTROLS_BUTTON_UP,
    JD_ARCADE_CONTROLS_BUTTON_RIGHT,   JD_ARCADE_CONTROLS_BUTTON_DOWN, JD_ARCADE_CONTROLS_BUTTON_A,
    JD_ARCADE_CONTROLS_BUTTON_B,       JD_ARCADE_CONTROLS_BUTTON_MENU};

#define NUM_PINS sizeof(buttonPins)

static bool sendReport, advertise, inited;

void jd_arcade_controls_incoming(jd_packet_t *pkt) {
    if (pkt->service_command == JD_CMD_ADVERTISEMENT_DATA)
        advertise = true;
    else
        // not expecting anything, but make sure we respond with something
        sendReport = true;
}

void jd_arcade_controls_process() {
    if (!inited) {
        inited = true;
        for (int i = 0; i < NUM_PINS; ++i) {
            pin_setup_input(buttonPins[i], 1);
        }
    }
    if (jd_display_frame_start)
        sendReport = true;
}

void jd_arcade_controls_outgoing(int serviceNo) {
    jdspi_send_ad_data(serviceNo, &advertise, buttonAd, sizeof(buttonAd));

    if (!sendReport)
        return;

    jd_arcade_controls_report_entry_t reports[NUM_PINS], *report;
    report = reports;

    for (int i = 0; i < NUM_PINS; ++i) {
        if (pin_get(buttonPins[i]) == 0) {
            report->button = i;
            report->player_index = 0;
            report->pressure = 0xff;
            // DMESG("btn %d", i);
            report++;
        }
    }

    if (jdspi_send(serviceNo, JD_CMD_GET_REG | JD_REG_READING, reports,
                   (uint8_t *)report - (uint8_t *)reports))
        sendReport = false;
}
