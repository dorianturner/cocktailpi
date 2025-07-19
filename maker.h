#ifndef MAKER_H
#define MAKER_H

#include <stdio.h>

#define LCD_ADDR 0x27 // use i2cdetect to find right address
// define gpio connections for up, down and selection buttons
#define UP_BUTTON 17
#define DOWN_BUTTON 27
#define SELECT_BUTTON 22
#define STATE_TRANSITION_TIME 3
// time in seconds
#define DEBOUNCE_TIME 200000 // 200ms
// time between presses, to stop double-input

// volumes all in ml
#define VOLUME_PER_PART 1 // for dashes
#define GLASS_VOLUME 300

#define TIME_PER_ML 0.25
// HOPE THIS IS THE SAME FOR ALL OF THEM
// NEED TO CALCULATE EXPERIMENTALLY

//    Enum Name
//                      Name as a string
//                                                                v Pineapple
//                                                                   v DEAD
//                                                                      v Orange
//                                                                         v Soda Water
//                                                                            v Grenadine

#define FSM_STATES                                                               \
	X(Cosmopolitan,     "Sunrise Splash",                         120, 0, 120, 60,  2) \
	X(Cosmorada,        "Tropical Fizz",                          180, 0, 60,  60,  0) \
	X(LostDaiquiri,     "Orange Sparkler",                        0,   0, 240, 60,  2) \
	X(CrimsonDaiquiri,  "Pineapple\nGrenadine Spritz",            180, 0, 0,   120, 3) \
	X(CapeCodder,       "Sunset Refresher",                       120, 0, 0,   120, 3) \
	X(RumPunch,         "Citrus Blast",                           30,  0, 120, 120, 0) \
	X(Kamikaze,         "Tropical Sunrise",                       120, 0, 60,  120, 3) \
	X(RedKamikaze,      "ORANGE",                                 0,   0, 150, 0,   0) \
	X(VodkaSour,        "PINEAPPLE",                              150, 0, 0,   0,   0) \
	X(Vodkarita,        "priming",                                30 , 0, 30,  30, 30) \
	X(ThatsIt,          "That's it!\n(for now!)",                 0, 0, 0, 0, 0) \
	X(Dispensing,       "Pouring...\nPlease wait...",             0, 0, 0, 0, 0) \
	X(FinishDispensing, "Finished pouring\nEnjoy your drink",     0, 0, 0, 0, 0) \
	X(Start,            "Welcome!\nPlease wait...",               0, 0, 0, 0, 0) \
	X(Error,            "A critical error\nhas occurred...",      0, 0, 0, 0, 0)

typedef enum {
#define X(elem, str, v, r, t, l, c) elem,
	FSM_STATES
#undef X
		TOTAL_STATE_COUNT
} FSMState;

enum Ingredients { VODKA, RUM, TRIPLE_SEC, LIME_JUICE, CRANBERRY_JUICE, NUM_INGREDIENTS };

void print_with_timestamp(FILE *fp, const char *message);
#endif

