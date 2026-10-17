#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stdint.h>

/* ---------------- ADE CONFIG ---------------- */
#define ADE9000_BURST_PHASE_A_BLOCK_START 0x680
#define ADE9000_BURST_PHASE_A_BLOCK_END   0x692

#define ADE9000_BURST_PHASE_A_REG_COUNT \
    (ADE9000_BURST_PHASE_A_BLOCK_END - ADE9000_BURST_PHASE_A_BLOCK_START + 1)

#define ADE9000_BURST_PHASE_A_BYTES (ADE9000_BURST_PHASE_A_REG_COUNT * 4)

/* ---------------- POWER PARAMS ---------------- */
typedef struct
{
    float vrms;
    float irms;
    float pf;
    float p_app;
    float p_real;
    float p_reactive;
    float vthd;
    float ithd;
    float vpeak;
    float ipeak;
} power_params_t;

/* ---------------- SHARED STRUCT ---------------- */
typedef struct
{
    uint32_t phase_regs[ADE9000_BURST_PHASE_A_REG_COUNT];
    volatile uint8_t new_data;

    power_params_t params;
    volatile uint8_t params_ready;

    /* -------- NEW -------- */
    char esp_ip[32];
    volatile uint8_t ip_ready;

} phase_shared_t;

/* ---------------- SHARED MEMORY ADDRESS ---------------- */
#define PHASE_SHARED_ADDR  ((phase_shared_t *)0x38000000)

#endif
