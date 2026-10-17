/*
 * lcd_i2c.c
 *
 * HD44780 over PCF8574, 4-bit mode. See lcd_i2c.h for the pin mapping.
 */
#include "lcd_i2c.h"

#define PIN_RS 0x01
#define PIN_EN 0x04
#define PIN_BL 0x08

#define I2C_TIMEOUT_MS 10

static I2C_HandleTypeDef *s_i2c;
static uint8_t s_addr;             /* 7-bit */
static uint8_t s_bl = PIN_BL;

static void expander_write(uint8_t data)
{
    data |= s_bl;
    HAL_I2C_Master_Transmit(s_i2c, (uint16_t)(s_addr << 1), &data, 1, I2C_TIMEOUT_MS);
}

static void pulse(uint8_t data)
{
    expander_write(data | PIN_EN);
    /* E pulse >= 450 ns: one I2C byte at 100 kHz already takes ~90 us */
    expander_write(data & (uint8_t)~PIN_EN);
}

static void write4(uint8_t nibble_hi, uint8_t mode)
{
    pulse((uint8_t)(nibble_hi & 0xF0) | mode);
}

static void send(uint8_t value, uint8_t mode)
{
    write4(value & 0xF0, mode);
    write4((uint8_t)(value << 4), mode);
}

static void command(uint8_t cmd)
{
    send(cmd, 0);
    if (cmd <= 0x02)              /* clear / home need ~1.5 ms */
        HAL_Delay(2);
}

bool lcd_init(I2C_HandleTypeDef *hi2c)
{
    static const uint8_t candidates[] = { 0x27, 0x3F };

    s_i2c = hi2c;
    s_addr = 0;
    for (unsigned k = 0; k < sizeof(candidates); k++) {
        if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(candidates[k] << 1), 3, I2C_TIMEOUT_MS) == HAL_OK) {
            s_addr = candidates[k];
            break;
        }
    }
    if (s_addr == 0)
        return false;

    /* HD44780 4-bit init sequence (datasheet figure 24) */
    HAL_Delay(50);
    expander_write(0);
    HAL_Delay(5);
    write4(0x30, 0); HAL_Delay(5);
    write4(0x30, 0); HAL_Delay(1);
    write4(0x30, 0); HAL_Delay(1);
    write4(0x20, 0); HAL_Delay(1);    /* switch to 4-bit */

    command(0x28);                    /* 4-bit, 2 logical lines, 5x8 font */
    command(0x0C);                    /* display on, cursor off, blink off */
    command(0x06);                    /* entry mode: increment, no shift */
    lcd_clear();
    return true;
}

uint8_t lcd_address(void)
{
    return s_addr;
}

void lcd_clear(void)
{
    command(0x01);
}

void lcd_set_cursor(uint8_t col, uint8_t row)
{
    static const uint8_t row_offset[LCD_ROWS] = { 0x00, 0x40, 0x14, 0x54 };
    if (row >= LCD_ROWS) row = LCD_ROWS - 1;
    if (col >= LCD_COLS) col = LCD_COLS - 1;
    command((uint8_t)(0x80 | (row_offset[row] + col)));
}

void lcd_write(const char *s)
{
    while (*s)
        send((uint8_t)*s++, PIN_RS);
}

void lcd_write_row(uint8_t row, const char *s)
{
    lcd_set_cursor(0, row);
    uint8_t n = 0;
    while (s[n] && n < LCD_COLS)
        send((uint8_t)s[n++], PIN_RS);
    while (n++ < LCD_COLS)
        send(' ', PIN_RS);
}

void lcd_backlight(bool on)
{
    s_bl = on ? PIN_BL : 0;
    expander_write(0);
}
