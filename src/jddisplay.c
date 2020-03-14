#include "jdsimple.h"
#include "jdspi.h"

static void handleCmdSetWindow(jd_display_set_window_t *cmd) {
    screen_start_pixels(cmd->x, cmd->y, cmd->width, cmd->height);
}

static void handleCmdPalette(jd_display_palette_t *cmd) {
    screen_send_palette(cmd->palette);
}

static void handleCmdPixels(jd_display_pixels_t *cmd, uint32_t size) {
    screen_send_indexed(cmd->pixels, (size >> 2));
}

static void handleCmdBrightness(jd_display_brightness_t *cmd) {
    screen_set_backlight(cmd->level >> 8);
}

bool jd_display_frame_start;
void jd_display_incoming(jd_packet_t *pkt) {
    void *pktData = pkt->data;
    switch (pkt->service_command) {
    case JD_DISPLAY_CMD_PALETTE:
        handleCmdPalette(pktData);
        break;
    case JD_DISPLAY_CMD_SET_WINDOW:
        handleCmdSetWindow(pktData);
        jd_display_frame_start = true;
        break;
    case JD_DISPLAY_CMD_PIXELS:
        jd_display_frame_start = false;
        handleCmdPixels(pktData, pkt->size);
        break;
    case JD_DISPLAY_CMD_SET_BRIGHTNESS:
        handleCmdBrightness(pktData);
        break;
    }
}

void jd_display_process() {}
void jd_display_outgoing(int serviceNo, void *dst) {}

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
