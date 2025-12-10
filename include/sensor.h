// sensor.h
#pragma once

int  sensor_uart_init(void);
int  sensor_start_collect(void);
int  sensor_read_spo2_bpm(int *spo2, int *bpm);
