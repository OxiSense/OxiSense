// adc_driver.c
#include "gd32vf103.h"
#include "systick.h"
#include "adc_driver.h"

#define ANALOG_PIN      GPIO_PIN_3
#define ANALOG_CHANNEL  ADC_CHANNEL_3

void adc_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, ANALOG_PIN);

    rcu_periph_clock_enable(RCU_ADC0);
    adc_deinit(ADC0);

    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV8);
    adc_mode_config(ADC_MODE_FREE);

    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, ENABLE);
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);

    adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 1);
    adc_regular_channel_config(ADC0, 0, ANALOG_CHANNEL, ADC_SAMPLETIME_55POINT5);

    adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL, ADC0_1_EXTTRIG_REGULAR_NONE);
    adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE);

    adc_enable(ADC0);
    delay_1ms(1);
    adc_calibration_enable(ADC0);
    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
}
