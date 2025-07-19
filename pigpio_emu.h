#ifndef PIGPIO_EMU_H
#define PIGPIO_EMU_H

#include <stdint.h>

#define PI_OUTPUT 1
#define PI_INPUT 0
#define PI_PUD_DOWN 2

typedef void (*alertFunc_t)(int gpio, int level, uint32_t tick);

int gpioInitialise(void);
void gpioTerminate(void);
void gpioSetMode(unsigned gpio, unsigned mode);
void gpioSetPullUpDown(unsigned gpio, unsigned pud);
void gpioSetAlertFunc(unsigned gpio, alertFunc_t f);
int gpioWrite(unsigned gpio, unsigned level);

int i2cOpen(unsigned bus, unsigned addr, unsigned flags);
int i2cClose(unsigned handle);
int i2cWriteByte(unsigned handle, uint8_t b);

void gpioDelay(unsigned us);
void time_sleep(double s);

#endif

