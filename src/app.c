// app.c – BLE logger + SD + SPO2/BPM-sensor

#include "gd32vf103.h"
#include "lcd.h"
#include "systick.h"
#include "gd32v_tf_card_if.h"
#include "ff.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "app.h"
#include "ble.h"
#include "sensor.h"
#include "timeutil.h"
#include "adc_driver.h"

/* ---------- Tidsstruktur (från timeutil.h) ---------- */
/* typedef struct { uint8_t h,m,s; } simple_time_t; */

/* ---------- Globala variabler ---------- */
u16 BACK_COLOR = BLACK;

/* CMD-hantering (MCU:s eget kommandoläge) */
#define CMD_BUF_SIZE 64
static char  cmd_buf[CMD_BUF_SIZE];
static int   cmd_len      = 0;
static int   dollar_count = 0;
static bool  cmd_mode     = false;

/* BLE-anslutningsstatus */
static bool     ble_connected          = false;
static uint32_t last_activity_seconds  = 0;

/* Under filöverföring pausar vi LCD-uppdatering */
static volatile bool sd_xfer_active = false;

/* ---------- ADC + SD ---------- */
#define VREF_MV         3300U
#define AVG_N           16U

/* LED för pulsvisning (aktiv låg: 0 = ON, 1 = OFF) */
#define LED_PORT  GPIOB
#define LED_PIN   GPIO_PIN_0

static FATFS fs;
static FIL  fil;       /* skriv-handle (loggning) */
static UINT bw;

static bool logging_enabled = false;
static bool file_open       = false;

/* loggfilnamn: "YYYYMMDD.TXT" */
static char log_filename[16] = "SENSOR.TXT";

/* ======= Senaste satta datum/tid (för FatFs) ======= */
static uint16_t g_year  = 1980;
static uint8_t  g_month = 1;
static uint8_t  g_day   = 1;
static uint8_t  g_hour  = 0;
static uint8_t  g_min   = 0;
static uint8_t  g_sec   = 0;

/* ======= Ringbuffer & filtrering (ADC, ej kritisk nu) ======= */
static uint16_t ring[AVG_N] = {0};
static uint32_t sum   = 0;
static uint8_t  pos   = 0;
static uint8_t  filled= 0;

/* ======= 64-bitars tid i ms (egen soft-timer) ======= */
static uint64_t ms_total = 0;
static uint16_t last_cnt = 0;

/* Antal loggade rader (för f_sync) */
static uint32_t sample_idx = 0;

/* Logg-intervall i sekunder (via RTC) */
static uint32_t last_seconds = 0;
static uint32_t last_log_sec = 0;   /* senaste loggsekund */

/* ======= Sensorstatus ======= */
static bool sensor_uart_inited = false;
static bool sensor_started     = false;

static void vAppTask(void *pvParameters);

/* ==================  LCD HJÄLPARE  ======================= */

static void lcd_show_time_now(void)
{
    simple_time_t t;
    rtc_get_time(&t);

    char buf[20];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", t.h, t.m, t.s);
    LCD_ShowString(0, 16, (const u8*)buf, GREEN);
}

/* ==================   COMMAND-HANTERING   ================ */

static void handle_command(const char *cmd)
{
    LCD_ShowString(0, 48, (const u8*)"CMD", CYAN);

    while (*cmd == ' ' || *cmd == '\t') cmd++;

    /* ======== SET_TIME ======== */
    if (strncmp(cmd, "SET_TIME", 8) == 0) {

        if (!cmd_mode) { uart_putline("ERR NOT IN CMD MODE"); return; }

        const char *p = cmd + 8;
        while (*p == ' ' || *p == '\t') p++;

        uint16_t year;
        uint8_t  mon, day, hh, mm, ss;

        if (parse_datetime(p, &year, &mon, &day, &hh, &mm, &ss)) {
            g_year  = year; g_month = mon; g_day = day;
            g_hour  = hh;   g_min   = mm;  g_sec = ss;

            rtc_set_time(hh, mm, ss);
            lcd_show_time_now();
            set_fattime(g_year, g_month, g_day, g_hour, g_min, g_sec);

            char dbg[64];
            snprintf(dbg, sizeof(dbg),
                     "OK DATETIME FATTIME %04u-%02u-%02u %02u:%02u:%02u",
                     g_year, g_month, g_day, g_hour, g_min, g_sec);
            uart_putline(dbg);

            cmd_mode = false;
            return;
        }

        if (parse_time_hms(p, &hh, &mm, &ss)) {
            g_hour = hh; g_min = mm; g_sec = ss;

            rtc_set_time(hh, mm, ss);
            lcd_show_time_now();
            set_fattime(g_year, g_month, g_day, g_hour, g_min, g_sec);

            uart_putline("OK TIME ONLY FATTIME");
            cmd_mode = false;
        } else {
            uart_putline("ERR WRONG TIME/DATETIME");
        }
        return;
    }

    /* ======== START_LOG yyyy-MM-dd HH:mm:ss ======== */
    if (strncmp(cmd, "START_LOG", 9) == 0) {

        if (!cmd_mode) { uart_putline("ERR NOT IN CMD MODE"); return; }

        const char *p = cmd + 9;
        while (*p == ' ' || *p == '\t') p++;

        uint16_t year;
        uint8_t  mon, day, hh, mm, ss;

        if (!parse_datetime(p, &year, &mon, &day, &hh, &mm, &ss)) {
            uart_putline("ERR START_LOG DATETIME");
            return;
        }

        g_year=year; g_month=mon; g_day=day; g_hour=hh; g_min=mm; g_sec=ss;

        rtc_set_time(hh, mm, ss);
        lcd_show_time_now();
        set_fattime(g_year, g_month, g_day, g_hour, g_min, g_sec);

        snprintf(log_filename, sizeof(log_filename),
                 "%04u%02u%02u.TXT",
                 (unsigned)g_year, (unsigned)g_month, (unsigned)g_day);

        if (file_open) {
            f_sync(&fil);
            f_close(&fil);
            file_open       = false;
            logging_enabled = false;
        }

        FRESULT fr = f_open(&fil, log_filename, FA_WRITE | FA_CREATE_ALWAYS);
        if (fr != FR_OK) {
            char msg[48];
            snprintf(msg, sizeof(msg), "ERR START_LOG OPEN %d", fr);
            uart_putline(msg);
            return;
        }

        /* Header: format B */
        const char *hdr = "time,value\r\n";
        bw = 0;
        fr = f_write(&fil, hdr, strlen(hdr), &bw);
        if (fr != FR_OK || bw != strlen(hdr)) {
            char msg[64];
            snprintf(msg, sizeof(msg),
                     "ERR WRITE HEADER fr=%d bw=%u",
                     (int)fr, (unsigned)bw);
            uart_putline(msg);
            f_sync(&fil);
            f_close(&fil);
            file_open       = false;
            logging_enabled = false;
            cmd_mode        = false;
            return;
        }
        f_sync(&fil);

        /* Initiera sensor-UART vid behov */
        if (!sensor_uart_inited) {
            sensor_uart_init();
            sensor_uart_inited = true;
            delay_1ms(200);
        }

        /* Försök starta sensorn tyst med retries */
        sensor_started = false;
        for (int tries = 0; tries < 5 && !sensor_started; ++tries) {
            if (sensor_start_collect()) {
                sensor_started = true;
            } else {
                sensor_uart_init();
                delay_1ms(1000);
            }
        }

        if (sensor_started) {
            uart_putline("OK SENSOR START");
            delay_1ms(1500);  /* ge sensorn tid att börja mäta */
        } else {
            uart_putline("ERR SENSOR START (NO LOGGING)");

            /* Stäng filen – vi vill inte logga bara ogiltiga värden */
            f_sync(&fil);
            f_close(&fil);
            file_open       = false;
            logging_enabled = false;
            cmd_mode        = false;
            return;
        }

        /* reset filter & timers */
        ms_total   = 0;
        last_cnt   = timer2_counter_ms();
        sum        = 0;
        pos        = 0;
        filled     = 0;
        sample_idx = 0;
        for (int i = 0; i < AVG_N; ++i) ring[i] = 0;

        /* logga första gången efter 3 s från nu */
        last_log_sec = rtc_counter_get();

        file_open       = true;
        logging_enabled = true;

        char dbg[80];
        snprintf(dbg, sizeof(dbg),
                "OK START LOGGING %s", log_filename);
        uart_putline(dbg);

        cmd_mode = false;
        return;
    }

    /* ======== STOP_LOG ======== */
    if (strncmp(cmd, "STOP_LOG", 8) == 0) {

        if (!cmd_mode) { uart_putline("ERR NOT IN CMD MODE"); return; }

        logging_enabled = false;

        if (file_open) {
            f_sync(&fil);
            f_close(&fil);
            file_open = false;
        }

        gpio_bit_write(LED_PORT, LED_PIN, 1);
        uart_putline("OK STOP_LOG");

        cmd_mode = false;
        return;
    }

    /* ======== READ_LAST (stabil variant) ======== */
    if (strncmp(cmd, "READ_LAST", 9) == 0) {

        if (!cmd_mode) { uart_putline("ERR NOT IN CMD MODE"); return; }

        FRESULT fr;
        UINT br;
        static uint8_t buf[128] __attribute__((aligned(4)));
        char msg[64];
        FIL rfil;   /* separat handle för läsning */

        sd_xfer_active = true;   /* blockera LCD-uppdateringar */

        /* Öppna senaste loggfilen för läsning */
        fr = f_open(&rfil, log_filename, FA_READ);
        if (fr != FR_OK) {
            snprintf(msg, sizeof(msg), "ERR READ_OPEN %d", fr);
            uart_putline(msg);
            sd_xfer_active = false;
            cmd_mode = false;
            return;
        }

        /* Tala om vilken fil som kommer */
        snprintf(msg, sizeof(msg), "FILE_BEGIN %s", log_filename);
        uart_putline(msg);

        /* Läs & skicka rådata i lagom block */
        for (;;) {
            br = 0;
            fr = f_read(&rfil, buf, sizeof(buf), &br);
            if (fr != FR_OK) {
                snprintf(msg, sizeof(msg), "ERR READ %d", fr);
                uart_putline(msg);
                break;
            }
            if (br == 0) {
                /* EOF */
                break;
            }

            for (UINT i = 0; i < br; i++) {
                uart_putc(buf[i]);
            }

            /* Liten paus mellan block för stabilitet (SD/FatFs/UART) */
            delay_1ms(1);
        }

        /* Avslutande markör (med CRLF före/efter) */
        uart_putline("\r\nFILE_END\r\n");

        f_close(&rfil);
        sd_xfer_active = false;

        cmd_mode = false;
        return;
    }

    /* ======== TIME (debug) ======== */
    if (strncmp(cmd, "TIME", 4) == 0) {
        simple_time_t t;
        rtc_get_time(&t);
        char buf[20];
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u", t.h, t.m, t.s);
        uart_putline(buf);
        return;
    }
}

/* Tecken in i kommandobuffern */
static void feed_command_char(char c)
{
    if (c == '\r' || c == '\n') {
        if (cmd_len > 0) {
            cmd_buf[cmd_len] = '\0';
            handle_command(cmd_buf);
            cmd_len = 0;
        }
    } else {
        if (cmd_len < (CMD_BUF_SIZE - 1)) {
            cmd_buf[cmd_len++] = c;
        } else {
            cmd_len = 0;   // overflow → rensa
        }
    }
}

/* ========================  APP MAIN  ===================== */

void app_main(void)
{
    rtc_init();
    uart_ble_init();
    timer2_init_1khz();
    adc_init();

    /* SD-init */
    set_fattime(g_year, g_month, g_day, g_hour, g_min, g_sec);
    delay_1ms(100);

    FRESULT fr_mount = f_mount(&fs, "", 1);
    if (fr_mount != FR_OK) {
        char msg[32];
        snprintf(msg, sizeof(msg), "ERR MOUNT %d", fr_mount);
        uart_putline(msg);
    } else {
        uart_putline("OK MOUNT");
    }

    /* LED init – AV från start (aktiv låg → 1 = OFF) */
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(LED_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LED_PIN);
    gpio_bit_write(LED_PORT, LED_PIN, 1);   // släckt

    /* LCD init */
    Lcd_SetType(LCD_INVERTED);
    Lcd_Init();
    BACK_COLOR = BLACK;
    LCD_Clear(BLACK);

    LCD_ShowString(0, 0, (const u8*)"BLE: WAITING", GREEN);
    lcd_show_time_now();

    /* init filter */
    for (int i = 0; i < AVG_N; ++i) ring[i] = 0;
    sum    = 0;
    pos    = 0;
    filled = 0;

    last_cnt      = timer2_counter_ms();
    ms_total      = 0;
    sample_idx    = 0;

    last_seconds = rtc_counter_get();
    last_activity_seconds = last_seconds;
    last_log_sec          = last_seconds;

    BaseType_t res = xTaskCreate(
        vAppTask,               /* Task-funktion */
        "APP",                  /* Namn (debug)  */
        2048,                   /* Stackord      */
        NULL,                   /* pvParameters  */
        tskIDLE_PRIORITY + 2,   /* Prioritet     */
        NULL                    /* Handle        */
    );

    if (res != pdPASS) {
        uart_putline("ERR xTaskCreate(APP)");
        while (1) { }
    }

    vTaskStartScheduler();

    while (1) {
    }
}

/* ========================  TASK  ========================= */

static void vAppTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        uint32_t now = rtc_counter_get();

        /* uppdatera ms_total */
        uint16_t now_cnt = timer2_counter_ms();
        uint16_t delta   = (uint16_t)(now_cnt - last_cnt);
        last_cnt = now_cnt;
        ms_total += delta;

        /* UART RX (BLE) */
        if (usart_flag_get(USART0, USART_FLAG_RBNE) != RESET) {
            uint8_t ch = (uint8_t)usart_data_receive(USART0);

            last_activity_seconds = now;

            /* Första tecknet → visa BLE: CONNECTED på LCD */
            if (!ble_connected) {
                ble_connected = true;
                LCD_Clear(BLACK);
                LCD_ShowString(0, 0, (const u8*)"BLE: CONNECTED", GREEN);
                lcd_show_time_now();
            }

            /* '$' → gå in i cmd-läge, pausa loggning men stäng inte filen här */
            if (ch == '$') {
                dollar_count++;
                if (dollar_count == 1) {
                    cmd_mode  = true;
                    cmd_len   = 0;
                    uart_putline("CMD? CMD MODE ACTIVE");
                    dollar_count = 0;

                    if (logging_enabled) {
                        logging_enabled = false;
                        if (file_open) {
                            f_sync(&fil);         /* håll filen öppen tills STOP_LOG */
                        }
                        gpio_bit_write(LED_PORT, LED_PIN, 1);  // LED av
                    }
                }
                continue;
            } else {
                dollar_count = 0;
            }

            if (cmd_mode) {
                feed_command_char((char)ch);
                continue;
            }
        }

        /* RTC 1 Hz tick → uppdatera LCD (ej under filöverföring) */
        if (now != last_seconds) {
            last_seconds = now;
            if (!sd_xfer_active) {
                lcd_show_time_now();
            }
        }

        /* Loggning – endast var 3:e sekund (via RTC-sekunder) */
        if (logging_enabled && file_open) {
            uint32_t now_sec = now;
            if ((now_sec - last_log_sec) >= 3U) {
                last_log_sec = now_sec;

                /* ADC-läsning + filter (behövs inte för sensorn, men låter vara kvar) */
                uint16_t raw = (uint16_t)ADC_RDATA(ADC0);
                sum -= ring[pos];
                ring[pos] = raw;
                sum += raw;
                pos++;
                if (pos >= AVG_N) { pos = 0; filled = 1; }
                uint32_t n   = filled ? AVG_N : (pos == 0 ? 1 : pos);
                uint16_t avg = (uint16_t)((sum + (n / 2)) / n);
                uint32_t mv  = (avg * (uint32_t)VREF_MV + 2047U) / 4095U;
                (void)mv;

                /* ta tid: RTC + ms_total */
                simple_time_t t;
                rtc_get_time(&t);
                uint16_t ms_now = (uint16_t)(ms_total % 1000ULL);

                int spo2 = -1, bpm = -1;
                if (sensor_started && sensor_read_spo2_bpm(&spo2, &bpm)) {
                    /* OK värden */
                } else {
                    spo2 = -1;
                    bpm  = -1;
                }

                /* Filtrera bort både 0 och <=0 så vi bara loggar plausibla värden */
                if (spo2 <= 0 || bpm <= 0) {
                    goto delay_and_continue;
                }

                /* logga: FORMAT B */
                char line[96];
                int len = snprintf(line, sizeof(line),
                                   "%02u:%02u:%02u.%03u,SPO2=%d,BPM=%d\r\n",
                                   t.h, t.m, t.s,
                                   (unsigned)ms_now,
                                   spo2, bpm);

                if (len > 0) {
                    bw = 0;
                    FRESULT frw = f_write(&fil, line, (UINT)len, &bw);
                    if (frw != FR_OK || bw != (UINT)len) {
                        char msg[64];
                        snprintf(msg, sizeof(msg),
                                 "ERR WRITE %d bw=%u",
                                 (int)frw, (unsigned)bw);
                        uart_putline(msg);
                    }
                    sample_idx++;
                    /* synca var 5:e rad */
                    if ((sample_idx % 5U) == 0U) {
                        f_sync(&fil);
                    }
                }

                /* LED-styrning (aktiv låg): kort blink vid loggning */
                gpio_bit_write(LED_PORT, LED_PIN, 0);       /* ON */
                delay_1ms(20);
                gpio_bit_write(LED_PORT, LED_PIN, 1);       /* OFF */
            }
        } else {
            gpio_bit_write(LED_PORT, LED_PIN, 1);
        }

    delay_and_continue:
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
