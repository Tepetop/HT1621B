/**
 * @file    ht1621b.h
 * @brief   HT1621B RAM Mapping 32x4 LCD Controller driver for STM32 (HAL SPI)
 *
 * Uses hardware SPI instead of bit-banging. The HT1621B 3-wire serial
 * protocol (CS, WR/CLK, DATA) maps to SPI as:
 *   SCK  = WR   (clock)
 *   MOSI = DATA (serial data)
 *   CS   = CS   (active LOW, software-managed GPIO)
 *
 * Required SPI configuration:
 *   - Master mode
 *   - SPI Mode 0: CPOL=0 (SPI_POLARITY_LOW), CPHA=0 (SPI_PHASE_1EDGE)
 *   - MSB first (SPI_FIRSTBIT_MSB)
 *   - 8-bit data size
 *   - Software NSS management (SPI_NSS_SOFT)
 *   - Baud rate <= 150 kHz @3.3V  or  <= 300 kHz @5V  (datasheet AC spec)
 *
 * The driver packs the non-byte-aligned HT1621B protocol into a byte buffer
 * and sends it in a single SPI transfer. Padding bits at the end are
 * discarded when CS goes HIGH (serial interface reset per datasheet).
 *
 * Reference: Holtek HT1621 datasheet Rev.1.30
 */

#ifndef HT1621B_H
#define HT1621B_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * HT1621B RAM size
 * -------------------------------------------------------------------------*/
#define HT1621B_RAM_SIZE    32   /* 32 addresses x 4 bits each = 128 segments */

/* Maximum number of SEG outputs on the HT1621B chip */
#define HT1621B_MAX_SEG     32

/* ---------------------------------------------------------------------------
 * Command definitions  (8-bit value, sent after mode ID 100)
 * -------------------------------------------------------------------------*/

/* System */
#define HT1621B_CMD_SYS_DIS     0x00
#define HT1621B_CMD_SYS_EN      0x01
#define HT1621B_CMD_LCD_OFF     0x02
#define HT1621B_CMD_LCD_ON      0x03

/* Timer / WDT */
#define HT1621B_CMD_TIMER_DIS   0x04
#define HT1621B_CMD_WDT_DIS     0x05
#define HT1621B_CMD_TIMER_EN    0x06
#define HT1621B_CMD_WDT_EN      0x07
#define HT1621B_CMD_CLR_TIMER   0x0C
#define HT1621B_CMD_CLR_WDT     0x0E

/* Tone */
#define HT1621B_CMD_TONE_OFF    0x08
#define HT1621B_CMD_TONE_ON     0x09
#define HT1621B_CMD_TONE_4K     0x40
#define HT1621B_CMD_TONE_2K     0x60

/* Oscillator source */
#define HT1621B_CMD_XTAL_32K    0x14
#define HT1621B_CMD_RC_256K     0x18
#define HT1621B_CMD_EXT_256K    0x1C

/* Bias & COM configuration */
#define HT1621B_CMD_BIAS_HALF_2COM   0x20
#define HT1621B_CMD_BIAS_HALF_3COM   0x24
#define HT1621B_CMD_BIAS_HALF_4COM   0x28
#define HT1621B_CMD_BIAS_THIRD_2COM  0x21
#define HT1621B_CMD_BIAS_THIRD_3COM  0x25
#define HT1621B_CMD_BIAS_THIRD_4COM  0x29

/* IRQ */
#define HT1621B_CMD_IRQ_DIS     0x80
#define HT1621B_CMD_IRQ_EN      0x88

/* Timebase / WDT frequency */
#define HT1621B_CMD_F1          0xA0
#define HT1621B_CMD_F2          0xA1
#define HT1621B_CMD_F4          0xA2
#define HT1621B_CMD_F8          0xA3
#define HT1621B_CMD_F16         0xA4
#define HT1621B_CMD_F32         0xA5
#define HT1621B_CMD_F64         0xA6
#define HT1621B_CMD_F128        0xA7

/* Test / Normal */
#define HT1621B_CMD_TEST        0xE0
#define HT1621B_CMD_NORMAL      0xE3

/* ---------------------------------------------------------------------------
 * Handle — SPI + CS GPIO, RAM buffer
 * -------------------------------------------------------------------------*/
typedef struct {
    SPI_HandleTypeDef *hspi;               /* HAL SPI handle                   */
    GPIO_TypeDef      *cs_port;            /* CS GPIO port                     */
    uint16_t           cs_pin;             /* CS GPIO pin                      */

    uint8_t            num_segments;       /* active SEG count (e.g. 13)       */
    uint8_t            ram[HT1621B_RAM_SIZE]; /* shadow RAM (lower 4 bits)     */
} HT1621B_HandleTypeDef;

/* ---------------------------------------------------------------------------
 * Core API
 * -------------------------------------------------------------------------*/

HAL_StatusTypeDef HT1621B_Init(HT1621B_HandleTypeDef *h,
                                SPI_HandleTypeDef *hspi,
                                GPIO_TypeDef *cs_port, uint16_t cs_pin,
                                uint8_t num_segments);

void HT1621B_SendCommand(HT1621B_HandleTypeDef *h, uint8_t cmd);

void HT1621B_Write(HT1621B_HandleTypeDef *h, uint8_t addr, uint8_t data);
void HT1621B_WriteBurst(HT1621B_HandleTypeDef *h, uint8_t addr,
                         const uint8_t *data, uint8_t len);
void HT1621B_WriteAll(HT1621B_HandleTypeDef *h);

void HT1621B_Clear(HT1621B_HandleTypeDef *h);
void HT1621B_SetAll(HT1621B_HandleTypeDef *h);

void HT1621B_LcdOff(HT1621B_HandleTypeDef *h);
void HT1621B_LcdOn(HT1621B_HandleTypeDef *h);
void HT1621B_PowerDown(HT1621B_HandleTypeDef *h);
void HT1621B_PowerUp(HT1621B_HandleTypeDef *h);



#ifdef __cplusplus
}
#endif

#endif /* HT1621B_H */
