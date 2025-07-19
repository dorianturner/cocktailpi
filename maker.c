#ifndef ON_PI
#include "pigpio_emu.h"
#else
#include <pigpio.h>
#endif

#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lcd_i2c.h"
#include "maker.h"

// current cocktail selection
static volatile FSMState cur_selection = Start; 
// to handle transition to FinishDispensing state
static volatile bool drink_pouring_done = false;
static atomic_bool pouring_error = false;
static volatile bool running = true;

static uint32_t last_up_time = 0;
static uint32_t last_down_time = 0;
static uint32_t last_select_time = 0;

FILE *err_out;

const int pump_gpio_pins[NUM_INGREDIENTS] = {5, 6, 13, 19, 26}; // BCM GPIO numbers

static const char *const FSMState_strings[] = {
#define X(elem, str, v, r, t, l, c) str,
    FSM_STATES
#undef X
};

static const int drinks[][NUM_INGREDIENTS] = {
#define X(elem, str, v, r, t, l, c) {v, r, t, l, c},
    FSM_STATES
#undef X
};

static void handle_sigint(int sig) {
    running = false;
}

void print_with_timestamp(FILE *fp, const char *message) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S", t);

    fprintf(fp, "[%s] %s\n", buf, message);
}

// all buttons deactivated if in a machine-controlled state
static void up_cb(int gpio, int level, uint32_t tick) {
    if (level != 1) return;
    if (tick - last_up_time < DEBOUNCE_TIME) return;
    last_up_time = tick;
    if (cur_selection > 0 && cur_selection < ThatsIt) cur_selection--;
}

static void down_cb(int gpio, int level, uint32_t tick) {
    if (level != 1) return;
    if (tick - last_down_time < DEBOUNCE_TIME) return;
    last_down_time = tick;
    if (cur_selection < ThatsIt) cur_selection++;
    // ThatsIt used as "last" drink
}

static void select_cb(int gpio, int level, uint32_t tick) {
    if (level != 1) return;
    if (tick - last_select_time < DEBOUNCE_TIME) return;
    last_select_time = tick;
    if (cur_selection >= 0 && cur_selection < ThatsIt) {
        assert(cur_selection >= 0 && cur_selection < TOTAL_STATE_COUNT);
        // quick assertion that cur_state is valid
        switch (cur_selection) {
            case Dispensing:
            case FinishDispensing:
            case Start:
            case ThatsIt:
            case Error:
                break; // do nothing
            default:
                assert(cur_selection < ThatsIt); // cur_state should be a drink
                cur_selection = Dispensing; // set to dispensing
        }
    }
}

static void stop_all_pumps() {
    for (int i = 0; i < NUM_INGREDIENTS; ++i) {
        #if  defined(EMU_OUTPUT) || defined(DEBUG)
        char msg_pump[64];
        snprintf(msg_pump, sizeof(msg_pump), "Turning off pump %d", i);
        print_with_timestamp(stdout, msg_pump);
        #endif
        #ifndef EMU_OUTPUT
        gpioWrite(pump_gpio_pins[i], 0);
        #endif
    }
}
             
static void *pump_timer_thread(void *arg) {
    int ingredient =  ((int *)arg)[0];
    int duration_ms = ((int *)arg)[1];
    free(arg);

    // early termination if another thread has an error
    if (pouring_error) return NULL;

    #if defined(EMU_OUTPUT) || defined(DEBUG)
    char msg_on[64];
    snprintf(msg_on, sizeof(msg_on), "Turning on pump %d", ingredient);
    print_with_timestamp(stdout, msg_on);
    #endif
    #ifndef EMU_OUTPUT
    gpioWrite(pump_gpio_pins[ingredient], 1); // turn on pump
    #endif
    usleep(duration_ms  * 1000);
    #if defined(EMU_OUTPUT) || defined(DEBUG)
    char msg_off[64];
    snprintf(msg_off, sizeof(msg_off), "Turning off pump %d", ingredient);
    print_with_timestamp(stdout, msg_off);
    #endif
    #ifndef EMU_OUTPUT
    gpioWrite(pump_gpio_pins[ingredient], 0); // turn off pump
    #endif

    return NULL;
}

static void *pour_drink_thread(void *arg) {
    FSMState drink = *(FSMState *)arg;
    free(arg);

    pthread_t pump_threads[NUM_INGREDIENTS];
    bool pump_thread_created[NUM_INGREDIENTS] = {false};
    int max_duration_ms = 0;

    for (int i = 0; i < NUM_INGREDIENTS; ++i) {
        int part_count = drinks[drink][i];
        if (part_count > 0) { // only create threads for actually poured ingredients
            int duration_ms = (int)(part_count * VOLUME_PER_PART * TIME_PER_ML * 1000.0);
            if (duration_ms > max_duration_ms) max_duration_ms = duration_ms;

            int *args = malloc(2 * sizeof(int));
            if (args == NULL) {
                char err_msg[64];
                snprintf(err_msg, sizeof(err_msg), "Failed to malloc for pump %d", i);
                print_with_timestamp(err_out, err_msg);
                pouring_error = true;
                stop_all_pumps();
                break; // dont create more threads
            }

            args[0] = i;           // ingredient number
            args[1] = duration_ms; // time to pour

            if (pthread_create(&pump_threads[i], NULL, pump_timer_thread, args) != 0) {
                char err_msg[64];
                snprintf(err_msg, sizeof(err_msg), "Failed to create thread for pump %d", i);
                print_with_timestamp(err_out, err_msg);
                pouring_error = true;
                stop_all_pumps();
                free(args);
                break;
            }

            pump_thread_created[i] = true;
        }
    }

    // wait for all threads to terminate
    for (int i = 0; i < NUM_INGREDIENTS; ++i) {
        if (pump_thread_created[i]) {
            pthread_join(pump_threads[i], NULL);
        }
    }

    stop_all_pumps(); // for safety

    drink_pouring_done = true;
    return NULL;
}

static void pour_drink(FSMState drink_selection) {
    assert(drink_selection >= 0 && drink_selection < ThatsIt); // make sure its actually a drink

    FSMState *arg = malloc(sizeof(FSMState));
    if (arg == NULL) {
        
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Failed to malloc for pouring drink %s", FSMState_strings[drink_selection]);
        print_with_timestamp(err_out, err_msg);
        pouring_error = true;
    }
    *arg = drink_selection;

    pthread_t tid;
    if (pthread_create(&tid, NULL, pour_drink_thread, arg) != 0) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Failed to initialise thread for pouring %s", FSMState_strings[drink_selection]);
        print_with_timestamp(err_out, err_msg);
        pouring_error = true;
    } else {
        pthread_detach(tid);
    }
}

int main(int argc, char *argv[]) {
    err_out = stderr;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-E") == 0) {
            err_out = fopen("error.log", "a");
            if (err_out == NULL) {
                print_with_timestamp(stderr, "Failed to open error.log");
                return EXIT_FAILURE;
            }
        }
    }

    #ifdef ON_PI
    int ret = system("pgrep pigpiod > /dev/null");
    if (ret == 0) {
        print_with_timestamp(err_out, "pigpiod running\nRun 'sudo killall pigpiod' before running this script again");
        return EXIT_FAILURE;
    }
    #endif

    for (int i = 0; i < ThatsIt; ++i) {
        int sum_parts = 0;
        for (int j = 0; j < NUM_INGREDIENTS; ++j) {
            sum_parts += drinks[i][j];
        }
        if (sum_parts * VOLUME_PER_PART > GLASS_VOLUME) {
            char err_msg[512];
            snprintf(err_msg, sizeof(err_msg), "Drink %s has %d parts total, and with %d ml per part, whole drink will be %d ml.\n This is greater than the stated glass volume of %d ml.\n Aborting...\n", FSMState_strings[i], sum_parts, VOLUME_PER_PART, sum_parts * VOLUME_PER_PART, GLASS_VOLUME);
            print_with_timestamp(err_out, err_msg);
            return EXIT_FAILURE;
        }
    }

    signal(SIGINT, handle_sigint); // to handle CTRL+C

    gpioCfgSetInternals(PI_CFG_NOSIGHANDLER);
    gpioInitialise();
    
    // initialise input pins
    gpioSetMode(UP_BUTTON, PI_INPUT);
    gpioSetPullUpDown(UP_BUTTON, PI_PUD_DOWN);
    gpioSetAlertFunc(UP_BUTTON, up_cb);

    gpioSetMode(DOWN_BUTTON, PI_INPUT);
    gpioSetPullUpDown(DOWN_BUTTON, PI_PUD_DOWN);
    gpioSetAlertFunc(DOWN_BUTTON, down_cb);

    gpioSetMode(SELECT_BUTTON, PI_INPUT);
    gpioSetPullUpDown(SELECT_BUTTON, PI_PUD_DOWN);
    gpioSetAlertFunc(SELECT_BUTTON, select_cb);

    // initialise output pins
    for (int i = 0; i < NUM_INGREDIENTS; ++i) {
        gpioSetMode(pump_gpio_pins[i], PI_OUTPUT);
        #if defined(EMU_OUTPUT) || defined(DEBUG)
        char msg_off[64];
        snprintf(msg_off, sizeof(msg_off), "Turning off pump %d", i);
        print_with_timestamp(stdout, msg_off);
        #endif
        #ifndef EMU_OUTPUT
        gpioWrite(pump_gpio_pins[i], 0); // ensure all off
        #endif
    }

    int lcd_handle = lcd_init(1, LCD_ADDR); 
    if (lcd_handle == -1) {
        print_with_timestamp(err_out, "Error initialising lcd. Ensure i2c is enabled and lcd correctly connected");
        return EXIT_FAILURE;
    }
    time_t state_start_time = time(NULL); // set to current time for Start state
    FSMState last_state = TOTAL_STATE_COUNT;

    while (running) {
        if (last_state != cur_selection) {
            lcd_clear(lcd_handle);
            lcd_write_string(lcd_handle, FSMState_strings[cur_selection]);
            if (cur_selection == Dispensing) {
                pour_drink(last_state);
            }
            state_start_time = time(NULL);
            last_state = cur_selection;
        }
        if ((cur_selection == Start || cur_selection == ThatsIt || cur_selection == FinishDispensing) && time(NULL) - state_start_time >= STATE_TRANSITION_TIME) {
            switch(cur_selection) {
                case FinishDispensing:
                case Start:
                case Error:
                    cur_selection = 0; // to first drink
                    break;
                case ThatsIt:
                    cur_selection = ThatsIt - 1; // to last drink
                    break;
                default:
                    assert(false); // no other states should reach this
            }
        }
        if (drink_pouring_done) {
            drink_pouring_done = false;

            if (pouring_error) { // handling error state
                cur_selection = Error;
            } else {
                cur_selection = FinishDispensing;
            }
            continue; // skip wait, to automatically update state
        }
        usleep(0.1 * 1000 * 1000); // 0.1s
    }
    lcd_clear(lcd_handle);
    lcd_write_string(lcd_handle, "Machine terminating...");
    sleep(3); // 1s
    lcd_clear(lcd_handle);
    lcd_close(lcd_handle);
    stop_all_pumps(); // rather be safe than sorry...
    gpioTerminate();

    if (err_out != stderr) {
        fclose(err_out);
    }

    return EXIT_SUCCESS;
}


