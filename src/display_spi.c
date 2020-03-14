#include "jdsimple.h"

#define SPI_IDX 2

#if SPI_IDX == 1
#define SPIx SPI1
#define IRQn SPI1_IRQn
#define IRQHandler SPI1_IRQHandler
#define LL_DMAMUX_REQ_SPIx_RX LL_DMAMUX_REQ_SPI1_RX
#define LL_DMAMUX_REQ_SPIx_TX LL_DMAMUX_REQ_SPI1_TX
#elif SPI_IDX == 2
#define SPIx SPI2
#define IRQn SPI2_IRQn
#define IRQHandler SPI2_IRQHandler
#define LL_DMAMUX_REQ_SPIx_RX LL_DMAMUX_REQ_SPI2_RX
#define LL_DMAMUX_REQ_SPIx_TX LL_DMAMUX_REQ_SPI2_TX
#else
#error "bad SPI"
#endif

#if SPI_IDX == 1
#define PIN_PORT GPIOA
#define PIN_SCK LL_GPIO_PIN_5
#define PIN_MOSI LL_GPIO_PIN_7
#define PIN_AF LL_GPIO_AF_0
#define SPI_CLK_ENABLE __HAL_RCC_SPI1_CLK_ENABLE
#elif SPI_IDX == 2
#define PIN_PORT GPIOB
#define SPI_CLK_ENABLE __HAL_RCC_SPI2_CLK_ENABLE
#ifdef STM32G0
#define PIN_SCK LL_GPIO_PIN_8
#define PIN_MOSI LL_GPIO_PIN_7
#define PIN_AF LL_GPIO_AF_1
#else
#define PIN_SCK LL_GPIO_PIN_13
#define PIN_MOSI LL_GPIO_PIN_15
#define PIN_AF LL_GPIO_AF_0
#endif
#else
#error "bad spi"
#endif

#ifdef STM32G0
#define DMA_CH LL_DMA_CHANNEL_1
#define DMA_IRQn DMA1_Channel1_IRQn
#define DMA_Handler DMA1_Channel1_IRQHandler
#define DMA_ClearFlag LL_DMA_ClearFlag_GI1
#else

#if SPI_IDX == 1
#define DMA_CH LL_DMA_CHANNEL_3
#define DMA_IRQn DMA1_Channel2_3_IRQn
#define DMA_Handler DMA1_Channel2_3_IRQHandler
#define DMA_ClearFlag LL_DMA_ClearFlag_GI3
#elif SPI_IDX == 2
#define DMA_CH LL_DMA_CHANNEL_5
#define DMA_IRQn DMA1_Channel4_5_IRQn
#define DMA_Handler DMA1_Channel4_5_IRQHandler
#define DMA_ClearFlag LL_DMA_ClearFlag_GI5
#else
#error "bad spi"
#endif

#endif

void dspi_init() {
    SPI_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = PIN_SCK | PIN_MOSI;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_DOWN;
    GPIO_InitStruct.Alternate = PIN_AF;
    LL_GPIO_Init(PIN_PORT, &GPIO_InitStruct);

    SPIx->CR1 = LL_SPI_HALF_DUPLEX_TX | LL_SPI_MODE_MASTER | LL_SPI_NSS_SOFT |
                LL_SPI_BAUDRATEPRESCALER_DIV2;
    SPIx->CR2 = LL_SPI_DATAWIDTH_8BIT | LL_SPI_RX_FIFO_TH_QUARTER;

    // LL_SPI_EnableIT_TXE(SPIx);
    // LL_SPI_EnableIT_RXNE(SPIx);
    LL_SPI_EnableIT_ERR(SPIx);

#ifdef STM32G0
    LL_DMA_SetPeriphRequest(DMA1, DMA_CH, LL_DMAMUX_REQ_SPIx_TX);
#endif
    LL_DMA_ConfigTransfer(DMA1, DMA_CH,
                          LL_DMA_DIRECTION_MEMORY_TO_PERIPH | //
                              LL_DMA_PRIORITY_LOW |           //
                              LL_DMA_MODE_NORMAL |            //
                              LL_DMA_PERIPH_NOINCREMENT |     //
                              LL_DMA_MEMORY_INCREMENT |       //
                              LL_DMA_PDATAALIGN_BYTE |        //
                              LL_DMA_MDATAALIGN_BYTE);

    /* Enable DMA transfer complete/error interrupts  */
    LL_DMA_EnableIT_TC(DMA1, DMA_CH);
    LL_DMA_EnableIT_TE(DMA1, DMA_CH);

    NVIC_SetPriority(DMA_IRQn, 1);
    NVIC_EnableIRQ(DMA_IRQn);

    NVIC_SetPriority(IRQn, 1);
    NVIC_EnableIRQ(IRQn);
}

static cb_t doneH;

void dspi_tx(const void *data, uint32_t numbytes, cb_t doneHandler) {
    // DMESG("dspi tx");

    if (doneH)
        panic();
    doneH = doneHandler;

    /* Reset the threshold bit */
    CLEAR_BIT(SPIx->CR2, SPI_CR2_LDMATX | SPI_CR2_LDMARX);

    LL_DMA_ConfigAddresses(DMA1, DMA_CH, (uint32_t)data, (uint32_t) & (SPIx->DR),
                           LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetDataLength(DMA1, DMA_CH, numbytes);

    LL_SPI_EnableDMAReq_TX(SPIx);
    LL_SPI_Enable(SPIx);
    LL_DMA_EnableChannel(DMA1, DMA_CH);
}

// this is only enabled for error events
void IRQHandler(void) {
    DMESG("SPI handler!");
}

void DMA_Handler(void) {
    // uint32_t isr = DMA1->ISR;

    DMA_ClearFlag(DMA1);
    LL_DMA_DisableChannel(DMA1, DMA_CH);

    // DMESG("DMA irq %x", isr);

    while (LL_SPI_GetTxFIFOLevel(SPIx))
        ;
    while (LL_SPI_IsActiveFlag_BSY(SPIx))
        ;

    // DMESG("DMA spi done waiting");

    cb_t f = doneH;
    if (f) {
        doneH = NULL;
        f();
    }
}
