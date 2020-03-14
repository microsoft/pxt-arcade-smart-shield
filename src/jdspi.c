#include "jdsimple.h"
#include "jdspi.h"

#include "addata.h"

// timeout before we go to sleep if no host found
#define INITIAL_TIMEOUT_MS 5000

#if defined(BB_V0) || defined(BB_V1)
#define PIN_FLOW PA_11
#else
#define PIN_FLOW PA_3
#endif

#define PIN_SPIS_MISO PA_6

typedef uint8_t *PByte;

static uint8_t recv0[JDSPI_PKTSIZE], recv1[JDSPI_PKTSIZE];
static uint8_t send0[JDSPI_PKTSIZE];
static volatile bool sendBusy;
static bool abort_req, anyPkts;
static volatile PByte pendingPkt, pendingPkt2, currPkt;
static uint64_t lastPktTime;
static uint32_t disconnectedCycles;
bool jdspi_is_connected;

static inline bool sendFull() {
    jd_spi_packet_t *pkt = (jd_spi_packet_t *)send0;
    return pkt->pkt.service_number != 0xff;
}
static void not_ready() {
    pin_set(PIN_FLOW, 0);
}

static void setupSend(int service, int cmd, int size) {
    jd_spi_packet_t *pkt = (jd_spi_packet_t *)send0;
    pkt->magic = JDSPI_MAGIC;
    pkt->pkt.size = size;
    pkt->pkt.service_flags = 0;
    pkt->pkt.service_command = cmd;
    pkt->pkt.service_number = service;
}

static void clearSend() {
    memset(send0, 0, sizeof(send0));
    setupSend(0xff, 0, 0);
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
        if (sendBusy) {
            clearSend();
            sendBusy = false;
        }
        if (pendingPkt) {
            pendingPkt2 = currPkt;
            currPkt = NULL;
            return; // not ready
        } else {
            pendingPkt = currPkt;
        }
    }
    if (pendingPkt == recv0)
        currPkt = recv1;
    else
        currPkt = recv0;

    if (sendFull())
        sendBusy = true;

    // setup transfer
    spis_xfer(send0, currPkt, JDSPI_PKTSIZE, transfer_done);
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

static void send_ad_data(int serviceNo, void *dst) {
    memcpy(dst, adData, sizeof(adData));
    jdspi_send(serviceNo, 0, sizeof(adData));
}

void jdspi_send(uint32_t service, uint32_t cmd, uint32_t size) {
    if (size > JDSPI_PKTSIZE - 4)
        panic();
    if (service > 0xfe || cmd > 0xff)
        panic();
    setupSend(service, cmd, size);
}

static bool advertise;

static void process_one(jd_packet_t *pkt) {
    switch (pkt->service_number) {
    case 0:
        advertise = true;
        break;
    case 1:
        jd_display_incoming(pkt);
        break;
    case 2:
        jd_arcade_controls_incoming(pkt);
        break;
    }

    if (jd_display_frame_start)
        advertise = true;

    void *dst = send0 + JDSPI_HEADER_SIZE;

    if (!sendFull() && advertise) {
        advertise = false;
        send_ad_data(0, dst);
    }

    if (!sendFull())
        jd_display_outgoing(1, dst);

    if (!sendFull())
        jd_arcade_controls_outgoing(2, dst);
}

static void do_process() {
    jd_spi_packet_t *pkt = (jd_spi_packet_t *)pendingPkt;

    if (pkt->magic != JDSPI_MAGIC) {
        DMESG("magic mismatch: %x", pkt->magic);
        abort_req = true;
        return;
    }
    // DMESG("PKT sz=%d to:%d", pkt->size, pkt->service_number);
    while (pkt->magic == JDSPI_MAGIC) {
        process_one(&pkt->pkt);
        pkt = (jd_spi_packet_t *)(pkt->data + ((pkt->pkt.size + 3) & ~3));
        if ((uint8_t *)pkt > pendingPkt + JDSPI_PKTSIZE - 4)
            break;
    }
}

void jdspi_process() {
    if (abort_req) {
        reset_comms();
        return;
    }

    jd_display_process();
    jd_arcade_controls_process();

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

    clearSend();

    jdspi_early_init(); // just in case

    screen_init();

    // only initialize SPI-slave after screen init (which is 250ms) - this is used for shield
    // detection
    spis_init();
    transfer_done();
}
