// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "jdsimple.h"
#include "jdspi.h"

static const jd_display_advertisement_data_t displayAd = {
    .flags = JD_DISPLAY_FLAGS_RETINA, .bpp = 4, .width = 160, .height = 120};

static void handleCmdSetWindow(jd_display_set_window_t *cmd) {
    screen_start_pixels(cmd->x, cmd->y, cmd->width, cmd->height);
}

static void handleCmdPalette(jd_display_palette_t *cmd) {
    screen_send_palette(cmd->palette);
}

static void handleCmdPixels(jd_display_pixels_t *cmd, uint32_t size) {
    screen_send_indexed(cmd->pixels, (size >> 2));
}

static void handleCmdBrightness(uint8_t level) {
    screen_set_backlight(level);
}

bool jd_display_frame_start;
static bool advertise;
void jd_display_incoming(jd_packet_t *pkt) {
    void *pktData = pkt->data;
    switch (pkt->service_command) {
    case JD_CMD_ADVERTISEMENT_DATA:
        advertise = true;
        break;
    case JD_DISPLAY_CMD_PALETTE:
        handleCmdPalette(pktData);
        break;
    case JD_DISPLAY_CMD_SET_WINDOW:
        handleCmdSetWindow(pktData);
        jd_display_frame_start = true;
        break;
    case JD_DISPLAY_CMD_PIXELS:
        jd_display_frame_start = false;
        handleCmdPixels(pktData, pkt->service_size);
        break;
    case JD_DISPLAY_CMD_SET_BRIGHTNESS:
        handleCmdBrightness(pkt->data[0]);
        break;
    }
}

void jd_display_process() {}

void jd_display_outgoing(int serviceNo) {
    jdspi_send_ad_data(serviceNo, &advertise, &displayAd, sizeof(displayAd));
}

void jdspi_connected() {
    screen_set_backlight(255);
}

void jdspi_disconnected() {
    pulse_log_pin2();
    screen_set_backlight(0);
    screen_sleep();

    LL_PWR_SetPowerMode(LL_PWR_MODE_STANDBY);
    SCB->SCR |= (uint32_t)SCB_SCR_SLEEPDEEP_Msk;
    while (1)
        asm("wfi");
}
