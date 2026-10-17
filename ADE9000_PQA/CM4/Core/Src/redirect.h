/*
 * redirect.h
 *
 *  Created on: Mar 6, 2026
 *      Author: jaison
 */

#ifndef REDIRECT_H
#define REDIRECT_H

void uart_printf(const char *fmt, ...);

/* Enable / Disable logging */
#ifndef ENABLE_LOG
#define ENABLE_LOG 1
#endif

#if ENABLE_LOG
#define LOG(...) uart_printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

#endif
