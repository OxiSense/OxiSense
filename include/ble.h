// ble.h
#pragma once

#include <stdint.h>

void uart_ble_init(void);
void uart_putc(uint8_t c);
void uart_puts(const char *s);
void uart_putline(const char *s);
