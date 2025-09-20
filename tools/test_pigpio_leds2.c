/* ws2811_fixed3.c – 3‑LED strip, custom colour pattern
 *
 * Compile:
 *     gcc -O2 -Wall ws2811_fixed3.c -lpigpio -lrt -pthread -lm -o ws2811_fixed3
 *
 * Run (root or with pigpiod):
 *     sudo ./ws2811_fixed3 <GPIO>
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pigpio.h>
#include <math.h>
#include <time.h>

/* WS2811 timing (ns) */
#define T0H  350
#define T0L  900
#define T1H  900
#define T1L  350
#define RES  50000   /* reset gap >50 µs */

static inline unsigned ns2us(double ns) { return (unsigned)(ns / 1000.0 + 0.5); }

/* -------------------------------------------------
 * Encode one byte (MSB first) into pigpio pulses.
 * ------------------------------------------------- */
static void encode_byte(unsigned char byte,
                        gpioPulse_t *pulses,
                        unsigned *idx,
                        int gpio)
{
    for (int bit = 7; bit >= 0; --bit) {
        int val = (byte >> bit) & 1;
        if (val) {                     /* ‘1’ */
            pulses[*idx].gpioOn  = (1 << gpio);
            pulses[*idx].gpioOff = 0;
            pulses[*idx].usDelay = ns2us(T1H);
            (*idx)++;
            pulses[*idx].gpioOn  = 0;
            pulses[*idx].gpioOff = (1 << gpio);
            pulses[*idx].usDelay = ns2us(T1L);
            (*idx)++;
        } else {                       /* ‘0’ */
            pulses[*idx].gpioOn  = (1 << gpio);
            pulses[*idx].gpioOff = 0;
            pulses[*idx].usDelay = ns2us(T0H);
            (*idx)++;
            ;
            pulses[*idx].gpioOn  = 0;
            pulses[*idx].gpioOff = (1 << gpio);
            pulses[*idx].usDelay = ns2us(T0L);
            (*idx)++;
        }
    }
}

/* -------------------------------------------------
 * Build a wave for the three LEDs.
 * colour[] must contain 3*3 = 9 bytes (GRB per LED).
 * ------------------------------------------------- */
static int build_wave(int gpio,
                      const unsigned char *colour,
                      unsigned *wave_id)
{
    const unsigned max_pulses = 3 * 24 * 2 + 2;   /* 3 LEDs, 24 bits each, + reset */
    gpioPulse_t *pulses = calloc(max_pulses, sizeof(gpioPulse_t));
    if (!pulses) return -1;

    unsigned idx = 0;
    for (int led = 0; led < 3; ++led) {
        const unsigned char *c = colour + led * 3;   /* GRB */
        encode_byte(c[0], pulses, &idx, gpio);      /* G */
        encode_byte(c[1], pulses, &idx, gpio);      /* R */
        encode_byte;
        encode_byte(c[2], pulses, &idx, gpio);      /* B */
    }

    /* Reset gap */
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
 * Colour tables
 * ------------------------------------------------- */
static const unsigned int rgb_dotcolors[] = {
    0x00200000,  // red
    0x00201000,  // orange
    0x00002020,  // lightblue
    0x00202000,  // yellow
    0x00000020,  // blue
    0x00100010,  // purple
    0x00002000,  // green
    0x00200010   // pink
};

/* Helper: unpack 0x00RRGGBB into GRB byte array */
static void unpack_rgb(unsigned int rgb, unsigned char out[3])
{
    unsigned char r = (rgb >> 16) & 0xFF;
    unsigned char g = (rgb >> 8 ) & 0xFF;
    unsigned char b =  rgb        & 0xFF;
    out[0] = g;   /* GRB order required by WS2811 */
    out[1] = r;
    out[2] = b;
}

/* -------------------------------------------------
 * Main loop
 * ------------------------------------------------- */
int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <GPIO>\n", argv[0]);
        return 1;
    }

    int gpio = atoi(argv[1]);

    if (gpioInitialise() < 0) {
        fprintf(stderr, "pigpio init failed\n");
        return 1;
    }

    gpioSetMode(gpio, PI_OUTPUT);
    gpioWrite(gpio, 0);

    /* Buffer for 3 LEDs (GRB per LED) */
    unsigned char colours[3 * 3] = {0};

    const double cycle_time = 3.0;               /* total period for LED 0 */
    const double dot_step = cycle_time / 8.0;      /* 0.375 s per colour for LED 2 */
    struct timespec sleep_ts = {0, 20000000L};    /* 20 ms loop granularity */

    double t_start = 0.0;
    struct timespec start_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);
    t_start = start_ts.tv_sec + start_ts.tv_nsec * 1e-9;

    while (1) {
        /* ---- compute elapsed time ---- */
        struct timespec now_ts;
        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        double now = now_ts.tv_sec + now_ts.tv_nsec * 1e-9;
        double elapsed = now - t_start;

        /* ---- LED 0 : red → green → blue (1 s each) ---- */
        int phase0 = ((int)fmod(elapsed, cycle_time)) / 1;   /* 0,1,2 */
        switch (phase0) {
            case 0: unpack_rgb(0x00200000, &colours[0]); break;   /* red   */
            case 1: unpack_rgb(0x00002000, &colours[0]); break;   /* green */
            case 2: unpack_rgb(0x00000020, &colours[0]); break;   /* blue  */
        }

        /* ---- LED 1 : always off ---- */
        colours[3] = colours[4] = colours[5] = 0;

        /* ---- LED 2 : step through rgb_dotcolors ---- */
        int idx = (int)fmod(elapsed / dot_step, 8.0);
        unpack_rgb(rgb_dotcolors[idx], &colours[6]);

        /* ---- build and send wave ---- */
        unsigned wave_id;
        if (build_wave(gpio, colours, &wave_id) == 0) {
            gpioWaveTxSend(wave_id, PI_WAVE_MODE_ONE_SHOT);
            while (gpioWaveTxBusy()) gpioDelay(10);
            gpioWaveDelete(wave_id);
        }

        nanosleep(&sleep_ts, NULL);
    }

    gpioTerminate();
    return 0;
}
