/*
 * ui.h
 *
 * CM4 user interface: 20x4 I2C LCD + rotary encoder with push button.
 * Shows the parameters the CM7 publishes in SRAM4 (see shared_mem.h).
 *
 * Wiring on the NUCLEO-H755ZI-Q (all on the Zio connectors):
 *   LCD SDA     PF0  (I2C2_SDA, AF4)
 *   LCD SCL     PF1  (I2C2_SCL, AF4)
 *   Encoder A   PD12 (TIM4_CH1, AF2)
 *   Encoder B   PD13 (TIM4_CH2, AF2)
 *   Encoder SW  PG2  (EXTI2, active low, internal pull-up)
 *
 * The peripherals are set up here rather than through CubeMX, so
 * regenerating code from the .ioc does not remove them.
 */
#ifndef UI_H
#define UI_H

#define UI_I2C              I2C2
#define UI_I2C_TIMING       0x10C0ECFFu      /* 100 kHz with the 100 MHz D2 APB1 kernel clock */

#define UI_SDA_PORT         GPIOF
#define UI_SDA_PIN          GPIO_PIN_0
#define UI_SCL_PORT         GPIOF
#define UI_SCL_PIN          GPIO_PIN_1

#define UI_ENC_PORT         GPIOD
#define UI_ENC_A_PIN        GPIO_PIN_12
#define UI_ENC_B_PIN        GPIO_PIN_13

#define UI_SW_PORT          GPIOG
#define UI_SW_PIN           GPIO_PIN_2
#define UI_SW_IRQn          EXTI2_IRQn

#define UI_COUNTS_PER_DETENT 4               /* quadrature edges per encoder click */

void ui_init(void);
void ui_task(void);                          /* call from the CM4 main loop */

#endif /* UI_H */
