#include "FreeRTOS.h" 
#include "task.h"
#include "semphr.h"
#include "gd32vf103.h"
#include "eclicw.h"
//#include "drivers.h"
#include "bsp.h"

typedef enum {W_CLOSED, W_UP, W_DOWN, W_OPEN} W_States;


/*
void bsp_enable_interrupt(void){
    eclic_global_interrupt_enable();
}

void bsp_disable_interrupt(void){
    eclic_global_interrupt_disable();
}
*/
void bsp_heartBeat(void * pParameters){
    uint32_t cycles = 0;
    uint32_t delay = (((uint32_t)pParameters)*1000)/portTICK_PERIOD_MS;
    rcu_periph_clock_enable(RCU_GPIOB);     // Config PB0 as heartbeat
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0);

    while (1) {                             // Heart beat super loop:
       ++cycles%2 ? gpio_bit_set(GPIOB, GPIO_PIN_0) : // Beat...
                    gpio_bit_reset(GPIOB, GPIO_PIN_0);
       vTaskDelay(delay);                             // ...wait
    }
}

typedef struct {
   SemaphoreHandle_t xGpsSemaphore;
   uint32_t latitude;
   uint32_t longitude;
} Point;
Point at;
void bsp_gps_init(void){
   rcu_periph_clock_enable(RCU_GPIOB);     // Config PB2 as beacon
   gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);
   at.xGpsSemaphore = xSemaphoreCreateMutex();
   at.latitude=0;
   at.longitude=0;
   xSemaphoreGive( at.xGpsSemaphore );
}
void bsp_gpsTracking(void *pParameters){
    while (1) {
        xSemaphoreTake( at.xGpsSemaphore, portMAX_DELAY ); // Acquier lock...
           at.latitude++;                                  // ...man. data...
        xSemaphoreGive( at.xGpsSemaphore );                // ...release lock
        vTaskDelay(2000);                                  // (Some delay)
    }
}
void bsp_emergencyBeacon(void *pParameters){
    uint32_t flg;
    while (1) {
        xSemaphoreTake( at.xGpsSemaphore, portMAX_DELAY ); // Acquier lock...
           flg=at.latitude;                                // ...man. data...
        xSemaphoreGive( at.xGpsSemaphore );                // ...release lock
        flg%2 ? gpio_bit_set(GPIOB, GPIO_PIN_2) :          // Beacon
                gpio_bit_reset(GPIOB, GPIO_PIN_2);
        vTaskDelay(500);                                   // (Some delay)
    }
}

/*
void bsp_timeSlice_isr(void){
    bsp_timeSlice_reload();
    
    schedule_signal(BSP_HEARTB);
    schedule_signal(BSP_GPSTRA);
    schedule_signal(BSP_BEACON);
}

*/
TaskHandle_t xAirbagTask;           // Globaly stored Airbag task handle.

void exti5_9_isr(void){
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (bsp_airbag_commit()) {
        vTaskNotifyGiveFromISR( xAirbagTask, &xHigherPriorityTaskWoken );
    }
    /*
    if (bsp_FRDupChanged() ||
        bsp_FRDdownChanged() ||
        bsp_FRDstopChanged()) schedule_signal(BSP_FRDFSM);
        */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

int  bsp_airbag_commit(void){
    int flg = exti_flag_get(EXTI_8);
    exti_flag_clear(EXTI_8);
    return flg;
}

void bsp_airbag_explode(void * pParameters){
    rcu_periph_clock_enable(RCU_GPIOB);     // Config PB1 as airbag exploed
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_1);

    rcu_periph_clock_enable(RCU_GPIOA);     // Config PA8 as airbag sensor
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_8);
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOA, GPIO_PIN_SOURCE_8);
    eclicw_enable(EXTI5_9_IRQn, 0, 0, exti5_9_isr);
    exti_init(EXTI_8, EXTI_INTERRUPT, EXTI_TRIG_BOTH);
    exti_interrupt_enable(EXTI_8);

    xAirbagTask = xTaskGetCurrentTaskHandle( );

    while (1){
       ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
       gpio_bit_set(GPIOB, GPIO_PIN_1);
    }
}

/*
int  bsp_FRDupChanged(void){
    int flg = exti_flag_get(EXTI_7);
    exti_flag_clear(EXTI_7);
    return flg;
}

int  bsp_FRDdownChanged(void){
    int flg = exti_flag_get(EXTI_6);
    exti_flag_clear(EXTI_6);
    return flg;
}

int  bsp_FRDstopChanged(void){
    int flg = exti_flag_get(EXTI_5);
    exti_flag_clear(EXTI_5);
    return flg;
}

void bsp_FRDwindowFSM(){
    static W_States state = W_CLOSED;
    switch (state) {    //7=up, 6=down, 5=stop
        case W_CLOSED : 
            if (gpio_input_bit_get(GPIOA, GPIO_PIN_7)) {
                state = W_UP; gpio_bit_set(GPIOA, GPIO_PIN_0);
            } break;
        case W_UP : 
            if (gpio_input_bit_get(GPIOA, GPIO_PIN_7))
                gpio_bit_set(GPIOA, GPIO_PIN_0);
            else
                gpio_bit_reset(GPIOA, GPIO_PIN_0);
            if (gpio_input_bit_get(GPIOA, GPIO_PIN_5)) {
                state = W_OPEN; gpio_bit_reset(GPIOA, GPIO_PIN_0);
            }
            if (gpio_input_bit_get(GPIOA, GPIO_PIN_6)) {
                state = W_DOWN; gpio_bit_set(GPIOA, GPIO_PIN_1);
            } break; 
        case W_DOWN :
            if (gpio_input_bit_get(GPIOA, GPIO_PIN_6))
                gpio_bit_set(GPIOA, GPIO_PIN_1);
            else
                gpio_bit_reset(GPIOA, GPIO_PIN_1);
            if (gpio_input_bit_get(GPIOA, GPIO_PIN_5)) {
                state = W_CLOSED; gpio_bit_reset(GPIOA, GPIO_PIN_1);
            }
            if (gpio_input_bit_get(GPIOA, GPIO_PIN_7)) {
                state = W_UP; gpio_bit_set(GPIOA, GPIO_PIN_0);
            } break;
        case W_OPEN : 
            if (gpio_input_bit_get(GPIOA, GPIO_PIN_6)) {
                state = W_DOWN; gpio_bit_set(GPIOA, GPIO_PIN_1);
            } break;   
    }
}
*/





/*
void bsp_FRDoor_init(void){
    rcu_periph_clock_enable(RCU_GPIOA);     // Config PA0 FRD window up 
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0);
    rcu_periph_clock_enable(RCU_GPIOA);     // Config PA1 FRD window down 
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_1);

    rcu_periph_clock_enable(RCU_GPIOA);     // Config PA7 as FRD up key
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_7);
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOA, GPIO_PIN_SOURCE_7);
    exti_init(EXTI_7, EXTI_INTERRUPT, EXTI_TRIG_BOTH);  // PA6 down key
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOA, GPIO_PIN_SOURCE_6);
    exti_init(EXTI_6, EXTI_INTERRUPT, EXTI_TRIG_BOTH);  // PA5 stop
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_5);
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOA, GPIO_PIN_SOURCE_5);
    exti_init(EXTI_5, EXTI_INTERRUPT, EXTI_TRIG_BOTH);
    eclicw_enable(EXTI5_9_IRQn, 7, 1, exti5_9_isr);
    exti_interrupt_enable(EXTI_8);
}

void bsp_sleep(void){
    schedule_signal(BSP_IDLE);      // Must always be ready...
    pmu_to_sleepmode(WFI_CMD);      //      ...the go to sleep!
}


void bsp_debug(void) {
    rcu_periph_clock_enable(RCU_GPIOB);     // Config PB2 assert failed!
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);
    gpio_bit_set(GPIOB, GPIO_PIN_2);
}

void bsp_performance(){
    int ready;

    //int i=1<<31;
    //while (!i&ready) i>>=1;
    
}

void bsp_watchdog_init(void){
   fwdgt_write_enable();
   // FWDGT 40KHz/64=625 Hz, set 5s delay!
   fwdgt_config(5*625, FWDGT_PSC_DIV64);
   fwdgt_enable();
}

void bsp_watchdog_feed(void){
   if (!gpio_input_bit_get(GPIOB, GPIO_PIN_1))  // Feed the dog if the
      fwdgt_counter_reload();                   // airbag have not exploded!
}
*/
