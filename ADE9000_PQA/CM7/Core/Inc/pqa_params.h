/*
 * pqa_params.h
 *
 * Power parameters computed on the CM7 from one waveform-buffer frame.
 * The maths mirrors visualizer_gui/visualizer.py so the LCD and the
 * desktop GUI show the same numbers. Keep the scaling constants below
 * in sync with the ones at the top of visualizer.py.
 */
#ifndef PQA_PARAMS_H
#define PQA_PARAMS_H

#include <stdint.h>
#include "../../../Common/Inc/shared_mem.h"

/* Scaling from PCF waveform codes, same as visualizer.py */
#define PQA_PCF_FS_CODES        74532013.0f
#define PQA_V_FS                (1.0f * 1001.0f)              /* 1 V peak x divider (1 M + 1 k) / 1 k */
#define PQA_ADC_FS_RMS_VOLTAGE  0.1765f
#define PQA_ROGOWSKI_V_PER_A    194e-6f
#define PQA_CURRENT_GAIN        1.42f
#define PQA_I_FS                (PQA_ADC_FS_RMS_VOLTAGE / PQA_ROGOWSKI_V_PER_A)

#define PQA_MAX_SAMPLES         256

/* Computes the parameters for `n` (<= PQA_MAX_SAMPLES) IA/VA sample pairs. */
void pqa_compute(const int32_t *ia, const int32_t *va, uint16_t n, pqa_params_t *out);

#endif /* PQA_PARAMS_H */
