// timeutil.h
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct { uint8_t h, m, s; } simple_time_t;

void rtc_init(void);
void rtc_get_time(simple_time_t *t);
void rtc_set_time(uint8_t h, uint8_t m, uint8_t s);

bool parse_datetime(const char *p,
                    uint16_t *year,
                    uint8_t  *mon,
                    uint8_t  *day,
                    uint8_t  *h,
                    uint8_t  *m,
                    uint8_t  *s);

bool parse_time_hms(const char *p, uint8_t *h, uint8_t *m, uint8_t *s);

void timer2_init_1khz(void);
uint16_t timer2_counter_ms(void);
