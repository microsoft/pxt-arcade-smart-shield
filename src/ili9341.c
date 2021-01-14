// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "jdsimple.h"

#ifdef PSCREEN

#ifdef PROTO_V2
#define PIN_DISPLAY_RD PB_11
#define PIN_DISPLAY_WR PA_15
#define PIN_DISPLAY_DC PA_4
#define PIN_DISPLAY_CS1 -1
#define PIN_DISPLAY_CS PB_10
#define PIN_DISPLAY_BL PB_0
#define PIN_DISPLAY_RST PB_1
// LCD0,1,2 - PA0,1,2
// LCD3,4,..,7 - PA8,9,..,12
#else
#define PIN_DISPLAY_RD PA_9
#define PIN_DISPLAY_WR PB_8
#define PIN_DISPLAY_DC PB_9
#define PIN_DISPLAY_CS1 -1
#define PIN_DISPLAY_CS PA_15
#define PIN_DISPLAY_BL PA_10
#define PIN_DISPLAY_RST PA_8
#endif

#define DISPLAY_CFG0 0x00000008 // connector on the right of the screen
//#define DISPLAY_CFG0 0x000000C8 // connector on the left of the screen
#define DISPLAY_CFG1 0x000010ff

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

#define ILI9341_NOP 0x00     ///< No-op register
#define ILI9341_SWRESET 0x01 ///< Software reset register
#define ILI9341_RDDID 0x04   ///< Read display identification information
#define ILI9341_RDDST 0x09   ///< Read Display Status

#define ILI9341_SLPIN 0x10  ///< Enter Sleep Mode
#define ILI9341_SLPOUT 0x11 ///< Sleep Out
#define ILI9341_PTLON 0x12  ///< Partial Mode ON
#define ILI9341_NORON 0x13  ///< Normal Display Mode ON

#define ILI9341_RDMODE 0x0A     ///< Read Display Power Mode
#define ILI9341_RDMADCTL 0x0B   ///< Read Display MADCTL
#define ILI9341_RDPIXFMT 0x0C   ///< Read Display Pixel Format
#define ILI9341_RDIMGFMT 0x0D   ///< Read Display Image Format
#define ILI9341_RDSELFDIAG 0x0F ///< Read Display Self-Diagnostic Result

#define ILI9341_INVOFF 0x20   ///< Display Inversion OFF
#define ILI9341_INVON 0x21    ///< Display Inversion ON
#define ILI9341_GAMMASET 0x26 ///< Gamma Set
#define ILI9341_DISPOFF 0x28  ///< Display OFF
#define ILI9341_DISPON 0x29   ///< Display ON

#define ILI9341_CASET 0x2A ///< Column Address Set
#define ILI9341_RASET 0x2B ///< Page Address Set
#define ILI9341_RAMWR 0x2C ///< Memory Write
#define ILI9341_RAMRD 0x2E ///< Memory Read
#define ILI9341_RGBSET 0x2D

#define ILI9341_PTLAR 0x30    ///< Partial Area
#define ILI9341_MADCTL 0x36   ///< Memory Access Control
#define ILI9341_VSCRSADD 0x37 ///< Vertical Scrolling Start Address
#define ILI9341_PIXFMT 0x3A   ///< COLMOD: Pixel Format Set

#define ILI9341_FRMCTR1 0xB1 ///< Frame Rate Control (In Normal Mode/Full Colors)
#define ILI9341_FRMCTR2 0xB2 ///< Frame Rate Control (In Idle Mode/8 colors)
#define ILI9341_FRMCTR3 0xB3 ///< Frame Rate control (In Partial Mode/Full Colors)
#define ILI9341_INVCTR 0xB4  ///< Display Inversion Control
#define ILI9341_DFUNCTR 0xB6 ///< Display Function Control

#define ILI9341_PWCTR1 0xC0 ///< Power Control 1
#define ILI9341_PWCTR2 0xC1 ///< Power Control 2
#define ILI9341_PWCTR3 0xC2 ///< Power Control 3
#define ILI9341_PWCTR4 0xC3 ///< Power Control 4
#define ILI9341_PWCTR5 0xC4 ///< Power Control 5
#define ILI9341_VMCTR1 0xC5 ///< VCOM Control 1
#define ILI9341_VMCTR2 0xC7 ///< VCOM Control 2

#define ILI9341_RDID1 0xDA ///< Read ID 1
#define ILI9341_RDID2 0xDB ///< Read ID 2
#define ILI9341_RDID3 0xDC ///< Read ID 3
#define ILI9341_RDID4 0xDD ///< Read ID 4

#define ILI9341_GMCTRP1 0xE0 ///< Positive Gamma Correction
#define ILI9341_GMCTRN1 0xE1 ///< Negative Gamma Correction
//#define ILI9341_PWCTR6     0xFC

#define CFG(x) x

static volatile uint8_t transferDoneFlag;
static uint8_t cmdBuf[20];
static uint8_t dataBuf[244 * 3];
static uint32_t bytesPerColumn;

static void transfer(void *ptr, uint32_t len);

#define DELAY 0x80

// clang-format off
static const uint8_t initCmds[] = {
  // Parameters based on https://github.com/adafruit/Adafruit_ILI9341
  0xEF, 3, 0x03, 0x80, 0x02,
  0xCF, 3, 0x00, 0xC1, 0x30,
  0xED, 4, 0x64, 0x03, 0x12, 0x81,
  0xE8, 3, 0x85, 0x00, 0x78,
  0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
  0xF7, 1, 0x20,
  0xEA, 2, 0x00, 0x00,
  ILI9341_PWCTR1  , 1, 0x23,             // Power control VRH[5:0]
  ILI9341_PWCTR2  , 1, 0x10,             // Power control SAP[2:0];BT[3:0]
  ILI9341_VMCTR1  , 2, 0x3e, 0x28,       // VCM control
  ILI9341_VMCTR2  , 1, 0x86,             // VCM control2
  ILI9341_MADCTL  , 1, 0x08,             // Memory Access Control
  ILI9341_VSCRSADD, 1, 0x00,             // Vertical scroll zero
  ILI9341_PIXFMT  , 1, 0x55,
  ILI9341_FRMCTR1 , 2, 0x00, 0x18,
  ILI9341_DFUNCTR , 3, 0x08, 0x82, 0x27, // Display Function Control
  0xF2, 1, 0x00,                         // 3Gamma Function Disable
  ILI9341_GAMMASET , 1, 0x01,             // Gamma curve selected
  ILI9341_GMCTRP1 , 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, // Set Gamma
    0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
  ILI9341_GMCTRN1 , 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, // Set Gamma
    0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
  ILI9341_SLPOUT  , 0x80,                // Exit Sleep
    120,
  ILI9341_DISPON  , 0x80,                // Display on
    120,
  0x00, 0x00,                                // End of list
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
    // DMESG("win %d,%d %d,%d", x, y, w, h);
    bytesPerColumn = h >> 2;
    w += x - 1;
    h += y - 1;
    uint8_t cmd0[] = {ILI9341_RASET, 0, (uint8_t)x, (uint8_t)(w >> 8), (uint8_t)w};
    uint8_t cmd1[] = {ILI9341_CASET, 0, (uint8_t)y, (uint8_t)(h >> 8), (uint8_t)h};
    sendCmd(cmd1, sizeof(cmd1));
    sendCmd(cmd0, sizeof(cmd0));
}

static void configure(uint8_t madctl, uint32_t frmctr1) {
    uint8_t cmd0[] = {ILI9341_MADCTL, madctl};
    uint8_t cmd1[] = {ILI9341_FRMCTR1, (uint8_t)(frmctr1 >> 16), (uint8_t)(frmctr1 >> 8),
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

void screen_start_pixels(int x, int y, int w, int h) {
    setAddrWindow(x, y, w * 2, h * 2);
    startRAMWR(ILI9341_RAMWR);
}

void screen_sleep() {
    cmdBuf[0] = ILI9341_SLPIN;
    sendCmd(cmdBuf, 1);
}

void screen_set_backlight(int level) {
    pwm_set_duty(level);
    // pin_set(CFG(PIN_DISPLAY_BL), level > 0);
}

void screen_init() {
    pin_setup_output(CFG(PIN_DISPLAY_BL));
    pin_setup_output(CFG(PIN_DISPLAY_DC));
    pin_setup_output(CFG(PIN_DISPLAY_RST));
    pin_setup_output(CFG(PIN_DISPLAY_CS));
    pin_setup_output(CFG(PIN_DISPLAY_CS1));
    pin_setup_output(CFG(PIN_DISPLAY_RD));

#ifdef PROTO_V2
    pin_setup_output(CFG(PIN_DISPLAY_WR));
    pin_setup_output(PA_0);
    pin_setup_output(PA_1);
    pin_setup_output(PA_2);
    for (int i = 8; i <= 12; ++i)
        pin_setup_output(PA_0 + i);
#else
    for (int i = 0; i < 9; ++i)
        pin_setup_output(PB_0 + i);
#endif

    SET_CS(1);
    SET_DC(1);

    pin_set(CFG(PIN_DISPLAY_BL), 0);
    pwm_init(255, 0);

    pin_set(CFG(PIN_DISPLAY_CS1), 0);
    pin_set(CFG(PIN_DISPLAY_RD), 1); // active-low
    pin_set(CFG(PIN_DISPLAY_WR), 1); // active-low

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
        cmdBuf[0] = ILI9341_INVON;
        sendCmd(cmdBuf, 1);
    }
}

/*
function split(x) {
    const r = x >> 3
    const b = x & 31
    const g = ((x & 7) << 3) | (x >> 5)
    return { r, g, b }
}
let nums = []
const usedR = {}
const usedG = {}
const usedB = {}
for (let x = 0; x <= 255; ++x) {
    const { r, g, b } = split(x)
    if (usedR[r] || usedG[g] || usedB[b])
        continue
    usedR[r] = 1
    usedG[g] = 1
    usedB[b] = 1
    console.log(nums.length, x,r,g,b)
    nums.push(x)
}
console.log(nums)
*/

static uint32_t bsrrMasks[16];
static const uint8_t colorBytes[16] = {
    // we only use the first 16 entries
    0, 9, 18, 27, 33, 40, 51, 58, 66, 75, 80, 89, 99, 106, 113, 120,
    // 132, 141, 150, 159, 165, 172, 183, 190, 198, 207, 212, 221, 231, 238, 245, 252
};

#ifdef PROTO_V2
#define BSRR_FOR(x) (0x1f070000 | ((x)&0x07) | (((x) >> 3) << 8))
#define MASK_ACTIVE 0x80000000
#define MASK_IDLE 0x8000
#define PORT GPIOA
#else
#define BSRR_FOR(x) (0x00ff0000 | (x))
#define MASK_ACTIVE 0x1000000
#define MASK_IDLE 0x100
#define PORT GPIOB
#endif

void screen_send_palette(const uint32_t *palette) {
    memset(dataBuf, 0, sizeof(dataBuf));
    uint8_t *base = dataBuf;
    for (int i = 0; i < 16; ++i) {
        int x = colorBytes[i];
        int r = x >> 3;
        int b = x & 31;
        int g = ((x & 7) << 3) | (x >> 5);
        base[r] = (palette[i] >> 18) & 0x3f;
        base[g + 32] = (palette[i] >> 10) & 0x3f;
        base[b + 32 + 64] = (palette[i] >> 2) & 0x3f;
        bsrrMasks[i] = BSRR_FOR(x);
    }
    // DMESG("send pal");
    startRAMWR(ILI9341_RGBSET);
    transfer(dataBuf, 128);
    SET_CS(1);
}

#define STROBE()                                                                                   \
    do {                                                                                           \
        *bsrr = MASK_ACTIVE;                                                                       \
        *bsrr = MASK_IDLE;                                                                         \
    } while (0)

RAM_FUNC static void do_send(const uint8_t *ptr, uint32_t n, volatile uint32_t *bsrr,
                             uint32_t *bsrrMasks) {
    while (n--) {
        uint8_t v = *ptr++;
        *bsrr = bsrrMasks[v & 0xf];
        STROBE();
        STROBE();
        STROBE();
        STROBE();
        *bsrr = bsrrMasks[v >> 4];
        STROBE();
        STROBE();
        STROBE();
        STROBE();
    }
}

// functions are not static, since we don't want them inlined
// around 54 cycles per byte (which is pixel, since the line needs to be duplicated)
// 1mpix/s - should be good
void send_pixel_data(const uint8_t *ptr, uint32_t sz, volatile uint32_t *bsrr,
                     uint32_t *bsrrMasks) {
    *bsrr = MASK_IDLE;
    uint32_t cols = sz / bytesPerColumn;
    while (cols--) {
        do_send(ptr, bytesPerColumn, bsrr, bsrrMasks);
        do_send(ptr, bytesPerColumn, bsrr, bsrrMasks);
        ptr += bytesPerColumn;
    }
}

void send_any_data(const uint8_t *ptr, uint32_t sz, volatile uint32_t *bsrr) {
    *bsrr = MASK_IDLE;
    while (sz--) {
        uint8_t v = *ptr++;
        *bsrr = BSRR_FOR(v);
        STROBE();
    }
}

static void transfer(void *ptr, uint32_t len) {
    send_any_data(ptr, len, &PORT->BSRR);
}

void screen_send_indexed(const uint32_t *src, uint32_t numwords) {
    send_pixel_data((const uint8_t *)src, numwords << 2, &PORT->BSRR, bsrrMasks);
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
    startRAMWR(ILI9341_RAMWR);
    for (int i = 0; i < 16; ++i) {
        memset(line, 0x11 * i, sizeof(line));
        for (int j = 0; j < 10; ++j)
            screen_send_indexed(line, 64 / 4);
    }
    SET_CS(0);
}

#endif
