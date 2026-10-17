/*
 * redirect.c
 *
 *  Created on: Mar 6, 2026
 *      Author: jaison
 */

#include "redirect.h"
#include "main.h"

#include <string.h>
#include <stdarg.h>

/* nanoprintf configuration */
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0

#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

extern UART_HandleTypeDef huart3;

void uart_printf(const char *fmt, ...)
{
    char buffer[128];

    va_list args;
    va_start(args, fmt);

    int len = npf_vsnprintf(buffer, sizeof(buffer), fmt, args);

    va_end(args);

    if(len > 0)
    {
        HAL_UART_Transmit(&huart3, (uint8_t*)buffer, len, 50);
    }
}
