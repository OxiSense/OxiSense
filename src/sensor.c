// sensor.c
#include "gd32vf103.h"
#include "systick.h"
#include "sensor.h"
#include <stdint.h>

#define DEV_ADDR   0x20  /* Modbus-adress (DFRobot-exempel) */

/* USART1 på PA2 (TX) / PA3 (RX)  – 9600 8N1 */

static void usart1_init_9600(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART1);

    gpio_init(GPIOA, GPIO_MODE_AF_PP,       GPIO_OSPEED_50MHZ, GPIO_PIN_2); /* TX */
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_3); /* RX */

    usart_deinit(USART1);
    usart_baudrate_set(USART1, 9600U);
    usart_word_length_set(USART1, USART_WL_8BIT);
    usart_stop_bit_set(USART1, USART_STB_1BIT);
    usart_parity_config(USART1, USART_PM_NONE);
    usart_hardware_flow_rts_config(USART1, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(USART1, USART_CTS_DISABLE);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
    usart_enable(USART1);
}

static void usart1_putc(uint8_t c)
{
    while(RESET == usart_flag_get(USART1, USART_FLAG_TBE));
    usart_data_transmit(USART1, c);
}

static int usart1_getc_timeout(uint8_t *c, uint32_t timeout_ms)
{
    while (timeout_ms--) {
        if (SET == usart_flag_get(USART1, USART_FLAG_RBNE)) {
            *c = (uint8_t)usart_data_receive(USART1);
            return 1;
        }
        delay_1ms(1);
    }
    return 0;
}

/* CRC16 Modbus (polynom 0xA001) */
static uint16_t mb_crc16(const uint8_t *buf, int len)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; ++i) {
        crc ^= buf[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else         crc >>= 1;
        }
    }
    return crc;
}

static void mb_send(const uint8_t *p, int n)
{
    for (int i = 0; i < n; ++i) usart1_putc(p[i]);
}

static int uart1_read_exact(uint8_t *dst, int n, uint32_t timeout_ms)
{
    uint8_t c;
    for (int i = 0; i < n; ++i) {
        if (!usart1_getc_timeout(&c, timeout_ms)) return 0;
        dst[i] = c;
    }
    return 1;
}

/* Töm RX-bufferten på USART1 (tar bort ev. gamla bytes) */
static void usart1_flush_rx(void)
{
    while (usart_flag_get(USART1, USART_FLAG_RBNE) != RESET) {
        (void)usart_data_receive(USART1);
    }
}

/* Write Single Register */
static int sensor_write_reg(uint16_t reg, uint16_t val)
{
    uint8_t P[8] = {
        DEV_ADDR, 0x06,
        (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF),
        (uint8_t)(val >> 8), (uint8_t)(val & 0xFF),
        0x00, 0x00
    };
    uint16_t crc = mb_crc16(P, 6);
    P[6] = (uint8_t)(crc & 0xFF);
    P[7] = (uint8_t)(crc >> 8);

    /* Töm RX innan vi skickar ett nytt Modbus-kommando */
    usart1_flush_rx();
    mb_send(P, 8);

    uint8_t R[8];
    if (!uart1_read_exact(R, 8, 800)) return 0;
    uint16_t rcrc = (uint16_t)R[6] | ((uint16_t)R[7] << 8);
    if (mb_crc16(R, 6) != rcrc) return 0;
    if (R[0] != DEV_ADDR || R[1] != 0x06) return 0;
    return 1;
}

/* Read Holding Registers */
static int sensor_read_regs(uint16_t reg, uint16_t qty, uint8_t *data, int data_len)
{
    uint8_t P[8] = {
        DEV_ADDR, 0x03,
        (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF),
        (uint8_t)(qty >> 8), (uint8_t)(qty & 0xFF),
        0x00, 0x00
    };
    uint16_t crc = mb_crc16(P, 6);
    P[6] = (uint8_t)(crc & 0xFF);
    P[7] = (uint8_t)(crc >> 8);

    /* Töm RX innan vi skickar ett nytt Modbus-kommando */
    usart1_flush_rx();
    mb_send(P, 8);

    uint8_t hdr[3];
    if (!uart1_read_exact(hdr, 3, 800)) return 0;
    if (hdr[0] != DEV_ADDR || hdr[1] != 0x03) return 0;
    uint8_t nbytes = hdr[2];
    if (nbytes > data_len) return 0;

    if (!uart1_read_exact(data, nbytes, 800)) return 0;

    uint8_t crcbuf[3+8]; /* max 8 databytes här */
    for (int i = 0; i < 3; i++)      crcbuf[i]   = hdr[i];
    for (int i = 0; i < nbytes; i++) crcbuf[3+i] = data[i];

    uint8_t crcraw[2];
    if (!uart1_read_exact(crcraw, 2, 800)) return 0;
    uint16_t rcrc = (uint16_t)crcraw[0] | ((uint16_t)crcraw[1] << 8);
    if (mb_crc16(crcbuf, 3 + nbytes) != rcrc) return 0;
    return nbytes;
}

/* ==== Publika wrappers ==== */

int sensor_uart_init(void)
{
    usart1_init_9600();
    return 1;
}

/* Starta sensorns mätmaskin */
int sensor_start_collect(void)
{
    /* Skriv reg 0x0010 = 0x0001 (start) */
    return sensor_write_reg(0x0010, 0x0001);
}

/* Läs SPO2 och BPM; returnerar 1 vid OK, fyller *spo2 och *bpm */
int sensor_read_spo2_bpm(int *spo2, int *bpm)
{
    uint8_t D[16];
    int n = sensor_read_regs(0x0006, 0x0004, D, sizeof(D));
    if (n != 8) return 0;

    *spo2 = (int)D[0];

    uint32_t hb = ((uint32_t)D[2] << 24) |
                  ((uint32_t)D[3] << 16) |
                  ((uint32_t)D[4] <<  8) |
                   (uint32_t)D[5];
    *bpm = (int)hb;
    return 1;
}
