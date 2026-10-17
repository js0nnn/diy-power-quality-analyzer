/*
 * ui.c
 *
 * Pages: instantaneous values, power components, power quality, diagnostics.
 * Turn the encoder to move through the menu, press it to open the menu or
 * pick a page. Values refresh every 500 ms from the CM7's shared block.
 */
#include "ui.h"
#include "lcd_i2c.h"
#include "main.h"
#include "../../../Common/Inc/shared_mem.h"

#include <stdio.h>
#include <stdbool.h>

#define REFRESH_MS      500
#define STALE_MS        2000
#define DEBOUNCE_MS     200

typedef enum
{
    PAGE_INST = 0,
    PAGE_POWERCOMP,
    PAGE_POWERQUALITY,
    PAGE_DIAGNOSTICS,
    PAGE_COUNT
} page_t;

static const char *const menu_items[PAGE_COUNT] =
{
    "Inst Values",
    "Power Comp",
    "Power Quality",
    "Diagnostics"
};

static I2C_HandleTypeDef hi2c_ui;
static TIM_HandleTypeDef htim_enc;

static page_t  current_page = PAGE_INST;
static bool    menu_active;
static int     menu_index;
static int     menu_top;
static int16_t last_count;
static int     enc_accum;

static volatile bool sw_pressed;
static uint32_t last_press;

static pqa_params_t params;
static uint32_t last_seq;
static uint32_t last_seq_change;
static bool     have_data;
static bool     lcd_ok;

/* ---------------------------------------------------------------- peripherals */

static void ui_gpio_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* I2C2: open-drain, the backpack has its own pull-ups but enable ours too */
    g.Pin = UI_SDA_PIN | UI_SCL_PIN;
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init(UI_SDA_PORT, &g);

    /* TIM4 encoder inputs */
    g.Pin = UI_ENC_A_PIN | UI_ENC_B_PIN;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(UI_ENC_PORT, &g);

    /* Push button, falling edge */
    g.Pin = UI_SW_PIN;
    g.Mode = GPIO_MODE_IT_FALLING;
    g.Pull = GPIO_PULLUP;
    g.Alternate = 0;
    HAL_GPIO_Init(UI_SW_PORT, &g);

    HAL_NVIC_SetPriority(UI_SW_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(UI_SW_IRQn);
}

static void ui_i2c_init(void)
{
    __HAL_RCC_I2C2_CLK_ENABLE();

    hi2c_ui.Instance = UI_I2C;
    hi2c_ui.Init.Timing = UI_I2C_TIMING;
    hi2c_ui.Init.OwnAddress1 = 0;
    hi2c_ui.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c_ui.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c_ui.Init.OwnAddress2 = 0;
    hi2c_ui.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c_ui.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c_ui.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c_ui) != HAL_OK)
        Error_Handler();
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c_ui, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
        Error_Handler();
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c_ui, 0) != HAL_OK)
        Error_Handler();
}

static void ui_encoder_init(void)
{
    TIM_Encoder_InitTypeDef e = {0};

    __HAL_RCC_TIM4_CLK_ENABLE();

    htim_enc.Instance = TIM4;
    htim_enc.Init.Prescaler = 0;
    htim_enc.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_enc.Init.Period = 0xFFFF;
    htim_enc.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim_enc.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    e.EncoderMode = TIM_ENCODERMODE_TI12;
    e.IC1Polarity = TIM_ICPOLARITY_RISING;
    e.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC1Prescaler = TIM_ICPSC_DIV1;
    e.IC1Filter = 8;
    e.IC2Polarity = TIM_ICPOLARITY_RISING;
    e.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC2Prescaler = TIM_ICPSC_DIV1;
    e.IC2Filter = 8;
    if (HAL_TIM_Encoder_Init(&htim_enc, &e) != HAL_OK)
        Error_Handler();
    if (HAL_TIM_Encoder_Start(&htim_enc, TIM_CHANNEL_ALL) != HAL_OK)
        Error_Handler();
}

void EXTI2_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(UI_SW_PIN);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == UI_SW_PIN)
        sw_pressed = true;            /* handled in ui_task(), never draw from an ISR */
}

/* ---------------------------------------------------------------- formatting */

/* Fixed-point formatting, so no float printf support is needed (newlib-nano). */
static void fmt_fixed(char *out, size_t n, float v, int decimals)
{
    static const int32_t scale[] = { 1, 10, 100, 1000 };
    if (decimals < 0) decimals = 0;
    if (decimals > 3) decimals = 3;

    bool neg = v < 0.0f;
    if (neg) v = -v;
    int32_t x = (int32_t)(v * scale[decimals] + 0.5f);
    int32_t whole = x / scale[decimals];
    int32_t frac = x % scale[decimals];

    if (decimals == 0)
        snprintf(out, n, "%s%ld", neg ? "-" : "", (long)whole);
    else
        snprintf(out, n, "%s%ld.%0*ld", neg ? "-" : "", (long)whole, decimals, (long)frac);
}

/* ---------------------------------------------------------------- pages */

static void row_value(uint8_t row, const char *label, float v, int dec, const char *unit)
{
    char num[16], line[LCD_COLS + 1];
    if (have_data) {
        fmt_fixed(num, sizeof num, v, dec);
        snprintf(line, sizeof line, "%s%s %s", label, num, unit);
    } else {
        snprintf(line, sizeof line, "%s--- %s", label, unit);
    }
    lcd_write_row(row, line);
}

static void page_inst(void)
{
    char v[12], i[12], line[LCD_COLS + 1];
    lcd_write_row(0, "PHASE-A: INST VALUES");
    if (have_data) {
        fmt_fixed(v, sizeof v, params.vrms, 1);
        fmt_fixed(i, sizeof i, params.irms, 2);
        snprintf(line, sizeof line, "V:%sV  I:%sA", v, i);
    } else {
        snprintf(line, sizeof line, "V:---V  I:---A");
    }
    lcd_write_row(1, line);
    row_value(2, "PF:", params.pf, 3, "");
    row_value(3, "P: ", params.p_real, 1, "W");
}

static void page_powercomp(void)
{
    lcd_write_row(0, "PHASE-A: POWER COMP");
    row_value(1, "P: ", params.p_real, 1, "W");
    row_value(2, "Q: ", params.p_reactive, 1, "VAR");
    row_value(3, "S: ", params.p_app, 1, "VA");
}

static void page_powerquality(void)
{
    char v[12], i[12], line[LCD_COLS + 1];
    lcd_write_row(0, "PHASE-A: PWR QUALITY");
    row_value(1, "VTHD: ", params.vthd * 100.0f, 1, "%");
    row_value(2, "ITHD: ", params.ithd * 100.0f, 1, "%");
    if (have_data) {
        fmt_fixed(v, sizeof v, params.vpeak, 0);
        fmt_fixed(i, sizeof i, params.ipeak, 2);
        snprintf(line, sizeof line, "PK:%sV %sA", v, i);
    } else {
        snprintf(line, sizeof line, "PK:---V ---A");
    }
    lcd_write_row(3, line);
}

static void page_diagnostics(void)
{
    const pqa_shared_t *s = PQA_SHARED;
    char line[LCD_COLS + 1];

    lcd_write_row(0, "DIAGNOSTICS");

    if (have_data)
        snprintf(line, sizeof line, "CM7:OK fr:%lu", (unsigned long)s->frames);
    else
        snprintf(line, sizeof line, "CM7:NO DATA");
    lcd_write_row(1, line);

    lcd_write_row(2, (s->magic == PQA_SHARED_MAGIC && s->ade_ok) ? "ADE9000:OK" : "ADE9000:NOT READY");

    if (s->magic == PQA_SHARED_MAGIC && s->esp_ip[0] != '\0')
        snprintf(line, sizeof line, "IP:%.17s", s->esp_ip);
    else
        snprintf(line, sizeof line, "ESP8266:NOT LINKED");
    lcd_write_row(3, line);
}

static void draw_page(void)
{
    switch (current_page) {
    case PAGE_INST:         page_inst();         break;
    case PAGE_POWERCOMP:    page_powercomp();    break;
    case PAGE_POWERQUALITY: page_powerquality(); break;
    case PAGE_DIAGNOSTICS:  page_diagnostics();  break;
    default:                                     break;
    }
}

static void draw_menu(void)
{
    char line[LCD_COLS + 1];
    lcd_write_row(0, "--- MAIN MENU ------");
    for (int r = 0; r < 3; r++) {
        int idx = menu_top + r;
        if (idx < PAGE_COUNT)
            snprintf(line, sizeof line, "%s%s", idx == menu_index ? "> " : "  ", menu_items[idx]);
        else
            line[0] = '\0';
        lcd_write_row((uint8_t)(r + 1), line);
    }
}

/* ---------------------------------------------------------------- input */

static int encoder_steps(void)
{
    int16_t count = (int16_t)__HAL_TIM_GET_COUNTER(&htim_enc);
    int16_t diff = (int16_t)(count - last_count);   /* wraps correctly across 0xFFFF */
    last_count = count;
    enc_accum += diff;

    int steps = 0;
    while (enc_accum >= UI_COUNTS_PER_DETENT)  { enc_accum -= UI_COUNTS_PER_DETENT; steps++; }
    while (enc_accum <= -UI_COUNTS_PER_DETENT) { enc_accum += UI_COUNTS_PER_DETENT; steps--; }
    return steps;
}

static void update_data(void)
{
    uint32_t seq = pqa_shared_read(&params);
    uint32_t now = HAL_GetTick();
    if (seq != 0 && seq != last_seq) {
        last_seq = seq;
        last_seq_change = now;
    }
    have_data = (last_seq != 0) && (now - last_seq_change < STALE_MS);
}

/* ---------------------------------------------------------------- public */

void ui_init(void)
{
    ui_gpio_init();
    ui_i2c_init();
    ui_encoder_init();
    last_count = (int16_t)__HAL_TIM_GET_COUNTER(&htim_enc);

    lcd_ok = lcd_init(&hi2c_ui);
    if (lcd_ok) {
        lcd_write_row(1, "   POWER QUALITY");
        lcd_write_row(2, "      ANALYZER");
        HAL_Delay(1500);
        lcd_clear();
    }
}

void ui_task(void)
{
    static uint32_t last_refresh;
    uint32_t now = HAL_GetTick();

    if (!lcd_ok)
        return;

    update_data();

    if (sw_pressed) {
        sw_pressed = false;
        if (now - last_press >= DEBOUNCE_MS) {
            last_press = now;
            if (!menu_active) {
                menu_active = true;
                menu_index = current_page;
                draw_menu();
            } else {
                menu_active = false;
                current_page = (page_t)menu_index;
                lcd_clear();
                draw_page();
                last_refresh = now;
            }
        }
    }

    int steps = encoder_steps();
    if (menu_active && steps != 0) {
        menu_index = ((menu_index + steps) % PAGE_COUNT + PAGE_COUNT) % PAGE_COUNT;
        if (menu_index < menu_top)      menu_top = menu_index;
        if (menu_index >= menu_top + 3) menu_top = menu_index - 2;
        draw_menu();
    }

    if (!menu_active && now - last_refresh >= REFRESH_MS) {
        last_refresh = now;
        draw_page();
    }
}
