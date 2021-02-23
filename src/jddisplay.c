// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "jdsimple.h"
#include "jdspi.h"

static const jd_arcade_screen_announce_report_t displayAd = {
    .flags = JD_ARCADE_SCREEN_DISPLAY_FLAGS_COLUMN_MAJOR | JD_ARCADE_SCREEN_DISPLAY_FLAGS_UPSCALE2X,
    .bits_per_pixel = 4,
    .width = 160,
    .height = 120,
};

static void handleCmdSetWindow(jd_arcade_screen_start_update_t *cmd) {
    screen_start_pixels(cmd->x, cmd->y, cmd->width, cmd->height);
}

static void handleCmdPalette(uint32_t *cmd) {
    screen_send_palette(cmd);
}

static void handleCmdPixels(uint32_t *cmd, uint32_t size) {
    screen_send_indexed(cmd, (size >> 2));
}

static void handleCmdBrightness(uint8_t level) {
    screen_set_backlight(level);
}

bool jd_display_frame_start;
static bool advertise;
void jd_display_incoming(jd_packet_t *pkt) {
    void *pktData = pkt->data;
    switch (pkt->service_command) {
    case JD_ARCADE_SCREEN_CMD_ANNOUNCE:
        advertise = true;
        break;
    case JD_SET(JD_ARCADE_SCREEN_REG_PALETTE):
        handleCmdPalette(pktData);
        break;
    case JD_ARCADE_SCREEN_CMD_START_UPDATE:
        handleCmdSetWindow(pktData);
        jd_display_frame_start = true;
        break;
    case JD_ARCADE_SCREEN_CMD_SET_PIXELS:
        jd_display_frame_start = false;
        handleCmdPixels(pktData, pkt->service_size);
        break;
    case JD_SET(JD_ARCADE_SCREEN_REG_BRIGHTNESS):
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
