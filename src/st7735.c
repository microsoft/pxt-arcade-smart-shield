// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "jdsimple.h"

#ifndef PSCREEN

#define PIN_DISPLAY_DC PB_6
#define PIN_DISPLAY_CS PB_9
#define PIN_DISPLAY_RST PB_1
#ifdef BB_V0
#define PIN_DISPLAY_BL -1 // PB_0
#else
#define PIN_DISPLAY_BL PB_0
#endif

#ifdef BB_V0
#define DISPLAY_CFG0 0x01000080
#else
#define DISPLAY_CFG0 0x00000080
#endif
#define DISPLAY_CFG1 0x00000603
#define DISPLAY_CFG2 32

#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 128

#define ST7735_NOP 0x00
#define ST7735_SWRESET 0x01
#define ST7735_SLPIN 0x10
#define ST7735_SLPOUT 0x11
#define ST7735_PTLON 0x12
#define ST7735_NORON 0x13
#define ST7735_INVOFF 0x20
#define ST7735_INVON 0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON 0x29
#define ST7735_CASET 0x2A
#define ST7735_RASET 0x2B
#define ST7735_RAMWR 0x2C
#define ST7735_RAMRD 0x2E
#define ST7735_PTLAR 0x30
#define ST7735_COLMOD 0x3A
#define ST7735_MADCTL 0x36
#define ST7735_FRMCTR1 0xB1
#define ST7735_INVCTR 0xB4
#define ST7735_PWCTR6 0xFC
#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1
#define ST7735_RGBSET 0x2D

#define CFG(x) x

static volatile uint8_t transferDoneFlag;
static uint8_t cmdBuf[20];
static uint8_t dataBuf[244 * 3];

static void transferDone() {
    transferDoneFlag = 1;
}

static void transfer(void *ptr, uint32_t len) {
    transferDoneFlag = 0;
    // DMESG("tr %d", len);
    dspi_tx(ptr, len, transferDone);
    while (!transferDoneFlag)
        ;
}

#define DELAY 0x80

// clang-format off
static const uint8_t initCmds[] = {
    ST7735_SWRESET,   DELAY,  //  1: Software reset, 0 args, w/delay
      120,                    //     150 ms delay
    ST7735_SLPOUT ,   DELAY,  //  2: Out of sleep mode, 0 args, w/delay
      120,                    //     500 ms delay
    ST7735_INVOFF , 0      ,  // 13: Don't invert display, no args, no delay
    ST7735_COLMOD , 1      ,  // 15: set color mode, 1 arg, no delay:
      0x03,                  //     12-bit color
    ST7735_GMCTRP1, 16      , //  1: Magical unicorn dust, 16 args, no delay:
      0x02, 0x1c, 0x07, 0x12,
      0x37, 0x32, 0x29, 0x2d,
      0x29, 0x25, 0x2B, 0x39,
      0x00, 0x01, 0x03, 0x10,
    ST7735_NORON  ,    DELAY, //  3: Normal display on, no args, w/delay
      10,                     //     10 ms delay
    ST7735_DISPON ,    DELAY, //  4: Main screen turn on, no args w/delay
      10,
    0, 0 // END
};
// clang-format on

#define SET_DC(v) pin_set(CFG(PIN_DISPLAY_DC), v)
#define SET_CS(v) pin_set(CFG(PIN_DISPLAY_CS), v)

static void scr_delay(unsigned msec) {
    wait_us(msec * 1000);
}

static void sendCmd(uint8_t *buf, int len) {
    if (buf != cmdBuf)
        memcpy(cmdBuf, buf, len);
    buf = cmdBuf;

    SET_DC(0);
    SET_CS(0);

    transfer(buf, 1);

    SET_DC(1);

    len--;
    buf++;
    if (len > 0)
        transfer(buf, len);

    SET_CS(1);
}

static void sendCmdSeq(const uint8_t *buf) {
    while (*buf) {
        cmdBuf[0] = *buf++;
        int v = *buf++;
        int len = v & ~DELAY;
        memcpy(cmdBuf + 1, buf, len);
        sendCmd(cmdBuf, len + 1);
        buf += len;
        if (v & DELAY) {
            scr_delay(*buf++);
        }
    }
}

static void setAddrWindow(int x, int y, int w, int h) {
    w += x - 1;
    h += y - 1;
    uint8_t cmd0[] = {ST7735_RASET, 0, (uint8_t)x, (uint8_t)(w >> 8), (uint8_t)w};
    uint8_t cmd1[] = {ST7735_CASET, 0, (uint8_t)y, (uint8_t)(h >> 8), (uint8_t)h};
    sendCmd(cmd1, sizeof(cmd1));
    sendCmd(cmd0, sizeof(cmd0));
}

static void configure(uint8_t madctl, uint32_t frmctr1) {
    uint8_t cmd0[] = {ST7735_MADCTL, madctl};
    uint8_t cmd1[] = {ST7735_FRMCTR1, (uint8_t)(frmctr1 >> 16), (uint8_t)(frmctr1 >> 8),
                      (uint8_t)frmctr1};
    sendCmd(cmd0, sizeof(cmd0));
    sendCmd(cmd1, cmd1[3] == 0xff ? 3 : 4);
}

static void startRAMWR(uint8_t cmd) {
    cmdBuf[0] = cmd;
    sendCmd(cmdBuf, 1);
    SET_DC(1);
    SET_CS(0);
}

void screen_sleep() {
    cmdBuf[0] = ST7735_SLPIN;
    sendCmd(cmdBuf, 1);
}

void screen_set_backlight(int level) {
    pin_set(CFG(PIN_DISPLAY_BL), level > 0);
}

void screen_send_palette(const uint32_t *palette) {
    memset(dataBuf, 0, sizeof(dataBuf));
    uint8_t *base = dataBuf;
    for (int i = 0; i < 16; ++i) {
        base[i] = (palette[i] >> 18) & 0x3f;
        base[i + 32] = (palette[i] >> 10) & 0x3f;
        base[i + 32 + 64] = (palette[i] >> 2) & 0x3f;
    }
    startRAMWR(ST7735_RGBSET);
    transfer(dataBuf, 128);
    SET_CS(1);
}

#define SEND_PIX(shift)                                                                            \
    do {                                                                                           \
        x = 0x1011 * ((v >> shift) & 0xf) | (0x110100 * ((v >> (shift + 4)) & 0xf));               \
        *dst++ = x >> 0;                                                                           \
        *dst++ = x >> 8;                                                                           \
        *dst++ = x >> 16;                                                                          \
    } while (0)

void screen_send_indexed(const uint32_t *src, uint32_t numwords) {
    uint8_t *dst = dataBuf;
    while (numwords--) {
        uint32_t v = *src++;
        uint32_t x;
        SEND_PIX(0);
        SEND_PIX(8);
        SEND_PIX(16);
        SEND_PIX(24);
    }
    transfer(dataBuf, dst - dataBuf);
}

void screen_start_pixels(int x, int y, int w, int h) {
    setAddrWindow(x, y, w, h);
    startRAMWR(ST7735_RAMWR);
}

static const uint32_t palette[] = {
    0x000000, // 0
    0xffffff, // 1
    0xff2121, // 2
    0xff93c4, // 3
    0xff8135, // 4
    0xfff609, // 5
    0x249ca3, // 6
    0x78dc52, // 7
    0x003fad, // 8
    0x87f2ff, // 9
    0x8e2ec4, // 10
    0xa4839f, // 11
    0x5c406c, // 12
    0xe5cdc4, // 13
    0x91463d, // 14
    0x000000, // 15
};

void screen_stripes() {
    static uint32_t line[64 / 4];
    screen_send_palette(palette);
    startRAMWR(ST7735_RAMWR);
    for (int i = 0; i < 16; ++i) {
        memset(line, 0x11 * i, sizeof(line));
        for (int j = 0; j < 10; ++j)
            screen_send_indexed(line, 64 / 4);
    }
    SET_CS(0);
}

void screen_init() {
    pin_setup_output(CFG(PIN_DISPLAY_BL));
    pin_setup_output(CFG(PIN_DISPLAY_DC));
    pin_setup_output(CFG(PIN_DISPLAY_RST));
    pin_setup_output(CFG(PIN_DISPLAY_CS));

    SET_CS(1);
    SET_DC(1);

    pin_set(CFG(PIN_DISPLAY_BL), 0);

    pin_set(CFG(PIN_DISPLAY_RST), 0);
    wait_us(20);
    pin_set(CFG(PIN_DISPLAY_RST), 1);
    wait_us(10000);

    sendCmdSeq(initCmds);

    uint32_t cfg0 = CFG(DISPLAY_CFG0);
    // uint32_t cfg2 = CFG(DISPLAY_CFG2);
    uint32_t frmctr1 = CFG(DISPLAY_CFG1);
    uint32_t madctl = cfg0 & 0xff;
    uint32_t offX = (cfg0 >> 8) & 0xff;
    uint32_t offY = (cfg0 >> 16) & 0xff;
    // uint32_t freq = (cfg2 & 0xff);

    DMESG("configure screen: FRMCTR1=%p MADCTL=%p SPI", frmctr1, madctl);
    configure(madctl, frmctr1);
    setAddrWindow(offX, offY, CFG(DISPLAY_WIDTH), CFG(DISPLAY_HEIGHT));

    if (cfg0 & 0x1000000) {
        cmdBuf[0] = ST7735_INVON;
        sendCmd(cmdBuf, 1);
    }
}

#endif