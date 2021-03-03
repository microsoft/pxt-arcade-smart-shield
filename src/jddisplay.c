// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "jdsimple.h"
#include "jdspi.h"

static void handleCmdSetWindow(jd_indexed_screen_start_update_t *cmd) {
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
static uint8_t advertise;
void jd_display_incoming(jd_packet_t *pkt) {
    void *pktData = pkt->data;
    switch (pkt->service_command) {
    case JD_SET(JD_INDEXED_SCREEN_REG_PALETTE):
        handleCmdPalette(pktData);
        break;
    case JD_INDEXED_SCREEN_CMD_START_UPDATE:
        handleCmdSetWindow(pktData);
        jd_display_frame_start = true;
        advertise = 3; // auto announce everything
        break;
    case JD_INDEXED_SCREEN_CMD_SET_PIXELS:
        jd_display_frame_start = false;
        handleCmdPixels(pktData, pkt->service_size);
        break;
    case JD_SET(JD_INDEXED_SCREEN_REG_BRIGHTNESS):
        handleCmdBrightness(pkt->data[0]);
        break;
    default:
        if ((pkt->service_command >> 12) == (JD_CMD_GET_REGISTER >> 12))
            advertise = 1;
        break;
    }
}

void jd_display_process() {}

static int sendreg16(int serviceNo, int reg, uint16_t val) {
    return jdspi_send(serviceNo, JD_GET(reg), &val, sizeof(val)) != NULL;
}

static int sendreg8(int serviceNo, int reg, uint8_t val) {
    return jdspi_send(serviceNo, JD_GET(reg), &val, sizeof(val)) != NULL;
}

void jd_display_outgoing(int serviceNo) {
    if (!advertise)
        return;
    if (advertise == 1) {
        if (sendreg16(serviceNo, JD_INDEXED_SCREEN_REG_WIDTH, 160) &&
            sendreg16(serviceNo, JD_INDEXED_SCREEN_REG_HEIGHT, 120) &&
            sendreg8(serviceNo, JD_INDEXED_SCREEN_REG_BITS_PER_PIXEL, 4) &&
            sendreg8(serviceNo, JD_INDEXED_SCREEN_REG_UP_SAMPLING, 2) &&
            sendreg8(serviceNo, JD_INDEXED_SCREEN_REG_WIDTH_MAJOR, 0) &&
            sendreg16(serviceNo, JD_INDEXED_SCREEN_REG_ROTATION, 0))
            advertise = 0;
    } else {
        advertise--;
    }
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
