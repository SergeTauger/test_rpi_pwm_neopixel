/* ws2811_pigpio.c – drive WS2811/WS2812 LEDs with pigpio
 *
 * Compile:
 *     gcc -O2 -Wall ws2811_pigpio.c -lpigpio -lrt -pthread -o ws2811_pigpio
 *
 * Run (needs root or pigpiod):
 *     sudo ./ws2811_pigpio <GPIO> <LED_COUNT>
 *
 * The program lights the strip with a simple rainbow that updates
 * every 100 ms.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pigpio.h>
#include <math.h>
#include <time.h>

#define T0H  350   /* ns, high time for a ‘0’ bit   */
#define T0L  900   /* ns, low  time for a ‘0’ bit   */
#define T1H  900   /* ns, high time for a ‘1’ bit   */
#define T1L  350   /* ns, low  time for a ‘1’ bit   */
#define RES  50000 /* ns, reset gap (>50 µs)        */

/* Convert nanoseconds → pigpio micro‑second units (rounded) */
static inline unsigned ns2us(double ns) { return (unsigned)(ns / 1000.0 + 0.5); }

/* -------------------------------------------------
 * Encode one byte (MSB first) into pigpio pulses.
 * The pulses are appended to the supplied array.
 * ------------------------------------------------- */
static void encode_byte(unsigned char byte,
                        gpioPulse_t *pulses,
                        unsigned *idx,
                        int gpio)
{
    for (int bit = 7; bit >= 0; --bit) {
        int val = (byte >> bit) & 1;
        if (val) {                     /* ‘1’ bit */
            pulses[*idx].gpioOn  = (1 << gpio);
            pulses[*idx].gpioOff = 0;
            pulses[*idx].usDelay = ns2us(T1H);
            (*idx)++;

            pulses[*idx].gpioOn  = 0;
            pulses[*idx].gpioOff = (1 << gpio);
            pulses[*idx].usDelay = ns2us(T1L);
            (*idx)++;
        } else {                       /* ‘0’ bit */
            pulses[*idx].gpioOn  = (1 << gpio);
            pulses[*idx].gpioOff = 0;
            pulses[*idx].usDelay = ns2us(T0H);
            (*idx)++;

            pulses[*idx].gpioOn  = 0;
            pulses[*idx].gpioOff = (1 << gpio);
            pulses[*idx].usDelay = ns2us(T0L);
            (*idx)++;
        }
    }
}

/* -------------------------------------------------
 * Build a wave for the whole LED strip.
 * colour data is an array of 3*leds bytes (GRB order).
 * ------------------------------------------------- */
static int build_wave(int gpio,
                      const unsigned char *colour,
                      unsigned led_count,
                      unsigned *wave_id)
{
    /* Each bit = 2 pulses, each LED = 24 bits → 48 pulses.
       Add a final reset pulse (≈50 µs). */
    unsigned max_pulses = led_count * 24 * 2 + 2;
    gpioPulse_t *pulses = calloc(max_pulses, sizeof(gpioPulse_t));
    if (!pulses) return -1;

    unsigned idx = 0;
    for (unsigned i = 0; i < led_count; ++i) {
        const unsigned char *c = colour + i * 3;   /* GRB */
        encode_byte(c[0], pulses, &idx, gpio);    /* G */
        encode_byte(c[1], pulses, &idx, gpio);    /* R */
        encode_byte(c[2], pulses, &idx, gpio);    /* B */
    }

    /* Reset gap – just keep the line low for >50 µs */
    pulses[idx].gpioOn  = 0;
    pulses[idx].gpioOff = (1 << gpio);
    pulses[idx].usDelay = ns2us(RES);
    idx++;

    gpioWaveClear();
    gpioWaveAddGeneric(idx, pulses);
    *wave_id = gpioWaveCreate();

    free(pulses);
    return (*wave_id >= 0) ? 0 : -1;
}

/* -------------------------------------------------
 * Simple rainbow generator (HSV → RGB → GRB)
 * ------------------------------------------------- */
static void hsv2grb(double h, double s, double v,
                    unsigned char out[3])
{
    double c = v * s;
    double x = c * (1 - fabs(fmod(h / 60.0, 2) - 1));
    double m = v - c;
    double r, g, b;

    if (h < 60)      { r = c; g = x; b = 0; }
    else if (h < 120){ r = x; g = c; b = 0; }
    else if (h < 180){ r = 0; g = c; b = x; }
    else if (h < 240){ r = 0; g = x; b = c; }
    else if (h < 300){ r = x; g = 0; b = c; }
    else             { r = c; g = 0; b = x; }

    out[0] = (unsigned char)((g + m) * 255);   /* G */
    out[1] = (unsigned char)((r + m) * 255);   /* R */
    out[2] = (unsigned char)((b + m) * 255);   /* B */
}

/* ------------------------------------------------- */
int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <GPIO> <LED_COUNT>\n", argv[0]);
        return 1;
    }

    int gpio       = atoi(argv[1]);
    unsigned leds  = (unsigned)atoi(argv[2]);

    if (gpioInitialise() < 0) {
        fprintf(stderr, "pigpio init failed\n");
        return 1;
    }

    gpioSetMode(gpio, PI_OUTPUT);
    gpioWrite(gpio, 0);

    /* Allocate colour buffer (GRB per LED) */
    unsigned char *buf = calloc(leds * 3, 1);
    if (!buf) {
        gpioTerminate();
        return 1;
    }

    unsigned wave_id;
    struct timespec ts = {0, 100 * 1000000L};   /* 100 ms */

    for (int frame = 0; ; ++frame) {
        /* Fill buffer with a moving rainbow */
        for (unsigned i = 0; i < leds; ++i) {
            double hue = fmod((frame * 5.0 + i * 360.0 / leds), 360.0);
            hsv2grb(hue, 1.0, 0.5, &buf[i * 3]);
        }

        if (build_wave(gpio, buf, leds, &wave_id) != 0) break;

        gpioWaveTxSend(wave_id, PI_WAVE_MODE_ONE_SHOT);
        /* Wait until the wave finishes (duration ≈ leds*30 µs) */
        while (gpioWaveTxBusy()) gpioDelay(10);

        gpioWaveDelete(wave_id);
        nanosleep(&ts, NULL);
    }

    free(buf);
    gpioTerminate();
    return 0;
}
