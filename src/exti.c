#include "jdsimple.h"

static cb_t callbacks[16];
static cb_t trigger_cb;

static void check_line(int ln) {
    uint32_t pin = 1 << ln;

    if (LL_EXTI_IsActiveFallingFlag_0_31(pin) != RESET) {
        LL_EXTI_ClearFallingFlag_0_31(pin);
        callbacks[ln]();
    }
}

void EXTI0_1_IRQHandler() {
    cb_t f = trigger_cb;
    if (f) {
        trigger_cb = NULL;
        f();
    }
    check_line(0);
    check_line(1);
}
void EXTI2_3_IRQHandler() {
    check_line(2);
    check_line(3);
}
void EXTI4_15_IRQHandler() {
    for (int i = 4; i <= 15; ++i)
        check_line(i);
}

// run cb at EXTI IRQ priority
void exti_trigger(cb_t cb) {
    trigger_cb = cb;
    NVIC_SetPendingIRQ(EXTI0_1_IRQn);
    // LL_EXTI_GenerateSWI_0_31(1);
}

#ifdef STM32F0
#define LL_EXTI_CONFIG_PORTA 0
#define LL_EXTI_CONFIG_PORTB 1
#define LL_EXTI_CONFIG_PORTC 2
#endif

void exti_set_callback(GPIO_TypeDef *port, uint32_t pin, cb_t callback) {
    uint32_t extiport = 0;

    if (port == GPIOA)
        extiport = LL_EXTI_CONFIG_PORTA;
    else if (port == GPIOB)
        extiport = LL_EXTI_CONFIG_PORTB;
    else if (port == GPIOC)
        extiport = LL_EXTI_CONFIG_PORTC;
    else
        panic();

    int numcb = 0;
    for (uint32_t pos = 0; pos < 16; ++pos) {
        if (callbacks[pos])
            numcb++;
        if (pin & (1 << pos)) {
#ifdef STM32F0
            uint32_t line = (pos >> 2) | ((pos & 3) << 18);
            LL_SYSCFG_SetEXTISource(extiport, line);
#else
            uint32_t line = (pos >> 2) | ((pos & 3) << 19);
            LL_EXTI_SetEXTISource(extiport, line);
#endif
                LL_EXTI_EnableIT_0_31(1 << pos);
            LL_EXTI_EnableFallingTrig_0_31(1 << pos);
            callbacks[pos] = callback;
        }
    }

    if (!numcb) {
        NVIC_SetPriority(EXTI0_1_IRQn, 0);
        NVIC_EnableIRQ(EXTI0_1_IRQn);
        NVIC_SetPriority(EXTI2_3_IRQn, 0);
        NVIC_EnableIRQ(EXTI2_3_IRQn);
        NVIC_SetPriority(EXTI4_15_IRQn, 0);
        NVIC_EnableIRQ(EXTI4_15_IRQn);
    }
}