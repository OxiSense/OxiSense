// timeutil.c
#include "gd32vf103.h"
#include "timeutil.h"
#include <stdio.h>

#define RTC_PRESCALER   39670u   /* fintrimma vid behov */

void rtc_init(void)
{
    rcu_periph_clock_enable(RCU_PMU);
    rcu_periph_clock_enable(RCU_BKPI);
    pmu_backup_write_enable();

    bkp_deinit();

    rcu_osci_on(RCU_IRC40K);
    rcu_osci_stab_wait(RCU_IRC40K);

    rcu_rtc_clock_config(RCU_RTCSRC_IRC40K);
    rcu_periph_clock_enable(RCU_RTC);

    rtc_register_sync_wait();

    rtc_prescaler_set(RTC_PRESCALER);
    rtc_lwoff_wait();

    rtc_counter_set(0);
    rtc_lwoff_wait();
}

void rtc_get_time(simple_time_t *t)
{
    uint32_t seconds = rtc_counter_get();

    seconds %= 24u * 3600u;
    t->h = seconds / 3600u;
    seconds %= 3600u;
    t->m = seconds / 60u;
    t->s = seconds % 60u;
}

/* +4 sek kompensation (justera vid behov) */
void rtc_set_time(uint8_t h, uint8_t m, uint8_t s)
{
    uint32_t seconds = (uint32_t)h * 3600u +
                       (uint32_t)m * 60u +
                       (uint32_t)s;

    seconds += 4;

    rtc_lwoff_wait();
    rtc_counter_set(seconds);
    rtc_lwoff_wait();
}

bool parse_datetime(const char *p,
                    uint16_t *year,
                    uint8_t  *mon,
                    uint8_t  *day,
                    uint8_t  *h,
                    uint8_t  *m,
                    uint8_t  *s)
{
    int Y, Mo, D, hh, mm, ss;

    if (sscanf(p, "%d-%d-%d %d:%d:%d", &Y, &Mo, &D, &hh, &mm, &ss) != 6) {
        return false;
    }

    if (Y < 1980 || Y > 2099) return false;
    if (Mo < 1 || Mo > 12)    return false;
    if (D  < 1 || D  > 31)    return false;
    if (hh < 0 || hh > 23)    return false;
    if (mm < 0 || mm > 59)    return false;
    if (ss < 0 || ss > 59)    return false;

    *year = (uint16_t)Y;
    *mon  = (uint8_t)Mo;
    *day  = (uint8_t)D;
    *h    = (uint8_t)hh;
    *m    = (uint8_t)mm;
    *s    = (uint8_t)ss;

    return true;
}

bool parse_time_hms(const char *p, uint8_t *h, uint8_t *m, uint8_t *s)
{
    int hh, mm, ss;

    if (sscanf(p, "%d:%d:%d", &hh, &mm, &ss) != 3) return false;
    if (hh < 0 || hh > 23) return false;
    if (mm < 0 || mm > 59) return false;
    if (ss < 0 || ss > 59) return false;

    *h = (uint8_t)hh;
    *m = (uint8_t)mm;
    *s = (uint8_t)ss;
    return true;
}

/* TIMER2 (millis) */

void timer2_init_1khz(void)
{
    rcu_periph_clock_enable(RCU_TIMER2);

    timer_parameter_struct t;
    timer_struct_para_init(&t);
    t.prescaler         = (uint16_t)((SystemCoreClock / 1000U) - 1U); /* 1 kHz */
    t.alignedmode       = TIMER_COUNTER_EDGE;
    t.counterdirection  = TIMER_COUNTER_UP;
    t.period            = 0xFFFF;
    t.clockdivision     = TIMER_CKDIV_DIV1;
    t.repetitioncounter = 0;

    timer_deinit(TIMER2);
    timer_init(TIMER2, &t);
    timer_counter_value_config(TIMER2, 0);
    timer_enable(TIMER2);
}

uint16_t timer2_counter_ms(void)
{
    return (uint16_t)timer_counter_read(TIMER2);
}
