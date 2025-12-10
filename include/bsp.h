#include <stdint.h>
#define BSP_AIRBAG 3
#define BSP_BEACON 3
#define BSP_FRDFSM 2
#define BSP_HEARTB 1
#define BSP_GPSTRA 1
#define BSP_IDLE   0

void bsp_gps_init(void);
void bsp_heartBeat(void *pParameters);
void bsp_airbag_explode(void *pParameters);
void bsp_gpsTracking(void *pParameters);
void bsp_emergencyBeacon(void *pParameters);
/*
void bsp_gpsTracking(void);
void bsp_emergencyBeacon(void);
void bsp_debug(void);
void bsp_performance(void);
void bsp_airbag_init(void);
int  bsp_airbag_commit(void);
void bsp_airbag_explode(void);
void bsp_FRDoor_init(void);
int  bsp_FRDupChanged(void);
int  bsp_FRDdownChanged(void);
int  bsp_FRDstopChanged(void);
void bsp_FRDwindowFSM(void);
void bsp_enable_interrupt(void);
void bsp_disable_interrupt(void);
void bsp_sleep(void);
void bsp_watchdog_init(void);
void bsp_watchdog_feed(void);
*/
