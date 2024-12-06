// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifndef __JDSIMPLE_H
#define __JDSIMPLE_H

#define DEVICE_DMESG_BUFFER_SIZE 512

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "stm.h"

#include "dmesg.h"
#include "pinnames.h"
#include "jd_protocol.h"
#include "arcadegamepad.h"
#include "indexedscreen.h"
#include "arcadesound.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAM_FUNC __attribute__((noinline, long_call, section(".data")))

typedef void (*cb_t)(void);

// main.c
void panic(void);
void led_toggle();
void led_set(int state);
void pulse_log_pin();
void pulse_log_pin2();
void set_log_pin(int v);
void set_log_pin2(int v);
void set_log_pin3(int v);
void set_log_pin4(int v);
void set_log_pin5(int v);

// utils.c
void target_enable_irq();
void target_disable_irq();
void wait_us(int n);
int itoa(int n, char *s);
int string_reverse(char *s);
uint32_t random();
uint32_t random_around(uint32_t v);
uint64_t device_id();
void seed_random(uint32_t s);

// exti.c
void exti_set_callback(GPIO_TypeDef *port, uint32_t pin, cb_t callback);
void exti_trigger(cb_t cb);
#define exti_disable(pin) LL_EXTI_DisableIT_0_31(pin)
#define exti_enable(pin) LL_EXTI_EnableIT_0_31(pin)
#define exti_enable(pin) LL_EXTI_EnableIT_0_31(pin)
#define exti_clear(pin) LL_EXTI_ClearFallingFlag_0_31(pin)

// tim.c
void tim_init();
uint64_t tim_get_micros();
void tim_set_timer(int delta, cb_t cb);

// crc.c
uint16_t crc16(const void *data, uint32_t size);

// jdlow.c
void jd_init();
void jd_tx_completed(int errCode);
void jd_rx_completed(int dataLeft);
void jd_line_falling();
void jd_queue_packet(jd_packet_t *pkt);
uint32_t jd_get_num_pending_tx();
uint32_t jd_get_free_queue_space();

// display_spi.c
void dspi_init();
void dspi_tx(const void *data, uint32_t numbytes, cb_t doneHandler);

// spis.c
void spis_init();
void spis_xfer(const void *txdata, void *rxdata, uint32_t numbytes, cb_t doneHandler);
void spis_abort();
bool spis_seems_connected();
extern cb_t spis_halftransfer_cb;
extern cb_t spis_error_cb;

// screen.c
void screen_init();
void screen_stripes();
void screen_send_indexed(const uint32_t *src, uint32_t numwords);
void screen_send_palette(const uint32_t *palette);
void screen_start_pixels(int x, int y, int w, int h);
void screen_set_backlight(int level);
void screen_sleep();

// pins.c
void pin_set(int pin, int v);
void pin_setup_output(int pin);
void pin_toggle(int pin);
int pin_get(int pin);
// pull: -1, 0, 1
void pin_setup_input(int pin, int pull);
void pin_setup_output_af(int pin, int af);

// adc.c
void adc_init_random(void);

// pwm.c
void pwm_init(uint32_t period, uint32_t duty);
void pwm_set_duty(uint32_t duty);

// jdapp.c
void app_queue_annouce();
void app_handle_packet(jd_packet_t *pkt);
void app_process();

void jd_reset_frame(jd_frame_t *frame);
int jd_shift_frame(jd_frame_t *frame);
void *jd_push_in_frame(jd_frame_t *frame, unsigned service_num, unsigned service_cmd,
                       unsigned service_size);


#ifdef __cplusplus
}
#endif

#endif
