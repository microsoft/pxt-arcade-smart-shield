#include "jdsimple.h"
#include "jdspi.h"

#ifdef PSCREEN
#define PIN_LOG1 PA_12
#else
#define PIN_LOG1 PA_9
#endif

#ifdef BB_V1
#define PIN_LED PC_6
#define PIN_LOG0 PC_14
#elif defined(PROTO_V2)
#define PIN_LED PB_14
#define PIN_LOG0 PB_12
#undef PIN_LOG1
#define PIN_LOG1 PB_13
#else
#define PIN_LED PC_6
#define PIN_LOG0 PA_10
#endif


void led_init() {
    pin_setup_output(PIN_LOG0);
    pin_setup_output(PIN_LOG1);
    pin_setup_output(PIN_LED);
}

void set_log_pin(int v) {
    pin_set(PIN_LOG0, v);
}

void set_log_pin2(int v) {
    pin_set(PIN_LOG1, v);
}

void pulse_log_pin() {
    set_log_pin(1);
    set_log_pin(0);
}

void pulse_log_pin2() {
    set_log_pin2(1);
    set_log_pin2(0);
}

void led_toggle() {
    pin_toggle(PIN_LED);
}

void led_set(int state) {
    pin_set(PIN_LED, state);
}

// uint8_t crcBuf[1024];

static void tick() {
    // pulse_log_pin();
    tim_set_timer(10000, tick);
}

int main(void) {
    jdspi_early_init();
    led_init();

    tim_init();
    dspi_init();
    adc_init_random();

    tick();

    jdspi_init();

    uint64_t lastBlink = tim_get_micros();
    while (1) {
        if (tim_get_micros() - lastBlink > 300000) {
            lastBlink = tim_get_micros();
            led_toggle();
        }

        jdspi_process();
    }
}

void panic(void) {
    DMESG("PANIC!");
    target_disable_irq();
    while (1) {
        led_toggle();
        wait_us(100000);
    }
}
