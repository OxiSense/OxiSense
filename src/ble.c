// ble.c
#include "gd32vf103.h"
#include "ble.h"

#define U0_TX_PIN   GPIO_PIN_9
#define U0_RX_PIN   GPIO_PIN_10
#define UART_BAUD   115200U

void uart_ble_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART0);

    gpio_init(GPIOA, GPIO_MODE_AF_PP,       GPIO_OSPEED_50MHZ, U0_TX_PIN);
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, U0_RX_PIN);

    usart_deinit(USART0);
    usart_baudrate_set(USART0, UART_BAUD);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_hardware_flow_rts_config(USART0, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(USART0, USART_CTS_DISABLE);

    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_enable(USART0);
}

void uart_putc(uint8_t c)
{
    while (usart_flag_get(USART0, USART_FLAG_TBE) == RESET) { }
    usart_data_transmit(USART0, c);
}

void uart_puts(const char *s)
{
    while (*s) {
        uart_putc((uint8_t)*s++);
    }
}

void uart_putline(const char *s)
{
    uart_puts(s);
    uart_puts("\r\n");
}
