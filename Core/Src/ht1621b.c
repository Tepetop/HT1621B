/**
 * @file    ht1621b.c
 * @brief   HT1621B RAM Mapping 32x4 LCD Controller driver — SPI implementation
 *
 * Uses hardware SPI to transmit the HT1621B serial protocol.
 * The non-byte-aligned frames are packed into a byte buffer and sent
 * in a single HAL_SPI_Transmit call.  Padding bits at the tail are
 * harmless — CS going HIGH resets the HT1621B serial interface.
 *
 * Wire protocol (from datasheet Rev.1.30):
 *   - Data latched on RISING edge of WR (= SCK in SPI Mode 0)
 *   - Mode ID / address: MSB first
 *   - Data nibbles: LSB first (D0, D1, D2, D3)
 *   - CS active LOW; HIGH resets serial interface
 *
 * Command frame : CS_LOW -> 100(3b) -> cmd(8b) -> X(1b) -> CS_HIGH   (12 bits)
 * Write frame   : CS_LOW -> 101(3b) -> addr(6b) -> data(4b×n)       -> CS_HIGH
 */

#include "ht1621b.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * SPI TX buffer size: enough for the largest frame
 *   WriteAll = 3 + 6 + 32*4 = 137 bits → ceil(137/8) = 18 bytes
 * -------------------------------------------------------------------------*/
#define HT1621B_TX_BUF_SIZE   18
#define HT1621B_SPI_TIMEOUT   10   /* ms */

/* Mode IDs (3-bit, MSB first) */
#define MODE_CMD    0x04   /* 100 */
#define MODE_WRITE  0x05   /* 101 */

/* ---------------------------------------------------------------------------
 * Bitstream builder — packs arbitrary bit sequences into a byte buffer.
 * SPI sends MSB of byte[0] first, which matches the packing order.
 * -------------------------------------------------------------------------*/
typedef struct {
    uint8_t  buf[HT1621B_TX_BUF_SIZE];
    uint16_t pos;   /* current bit position */
} BitStream;

static inline void bs_reset(BitStream *bs)
{
    memset(bs->buf, 0, HT1621B_TX_BUF_SIZE);
    bs->pos = 0;
}

static void bs_add_msb(BitStream *bs, uint8_t data, uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        if ((data >> (count - 1 - i)) & 1)
            bs->buf[bs->pos >> 3] |= (0x80 >> (bs->pos & 7));
        bs->pos++;
    }
}

static void bs_add_nibble_lsb(BitStream *bs, uint8_t nibble)
{
    for (uint8_t i = 0; i < 4; i++) {
        if ((nibble >> i) & 1)
            bs->buf[bs->pos >> 3] |= (0x80 >> (bs->pos & 7));
        bs->pos++;
    }
}

static void bs_transmit(HT1621B_HandleTypeDef *h, BitStream *bs)
{
    uint8_t len = (uint8_t)((bs->pos + 7) >> 3);
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(h->hspi, bs->buf, len, HT1621B_SPI_TIMEOUT);
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_SET);
}



/* ---------------------------------------------------------------------------
 * Public API — Core
 * -------------------------------------------------------------------------*/

void HT1621B_SendCommand(HT1621B_HandleTypeDef *h, uint8_t cmd)
{
    BitStream bs;
    bs_reset(&bs);
    bs_add_msb(&bs, MODE_CMD, 3);   /* 100 */
    bs_add_msb(&bs, cmd, 8);
    bs_add_msb(&bs, 0, 1);          /* don't-care bit */
    bs_transmit(h, &bs);            /* 12 bits → 2 bytes */
}

void HT1621B_Write(HT1621B_HandleTypeDef *h, uint8_t addr, uint8_t data)
{
    if (addr >= HT1621B_RAM_SIZE)
        return;

    h->ram[addr] = data & 0x0F;

    BitStream bs;
    bs_reset(&bs);
    bs_add_msb(&bs, MODE_WRITE, 3);
    bs_add_msb(&bs, addr, 6);
    bs_add_nibble_lsb(&bs, h->ram[addr]);
    bs_transmit(h, &bs);            /* 13 bits → 2 bytes */
}

void HT1621B_WriteBurst(HT1621B_HandleTypeDef *h, uint8_t addr,
                         const uint8_t *data, uint8_t len)
{
    if (addr >= HT1621B_RAM_SIZE)
        return;
    if (addr + len > HT1621B_RAM_SIZE)
        len = HT1621B_RAM_SIZE - addr;

    BitStream bs;
    bs_reset(&bs);
    bs_add_msb(&bs, MODE_WRITE, 3);
    bs_add_msb(&bs, addr, 6);

    for (uint8_t i = 0; i < len; i++) {
        h->ram[addr + i] = data[i] & 0x0F;
        bs_add_nibble_lsb(&bs, h->ram[addr + i]);
    }

    bs_transmit(h, &bs);
}

void HT1621B_WriteAll(HT1621B_HandleTypeDef *h)
{
    BitStream bs;
    bs_reset(&bs);
    bs_add_msb(&bs, MODE_WRITE, 3);
    bs_add_msb(&bs, 0, 6);           /* start at address 0 */

    for (uint8_t i = 0; i < h->num_segments; i++)
        bs_add_nibble_lsb(&bs, h->ram[i] & 0x0F);

    bs_transmit(h, &bs);
}

void HT1621B_Clear(HT1621B_HandleTypeDef *h)
{
    memset(h->ram, 0x00, h->num_segments);
    HT1621B_WriteAll(h);
}

void HT1621B_SetAll(HT1621B_HandleTypeDef *h)
{
    memset(h->ram, 0x0F, h->num_segments);
    HT1621B_WriteAll(h);
}

void HT1621B_LcdOff(HT1621B_HandleTypeDef *h)
{
    HT1621B_SendCommand(h, HT1621B_CMD_LCD_OFF);
}

void HT1621B_LcdOn(HT1621B_HandleTypeDef *h)
{
    HT1621B_SendCommand(h, HT1621B_CMD_LCD_ON);
}

void HT1621B_PowerDown(HT1621B_HandleTypeDef *h)
{
    HT1621B_SendCommand(h, HT1621B_CMD_LCD_OFF);
    HT1621B_SendCommand(h, HT1621B_CMD_SYS_DIS);
}

void HT1621B_PowerUp(HT1621B_HandleTypeDef *h)
{
    HT1621B_SendCommand(h, HT1621B_CMD_SYS_EN);
    HT1621B_SendCommand(h, HT1621B_CMD_RC_256K);
    HT1621B_SendCommand(h, HT1621B_CMD_LCD_ON);
}

HAL_StatusTypeDef HT1621B_Init(HT1621B_HandleTypeDef *h, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t num_segments)
{
    if (h == NULL || hspi == NULL || cs_port == NULL)
        return HAL_ERROR;
    if (num_segments == 0 || num_segments > HT1621B_MAX_SEG)
        return HAL_ERROR;

    h->hspi         = hspi;
    h->cs_port      = cs_port;
    h->cs_pin       = cs_pin;
    h->num_segments = num_segments;
    memset(h->ram, 0, HT1621B_RAM_SIZE);

    /* Recommended init sequence (datasheet p.13) */
    HT1621B_SendCommand(h, HT1621B_CMD_SYS_EN);
    HT1621B_SendCommand(h, HT1621B_CMD_RC_256K);
    HT1621B_SendCommand(h, HT1621B_CMD_BIAS_THIRD_4COM);
    HT1621B_SendCommand(h, HT1621B_CMD_LCD_ON);

    HT1621B_Clear(h);

    return HAL_OK;
}


