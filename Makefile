CC       ?= gcc
CFLAGS   ?= -std=c17 -g -O2 -Wall -Werror -pedantic -D_POSIX_SOURCE -D_DEFAULT_SOURCE
LIBS     := -lrt -pthread
SRC      := lcd_i2c.c maker.c
PI_DETECTED := $(shell grep -q 'Raspberry Pi' /proc/cpuinfo && echo 1 || echo 0)

# If no PI detected we simulate pigpio on our laptops
ifeq ($(PI_DETECTED), 1)
    CFLAGS += -DON_PI
    LIBS += -lpigpio
else
    SRC += pigpio_emu.c
endif

OBJS := $(SRC:.c=.o)

.PHONY: all clean

all: cocktailmaker

cocktailmaker: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) *.o cocktailmaker

