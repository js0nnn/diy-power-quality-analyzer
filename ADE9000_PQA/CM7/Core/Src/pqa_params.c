/*
 * pqa_params.c
 *
 * C port of compute_params() in visualizer_gui/visualizer.py:
 * DC removal, RMS, real/apparent/reactive power, PF, peaks and a
 * Hann-windowed THD with a +/-2 bin fundamental band.
 */
#include "pqa_params.h"
#include <math.h>

#define THD_BAND 2

static float s_v[PQA_MAX_SAMPLES];
static float s_i[PQA_MAX_SAMPLES];
static float s_cos[PQA_MAX_SAMPLES];
static float s_sin[PQA_MAX_SAMPLES];
static float s_hann[PQA_MAX_SAMPLES];
static uint16_t s_tables_n;

static void build_tables(uint16_t n)
{
    const float two_pi = 6.28318530718f;
    for (uint16_t k = 0; k < n; k++) {
        s_cos[k]  = cosf(two_pi * k / n);
        s_sin[k]  = sinf(two_pi * k / n);
        s_hann[k] = 0.5f - 0.5f * cosf(two_pi * k / (n - 1));   /* matches np.hanning */
    }
    s_tables_n = n;
}

/* Magnitude of DFT bin `bin` of the windowed signal, normalised by n/2. */
static float bin_mag(const float *x, uint16_t n, uint16_t bin)
{
    float re = 0.0f, im = 0.0f;
    uint32_t idx = 0;
    for (uint16_t k = 0; k < n; k++) {
        float w = x[k] * s_hann[k];
        re += w * s_cos[idx];
        im -= w * s_sin[idx];
        idx += bin;
        if (idx >= n) idx -= n;
    }
    return sqrtf(re * re + im * im) / (n / 2.0f);
}

static float thd_ratio(const float *x, uint16_t n)
{
    static float mag[PQA_MAX_SAMPLES / 2 + 1];
    const uint16_t bins = n / 2 + 1;

    uint16_t fund = 1;
    for (uint16_t b = 1; b < bins; b++) {          /* bin 0 (DC) excluded */
        mag[b] = bin_mag(x, n, b);
        if (mag[b] > mag[fund]) fund = b;
    }

    uint16_t start = (fund > THD_BAND + 1) ? fund - THD_BAND : 1;
    uint16_t end   = (fund + THD_BAND + 1 < bins) ? fund + THD_BAND + 1 : bins;

    float f2 = 0.0f, h2 = 0.0f;
    for (uint16_t b = 1; b < bins; b++) {
        float m2 = mag[b] * mag[b];
        if (b >= start && b < end) f2 += m2;
        else                       h2 += m2;
    }
    return (f2 > 0.0f) ? sqrtf(h2) / sqrtf(f2) : 0.0f;
}

void pqa_compute(const int32_t *ia, const int32_t *va, uint16_t n, pqa_params_t *out)
{
    if (n < 4 || n > PQA_MAX_SAMPLES) {
        return;
    }
    if (s_tables_n != n) {
        build_tables(n);
    }

    float v_mean = 0.0f, i_mean = 0.0f;
    for (uint16_t k = 0; k < n; k++) {
        s_v[k] = ((float)va[k] / PQA_PCF_FS_CODES) * PQA_V_FS;
        s_i[k] = ((float)ia[k] / PQA_PCF_FS_CODES) * PQA_I_FS * PQA_CURRENT_GAIN;
        v_mean += s_v[k];
        i_mean += s_i[k];
    }
    v_mean /= n;
    i_mean /= n;

    /* double accumulators: the M7 has a DP FPU, and Q = sqrt(S^2 - P^2) cancels badly in float near PF = 1 */
    double v2 = 0.0, i2 = 0.0, p = 0.0;
    float vpk = 0.0f, ipk = 0.0f;
    for (uint16_t k = 0; k < n; k++) {
        s_v[k] -= v_mean;
        s_i[k] -= i_mean;
        v2 += (double)s_v[k] * s_v[k];
        i2 += (double)s_i[k] * s_i[k];
        p  += (double)s_v[k] * s_i[k];
        if (fabsf(s_v[k]) > vpk) vpk = fabsf(s_v[k]);
        if (fabsf(s_i[k]) > ipk) ipk = fabsf(s_i[k]);
    }

    double vrms = sqrt(v2 / n);
    double irms = sqrt(i2 / n);
    double preal = p / n;
    double papp = vrms * irms;
    double q2 = papp * papp - preal * preal;

    out->vrms       = (float)vrms;
    out->irms       = (float)irms;
    out->p_real     = (float)preal;
    out->p_app      = (float)papp;
    out->pf         = (papp != 0.0) ? (float)(preal / papp) : 0.0f;
    out->p_reactive = (q2 > 0.0) ? (float)sqrt(q2) : 0.0f;
    out->vpeak      = vpk;
    out->ipeak      = ipk;
    out->vthd       = thd_ratio(s_v, n);
    out->ithd       = thd_ratio(s_i, n);
}
