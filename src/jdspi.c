// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "jdsimple.h"
#include "jdspi.h"

// #define SOUND_TEST 1

// timeout before we go to sleep if no host found
#define INITIAL_TIMEOUT_MS 5000

#if defined(BB_V0) || defined(BB_V1)
#define PIN_FLOW PA_11
#else
#define PIN_FLOW PA_3
#endif

#define PIN_SPIS_MISO PA_6

typedef uint8_t *PByte;
typedef jd_frame_t *PFrame;

#define SEND_FILLING 0
#define SEND_READY 1
#define SEND_SENDING 2

static jd_frame_t recv0, recv1, send0;
static volatile uint8_t sendState;
static bool abort_req, anyPkts;
static volatile PFrame pendingPkt, pendingPkt2, currPkt;
static uint64_t lastPktTime;
static uint32_t disconnectedCycles;
bool jdspi_is_connected;

static void not_ready() {
    pin_set(PIN_FLOW, 0);
}

static void setConnected(bool isConn) {
    if (isConn)
        disconnectedCycles = 0;
    if (jdspi_is_connected == isConn)
        return;
    DMESG("conn %d", isConn);
    jdspi_is_connected = isConn;
    if (isConn)
        jdspi_connected();
    else
        jdspi_disconnected();
}

static void transfer_done() {
    not_ready(); // should be set by HalfTransfer, but just in case

    if (currPkt) {
        if (sendState == SEND_SENDING) {
            sendState = SEND_FILLING;
            memset(&send0, 0, sizeof(send0));
        }
        if (pendingPkt) {
            pendingPkt2 = currPkt;
            currPkt = NULL;
            return; // not ready
        } else {
            pendingPkt = currPkt;
        }
    }
    if (pendingPkt == &recv0)
        currPkt = &recv1;
    else
        currPkt = &recv0;

    if (sendState == SEND_READY) {
        sendState = SEND_SENDING;
        send0.crc = JDSPI_MAGIC;
    } else {
        send0.crc = JDSPI_MAGIC_NOOP;
    }

    // setup transfer
    spis_xfer(&send0, currPkt, sizeof(send0), transfer_done);
    // announce we're ready
    pin_set(PIN_FLOW, 1);
}

static void reset_comms() {
    abort_req = false;
    set_log_pin(1);
    pin_set(PIN_FLOW, 0);
    target_disable_irq();
    spis_abort();
    pendingPkt2 = NULL;
    pendingPkt = NULL;
    currPkt = NULL;
    target_enable_irq();
    wait_us(1000);
    set_log_pin(0);
    disconnectedCycles = 0;
    transfer_done();
}

static void error_cb() {
    DMESG("SPI error");
    abort_req = true;
    not_ready();
}

void *jdspi_send(unsigned service_num, unsigned service_cmd, const void *data, unsigned size) {
    void *p = jd_push_in_frame(&send0, service_num, service_cmd, size);
    if (p && data)
        memcpy(p, data, size);
    return p;
}

void jdspi_send_ad_data(unsigned service_num, bool *flag, const void *data, unsigned size) {
    if (*flag && jdspi_send(service_num, JD_CMD_ANNOUNCE, data, size))
        *flag = false;
}

static const uint32_t adData[] = {
    0x00000000,                      // 0 TODO?
    JD_SERVICE_CLASS_INDEXED_SCREEN, // 1
    JD_SERVICE_CLASS_ARCADE_GAMEPAD, // 2
#ifdef SOUND_TEST
    JD_SERVICE_CLASS_ARCADE_SOUND, // 3
#endif
};

static bool advertise;

static void process_one(jd_packet_t *pkt) {
    switch (pkt->service_number) {
    case 0:
        if (pkt->service_command == JD_CMD_ANNOUNCE)
            advertise = true;
        break;
    case 1:
        jd_display_incoming(pkt);
        break;
    case 2:
        jd_arcade_gamepad_incoming(pkt);
        break;
#ifdef SOUND_TEST
    case 3:
        jd_arcade_sound_incoming(pkt);
        break;
#endif
    }

    if (jd_display_frame_start)
        advertise = true;

    if (sendState == SEND_FILLING) {
        jdspi_send_ad_data(0, &advertise, adData, sizeof(adData));
        jd_display_outgoing(1);
        jd_arcade_gamepad_outgoing(2);
#ifdef SOUND_TEST
        jd_arcade_sound_outgoing(3);
#endif
        if (send0.size)
            sendState = SEND_READY;
    }
}

static void do_process() {
    jd_frame_t *pkt = pendingPkt;

    if (pkt->crc == JDSPI_MAGIC_NOOP)
        return;

    if (pkt->crc != JDSPI_MAGIC) {
        DMESG("magic mismatch: %x", pkt->crc);
        abort_req = true;
        return;
    }

    // DMESG("PKT sz=%d to:%d", pkt->size, pkt->service_number);
    for (;;) {
        process_one((jd_packet_t *)pkt);
        if (!jd_shift_frame(pkt))
            break;
    }
}

void jdspi_process() {
    if (abort_req) {
        reset_comms();
        return;
    }

    jd_display_process();
    jd_arcade_gamepad_process();
#ifdef SOUND_TEST
    jd_arcade_sound_process();
#endif

    uint64_t now = tim_get_micros();

    if (!pendingPkt) {
        if (!lastPktTime)
            lastPktTime = now;
        if (now - lastPktTime > INITIAL_TIMEOUT_MS * 1000) {
            lastPktTime = now;
            abort_req = true;
            if (!anyPkts)
                jdspi_disconnected();
        } else if (jdspi_is_connected && now - lastPktTime > 50000) {
            if (spis_seems_connected())
                disconnectedCycles = 0;
            else
                disconnectedCycles++;
            if (disconnectedCycles > 1000) {
                setConnected(false);
            }
        }
        return;
    }

    if (!anyPkts) {
        anyPkts = true;
        // pin_setup_input(PIN_HC_DATA, 0);
    }
    setConnected(true);

    while (1) {
        lastPktTime = now;
        disconnectedCycles = 0;
        do_process();

        target_disable_irq();
        pendingPkt = pendingPkt2;
        pendingPkt2 = NULL;
        target_enable_irq();

        if (currPkt == NULL && !abort_req)
            transfer_done();

        if (!pendingPkt)
            break;
    }
}

void jdspi_early_init() {
    // pull it down
    pin_setup_output(PIN_FLOW);
    pin_set(PIN_FLOW, 0);
    // pin_setup_output(PIN_HC_DATA);
    // pin_set(PIN_HC_DATA, 0);

    // drive the MISO line high while initializing the screen
    pin_setup_output(PIN_SPIS_MISO);
    pin_set(PIN_SPIS_MISO, 1);
}

void jdspi_init() {
    spis_halftransfer_cb = not_ready;
    spis_error_cb = error_cb;

    jdspi_early_init(); // just in case

    screen_init();

    // only initialize SPI-slave after screen init (which is 250ms) - this is used for shield
    // detection
    spis_init();
    transfer_done();
}
