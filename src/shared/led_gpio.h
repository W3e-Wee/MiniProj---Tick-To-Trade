#ifndef LED_GPIO_H
#define LED_GPIO_H
/*
 * led_gpio.h - header-only external red/green LED driver for the Raspberry Pi 5.
 *
 * Put this file in the shared/ folder and include it from any program:
 *
 *     #include "../shared/led_gpio.h"
 *
 * Then call, once per program:   led_init()      (at start-up, check it returns 0)
 * and any number of times:       led_on(LED_GREEN) / led_off(LED_RED) ...
 * and once at the end:           led_cleanup()
 *
 * No led.c and no extra library: everything is in this header.
 * Uses the Linux GPIO character device (uAPI v2) on the Pi 5's RP1 chip.
 *
 * ---- SETUP: fill in the two pin numbers just below ----
 *
 * Optional environment variables (both default to "not set"):
 *   LED_ACTIVE_LOW   set to 1 if the LED lights when the pin is LOW
 *   LED_GPIOCHIP     chip override, e.g. "4" or "/dev/gpiochip4"
 *                    (by default the RP1 chip "pinctrl-rp1" is auto-detected)
 *   (sudo strips environment variables, so use  sudo -E ./program  if you set these)
 *
 * Testing on a PC / without a Pi: build with  -DLED_NO_GPIO  and the LEDs are
 * printed in the terminal instead (LED_GREEN ON, ...). The same happens
 * automatically on a non-Linux build.
 *
 * Do NOT include this together with the brief's led_pi.h: both define
 * enum led_colour.
 *
 * Note: the functions are static, so each program (each .c file) has its own
 * copy of the LED state. Call led_init() in only ONE .c file per executable.
 */

/* ================================================================== */
/*  EDIT HERE: the BCM GPIO number each LED is wired to.              */
/*  These are GPIO numbers (e.g. "GPIOxx"), NOT physical header pins. */
/* ================================================================== */
#define LED_GREEN_GPIO_PIN 17 /* <-- put the GREEN LED's BCM GPIO number here */
#define LED_RED_GPIO_PIN 27  /* <-- put the RED LED's BCM GPIO number here */
/* ================================================================== */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* <linux/gpio.h> only exists on Linux; elsewhere the terminal fallback is used.
 * Force the fallback anywhere with:  gcc -DLED_NO_GPIO ... */
#if !defined(LED_NO_GPIO) && defined(__linux__) && defined(__has_include)
#if __has_include(<linux/gpio.h>)
#define LED_HAVE_GPIO 1
#include <linux/gpio.h>
#include <sys/ioctl.h>
#endif
#endif

#ifdef LED_HAVE_GPIO
/* Compile-time checks so a missing or wrong pin number gives a clear error.
 * LED_PIN_EMPTY(x) is true when the macro x was left without a value. */
#define LED_PASTE_(a, b) a##b
#define LED_PASTE(a, b) LED_PASTE_(a, b)
#define LED_PIN_EMPTY(x) (LED_PASTE(1, x) == 1)

#if LED_PIN_EMPTY(LED_GREEN_GPIO_PIN)
#error "led_gpio.h: set LED_GREEN_GPIO_PIN to the green LED's BCM GPIO number"
#elif LED_PIN_EMPTY(LED_RED_GPIO_PIN)
#error "led_gpio.h: set LED_RED_GPIO_PIN to the red LED's BCM GPIO number"
#elif (LED_GREEN_GPIO_PIN) == (LED_RED_GPIO_PIN)
#error "led_gpio.h: green and red cannot use the same GPIO"
#elif (LED_GREEN_GPIO_PIN) < 0 || (LED_GREEN_GPIO_PIN) > 255 || \
    (LED_RED_GPIO_PIN) < 0 || (LED_RED_GPIO_PIN) > 255
#error "led_gpio.h: pin numbers must be BCM GPIO numbers (0-255)"
#endif
#endif /* LED_HAVE_GPIO */

#define LED_MAX_GPIOCHIPS 16
#define LED_RP1_CHIP_LABEL "pinctrl-rp1"

enum led_colour
{
    LED_GREEN = 0,
    LED_RED = 1
};

enum led_mode
{
    LED_MODE_UNINIT = 0,
    LED_MODE_FALLBACK,
    LED_MODE_HARDWARE
};

static enum led_mode led_current_mode = LED_MODE_UNINIT;

#ifdef LED_HAVE_GPIO
static int led_line_fd = -1; /* one request holds both lines: bit0 = green, bit1 = red */

/* Open the GPIO chip that carries the Pi 5 40-pin header. */
static inline int led_open_gpio_chip(void)
{
    char path[128];
    const char *forced = getenv("LED_GPIOCHIP");

    if (forced && *forced)
    {
        if (forced[0] == '/')
        {
            snprintf(path, sizeof path, "%s", forced);
        }
        else
        {
            snprintf(path, sizeof path, "/dev/gpiochip%s", forced);
        }
        return open(path, O_RDWR | O_CLOEXEC);
    }

    int saw_permission_error = 0;
    for (int i = 0; i < LED_MAX_GPIOCHIPS; ++i)
    {
        snprintf(path, sizeof path, "/dev/gpiochip%d", i);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0)
        {
            if (errno == EACCES || errno == EPERM)
            {
                saw_permission_error = 1;
            }
            continue;
        }
        struct gpiochip_info info;
        memset(&info, 0, sizeof info);
        if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) == 0 &&
            strcmp(info.label, LED_RP1_CHIP_LABEL) == 0)
        {
            return fd;
        }
        close(fd);
    }
    errno = saw_permission_error ? EACCES : ENODEV;
    return -1;
}

static inline int led_init_hardware(void)
{
    const unsigned int green = (unsigned int)(LED_GREEN_GPIO_PIN);
    const unsigned int red = (unsigned int)(LED_RED_GPIO_PIN);

    int chip_fd = led_open_gpio_chip();
    if (chip_fd < 0)
    {
        int err = errno;
        fprintf(stderr, "led: cannot open the Pi 5 GPIO chip: %s\n", strerror(err));
        if (err == EACCES || err == EPERM)
        {
            fprintf(stderr, "led: run with sudo, or add your user to the 'gpio' group\n");
        }
        else
        {
            fprintf(stderr, "led: is this a Raspberry Pi 5? (check with: gpiodetect)\n");
        }
        return -1;
    }

    const char *al = getenv("LED_ACTIVE_LOW");
    int active_low = al && al[0] == '1';

    struct gpio_v2_line_request req;
    memset(&req, 0, sizeof req);
    req.offsets[LED_GREEN] = green;
    req.offsets[LED_RED] = red;
    req.num_lines = 2;
    snprintf(req.consumer, sizeof req.consumer, "tick-to-trade-led");
    req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    if (active_low)
    {
        req.config.flags |= GPIO_V2_LINE_FLAG_ACTIVE_LOW;
    }
    /* No output_values attribute => both lines start inactive (LEDs off). */

    int rc = ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req);
    int err = errno;
    close(chip_fd); /* the line fd stays valid on its own */

    if (rc < 0)
    {
        fprintf(stderr, "led: cannot claim GPIO %u (green) / %u (red): %s\n",
                green, red, strerror(err));
        if (err == EBUSY)
        {
            fprintf(stderr, "led: pin already in use - another program or an old run?\n");
        }
        return -1;
    }

    led_line_fd = req.fd;
    led_current_mode = LED_MODE_HARDWARE;
    return 0;
}
#endif /* LED_HAVE_GPIO */

static inline int led_write_line(enum led_colour colour, int value)
{
    if (colour != LED_GREEN && colour != LED_RED)
    {
        fprintf(stderr, "led: invalid colour %d\n", (int)colour);
        return -1;
    }

    if (led_current_mode == LED_MODE_FALLBACK)
    {
        printf("LED_%s %s\n", colour == LED_GREEN ? "GREEN" : "RED", value ? "ON" : "OFF");
        fflush(stdout);
        return 0;
    }
    if (led_current_mode != LED_MODE_HARDWARE)
    {
        fprintf(stderr, "led: led_init() has not been called successfully\n");
        return -1;
    }

#ifdef LED_HAVE_GPIO
    struct gpio_v2_line_values values;
    memset(&values, 0, sizeof values);
    values.mask = 1ULL << colour; /* only touch this LED */
    values.bits = value ? (1ULL << colour) : 0ULL;

    if (ioctl(led_line_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0)
    {
        fprintf(stderr, "led: set value failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
#else
    return -1; /* unreachable: hardware mode is never set without GPIO support */
#endif
}

/* ---- public API ---- */

/* Call once at start-up. Returns 0 on success, -1 on failure. */
static inline int led_init(void)
{
    if (led_current_mode != LED_MODE_UNINIT)
    {
        return 0; /* already initialised; keep init idempotent */
    }

#ifdef LED_HAVE_GPIO
    return led_init_hardware();
#else
    puts("LED: no GPIO support in this build (LED_NO_GPIO or not Linux); using terminal fallback.");
    led_current_mode = LED_MODE_FALLBACK;
    return 0;
#endif
}

static inline int led_on(enum led_colour colour)
{
    return led_write_line(colour, 1);
}

static inline int led_off(enum led_colour colour)
{
    return led_write_line(colour, 0);
}

/* Call once at the end: LEDs off, pins released back to the kernel. */
static inline void led_cleanup(void)
{
    if (led_current_mode == LED_MODE_UNINIT)
    {
        return;
    }
    (void)led_off(LED_GREEN);
    (void)led_off(LED_RED);

#ifdef LED_HAVE_GPIO
    if (led_current_mode == LED_MODE_HARDWARE && led_line_fd >= 0)
    {
        close(led_line_fd);
        led_line_fd = -1;
    }
#endif
    led_current_mode = LED_MODE_UNINIT;
}

#endif /* LED_GPIO_H */
