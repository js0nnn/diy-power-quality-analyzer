/*
 * lcd_i2c.h
 *
 * Minimal driver for an HD44780 character LCD behind a PCF8574 I2C backpack
 * (the common blue/green "I2C LCD" modules). Standard backpack wiring:
 * P0 = RS, P1 = RW, P2 = E, P3 = backlight, P4..P7 = D4..D7.
 */
#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32h7xx_hal.h"

#define LCD_COLS 20
#define LCD_ROWS 4

/* Probes 0x27 then 0x3F (PCF8574 / PCF8574A defaults) and initialises the LCD.
 * Returns false if no backpack answers. */
bool lcd_init(I2C_HandleTypeDef *hi2c);
uint8_t lcd_address(void);              /* 7-bit address in use, 0 if none */

void lcd_clear(void);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_write(const char *s);
void lcd_write_row(uint8_t row, const char *s);  /* writes s padded/cut to LCD_COLS */
void lcd_backlight(bool on);

#endif /* LCD_I2C_H */
