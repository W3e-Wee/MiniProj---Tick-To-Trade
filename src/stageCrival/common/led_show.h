#ifndef LED_SHOW_H
#define LED_SHOW_H

/* The LED of a race window or of Stage C. Only built with -Dshow_signals; otherwise every macro is empty.
   LED_START() once before the run, SHOW_LED(sig) for each signal, LED_STOP() once after. */

#ifdef show_signals
#include "led.h" /* LED helper from the brief */

/* shows one signal on the LED (1 = BUY green, -1 = SELL red) */
static inline void show_led(int sig)
{
    if (sig == 1)
    {
        led_off(LED_RED);
        led_on(LED_GREEN);
    }
    else
    {
        led_off(LED_GREEN);
        led_on(LED_RED);
    }
}
#define LED_START() led_init()
#define SHOW_LED(sig) show_led(sig)
#define LED_STOP()          \
    do                      \
    {                       \
        led_off(LED_GREEN); \
        led_off(LED_RED);   \
        led_cleanup();      \
    } while (0)
#else
#define LED_START() ((void)0)
#define SHOW_LED(sig) ((void)0)
#define LED_STOP() ((void)0)
#endif

#endif
