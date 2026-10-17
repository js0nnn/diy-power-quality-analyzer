/*
 * shared_mem.h
 *
 * CM7 -> CM4 hand-off of the computed power parameters.
 *
 * The block lives at the start of SRAM4 (D3 domain), which both cores can
 * access and neither linker script places anything in. CM7 is the only
 * writer, CM4 only reads. Consistency uses a sequence counter (seqlock):
 * the writer makes `seq` odd while it updates the payload, and the reader
 * retries until it sees the same even value before and after copying.
 *
 * The CM7 D-cache is not enabled in this project. If you enable it, mark
 * SRAM4 non-cacheable with the MPU or the CM4 will read stale data.
 */
#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stdint.h>
#include <string.h>

#if defined(CORE_CM7) || defined(CORE_CM4)
#include "stm32h7xx.h"
#define PQA_DMB() __DMB()
#else
#define PQA_DMB() ((void)0)
#endif

#define PQA_SHARED_ADDR   (0x38000000UL)      /* SRAM4 base */
#define PQA_SHARED_MAGIC  (0x50514131UL)      /* "PQA1" */

typedef struct
{
    float vrms;        /* V   */
    float irms;        /* A   */
    float p_real;      /* W   */
    float p_reactive;  /* VAR */
    float p_app;       /* VA  */
    float pf;          /* -1 .. 1 */
    float vthd;        /* ratio, 0.05 = 5 % */
    float ithd;        /* ratio */
    float vpeak;       /* V   */
    float ipeak;       /* A   */
} pqa_params_t;

typedef struct
{
    volatile uint32_t magic;     /* PQA_SHARED_MAGIC once CM7 has initialised the block */
    volatile uint32_t seq;       /* seqlock counter, odd while CM7 is writing           */
    volatile uint32_t frames;    /* waveform frames processed by CM7                    */
    volatile uint32_t ade_ok;    /* 1 when the ADE9000 initialised correctly            */
    pqa_params_t      params;
    char              esp_ip[32];/* filled in once the ESP8266 link exists, else ""     */
} pqa_shared_t;

#define PQA_SHARED ((pqa_shared_t *)PQA_SHARED_ADDR)

/* CM7: call once before the CM4 starts reading. */
static inline void pqa_shared_init(void)
{
    pqa_shared_t *s = PQA_SHARED;
    memset((void *)s, 0, sizeof(*s));
    PQA_DMB();
    s->magic = PQA_SHARED_MAGIC;
}

/* CM7: publish a new parameter set. */
static inline void pqa_shared_publish(const pqa_params_t *p)
{
    pqa_shared_t *s = PQA_SHARED;
    s->seq++;                       /* odd: write in progress */
    PQA_DMB();
    memcpy((void *)&s->params, p, sizeof(*p));
    s->frames++;
    PQA_DMB();
    s->seq++;                       /* even: consistent */
}

/* CM4: copy the latest consistent parameter set. Returns its sequence number,
 * or 0 if the CM7 has not initialised the block yet. */
static inline uint32_t pqa_shared_read(pqa_params_t *out)
{
    const pqa_shared_t *s = PQA_SHARED;
    if (s->magic != PQA_SHARED_MAGIC)
        return 0;

    uint32_t a, b;
    do {
        a = s->seq;
        PQA_DMB();
        memcpy(out, (const void *)&s->params, sizeof(*out));
        PQA_DMB();
        b = s->seq;
    } while ((a & 1u) || a != b);

    return a;
}

#endif /* SHARED_MEM_H */
