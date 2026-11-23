#ifndef LED_SHOW_H
#define LED_SHOW_H

/* The LED of a race window or of Stage C. Only built with -Dshow_signals; otherwise every macro is empty.
   LED_START() once before the run, SHOW_LED(sig) for each signal, LED_STOP() once after.

   On a Raspberry Pi 5 every signal now lights TWO sets of LEDs (green = BUY, red = SELL):
     1. the built-in status LEDs         (provided/led.h)
     2. the external red/green GPIO LEDs (shared/led_gpio.h, pins are set inside that file)
   The external LEDs are optional: if they cannot be claimed (not a Pi 5, no permission, pin busy,
   -DLED_NO_GPIO, or not Linux) the program keeps going with the built-in LED / terminal printing only.
   Software cannot tell whether an LED is physically wired to the pin, so with the pins set the program
   works the same whether the external LEDs are plugged in or not.

   Build notes:
     -DLED_NO_GPIO   skip the external LEDs completely (led_gpio.h is not even included)
     sudo -E ./prog  keeps LED_ACTIVE_LOW / LED_GPIOCHIP if you set them (plain sudo strips them) */

#ifdef show_signals

#include <stdio.h>

/* ------------------------------------------------------------------ */
/* 1. Built-in LED (led.h)                                            */
/* ------------------------------------------------------------------ */
/* led.h and led_gpio.h both define led_init / led_on / led_off / led_cleanup and LED_GREEN / LED_RED,
   so they cannot be included together as they are. led.h is included first with its four functions
   renamed to builtin_led_*, then the names are released for led_gpio.h. Neither file is edited. */
#ifdef LED_H
#error "led_show.h must be included before led.h (or without it) so that led.h can be renamed"
#endif

#define led_init builtin_led_init
#define led_on builtin_led_on
#define led_off builtin_led_off
#define led_cleanup builtin_led_cleanup
#include "led.h" /* LED helper from the brief */
#undef led_init
#undef led_on
#undef led_off
#undef led_cleanup
#undef LED_GREEN /* led.h made these macros; led_gpio.h makes them an enum */
#undef LED_RED

/* ------------------------------------------------------------------ */
/* 2. External GPIO LED (led_gpio.h, Linux / Pi 5 only)               */
/* ------------------------------------------------------------------ */
#if defined(__linux__) && !defined(LED_NO_GPIO)
#include "../../shared/led_gpio.h"
#ifdef LED_HAVE_GPIO /* set by led_gpio.h only when <linux/gpio.h> exists */
#define LED_SHOW_EXTERNAL 1
#endif
#endif

/* ------------------------------------------------------------------ */
/* 3. Both LEDs together                                              */
/* ------------------------------------------------------------------ */
#define LS_GREEN 0 /* private colour numbers, the same for both drivers */
#define LS_RED 1

#ifdef LED_SHOW_EXTERNAL
static int led_show_ext = 0; /* 1 = the external LEDs were claimed and work */
#endif

/* turns one LED (LS_GREEN / LS_RED) on or off, on the built-in LED and on the external one */
static inline void led_show_set(int colour, int on)
{
    if (on)
    {
        builtin_led_on(colour);
    }
    else
    {
        builtin_led_off(colour);
    }

#ifdef LED_SHOW_EXTERNAL
    if (led_show_ext)
    {
        enum led_colour c = (colour == LS_GREEN) ? LED_GREEN : LED_RED;
        int rc = on ? led_on(c) : led_off(c);

        if (rc < 0) /* the GPIO driver already printed the reason; stop so it does not repeat for every signal */
        {
            led_show_ext = 0;
            fprintf(stderr, "[led_show] External GPIO LEDs stopped - using the built-in LED only.\n");
        }
    }
#endif
}

/* once, before the run */
static inline void led_show_start(void)
{
    builtin_led_init();

#ifdef LED_SHOW_EXTERNAL
    if (led_init() == 0)
    {
        led_show_ext = 1;
    }
    else
    {
        fprintf(stderr, "[led_show] External GPIO LEDs not available - using the built-in LED only.\n");
    }
#endif
}

/* shows one signal on the LEDs (1 = BUY green, -1 = SELL red) */
static inline void show_led(int sig)
{
    if (sig == 1)
    {
        led_show_set(LS_RED, 0);
        led_show_set(LS_GREEN, 1);
    }
    else
    {
        led_show_set(LS_GREEN, 0);
        led_show_set(LS_RED, 1);
    }
}

/* once, after the run: everything off, built-in LED given back to the system, GPIO pins released */
static inline void led_show_stop(void)
{
    led_show_set(LS_GREEN, 0);
    led_show_set(LS_RED, 0);

#ifdef LED_SHOW_EXTERNAL
    if (led_show_ext)
    {
        led_cleanup(); /* external: turns both off again and releases the pins */
        led_show_ext = 0;
    }
#endif
    builtin_led_cleanup();
}

#define LED_START() led_show_start()
#define SHOW_LED(sig) show_led(sig)
#define LED_STOP() led_show_stop()

#else
#define LED_START() ((void)0)
#define SHOW_LED(sig) ((void)0)
#define LED_STOP() ((void)0)
#endif

#endif
