#include "pigpio_emu.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>

int gpioInitialise(void) {
    printf("[STUB] gpioInitialise()\n");
    return 0;
}

void gpioTerminate(void) {
    printf("[STUB] gpioTerminate()\n");
}

void gpioSetMode(unsigned gpio, unsigned mode) {
    printf("[STUB] gpioSetMode(gpio=%u, mode=%u)\n", gpio, mode);
}

void gpioSetPullUpDown(unsigned gpio, unsigned pud) {
    printf("[STUB] gpioSetPullUpDown(gpio=%u, pud=%u)\n", gpio, pud);
}

void gpioSetAlertFunc(unsigned gpio, alertFunc_t f) {
    printf("[STUB] gpioSetAlertFunc(gpio=%u, f=%#" PRIxPTR ")\n", gpio, (uintptr_t)f);
}

int gpioWrite(unsigned gpio, unsigned level) {
    printf("[STUB] gpioWrite(gpio=%u, level=%u)\n", gpio, level);
    return 0;
}

int i2cOpen(unsigned bus, unsigned addr, unsigned flags) {
    printf("[STUB] i2cOpen(bus=%u, addr=0x%x, flags=%u)\n", bus, addr, flags);
    return 42; // fake handle
}

int i2cClose(unsigned handle) {
    printf("[STUB] i2cClose(handle=%d)\n", handle);
    return 0;
}

int i2cWriteByte(unsigned handle, uint8_t b) {
    printf("[STUB] i2cWriteByte(handle=%d, byte=0x%02x)\n", handle, b);
    return 0;
}

void gpioDelay(unsigned us) {
    // usleep is in microseconds
    usleep(us);
}

