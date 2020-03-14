#include "jdsimple.h"

#define CLK_EN __HAL_RCC_TIM3_CLK_ENABLE
#define TIMx TIM3
#define PIN PB_0
#define CHANNEL LL_TIM_CHANNEL_CH3
#define SET_COMPARE LL_TIM_OC_SetCompareCH3
#define PIN_AF LL_GPIO_AF_1

void pwm_init(uint32_t period, uint32_t duty) {
    CLK_EN();
    pin_setup_output_af(PIN, PIN_AF);

    LL_TIM_OC_InitTypeDef tim_oc_initstruct;

    TIMx->CR1 = LL_TIM_COUNTERMODE_UP | LL_TIM_CLOCKDIVISION_DIV1; // default anyways

    // set to 1us
    LL_TIM_SetPrescaler(TIMx, CPU_MHZ - 1);
    LL_TIM_SetAutoReload(TIMx, period - 1);

    LL_TIM_GenerateEvent_UPDATE(TIMx);
    LL_TIM_EnableARRPreload(TIMx);

    tim_oc_initstruct.OCMode = LL_TIM_OCMODE_PWM1;
    tim_oc_initstruct.OCState = LL_TIM_OCSTATE_DISABLE;
    tim_oc_initstruct.OCNState = LL_TIM_OCSTATE_DISABLE;
    tim_oc_initstruct.CompareValue = duty;
    tim_oc_initstruct.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
    tim_oc_initstruct.OCNPolarity = LL_TIM_OCPOLARITY_HIGH;
    tim_oc_initstruct.OCIdleState = LL_TIM_OCIDLESTATE_LOW;
    tim_oc_initstruct.OCNIdleState = LL_TIM_OCIDLESTATE_LOW;

    // this is quite involved - differetn config for every channel...
    LL_TIM_OC_Init(TIMx, CHANNEL, &tim_oc_initstruct);

    LL_TIM_OC_EnablePreload(TIMx, CHANNEL);
    LL_TIM_CC_EnableChannel(TIMx, CHANNEL);

    LL_TIM_EnableCounter(TIMx);
    LL_TIM_GenerateEvent_UPDATE(TIMx);
}

void pwm_set_duty(uint32_t duty) {
    SET_COMPARE(TIMx, duty);
}