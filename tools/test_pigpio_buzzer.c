/* multi_tone.c – play several 1 s square‑wave tones
 *
 * Notes (Hz): 1047, 1175, 1319, 1397, 1568, 1760
 *
 * Compile:
 *     gcc -O2 -Wall multi_tone.c -lpigpio -lrt -pthread -o multi_tone
 *
 * Run (as root or with pigpiod running):
 *     sudo ./multi_tone <GPIO>
 */

#include <stdio.h>
#include <stdlib.h>
#include <pigpio.h>

#define NOTE_COUNT 6
static const double notes[NOTE_COUNT] = {
    1047.0,   /* C6 */
    1175.0,   /* D6 */
    1319.0,   /* E6 */
    1397.0,   /* F6 */
    1568.0,   /* G6 */
    1760.0    /* A6 */
};

/* -------------------------------------------------
 * Helper: generate and transmit a single wave for `duration_s`
 * ------------------------------------------------- */
static void play_tone(int gpio, double freq, double duration_s)
{
    const double period = 1.0 / freq;
    const unsigned half_us = (unsigned)(period * 1e6 / 2.0 + 0.5);

    /* Build a minimal wave (one high + one low) */
    gpioPulse_t pulse[2];
    pulse[0].gpioOn  = (1 << gpio);
    pulse[0].gpioOff = 0;
    pulse[0].usDelay = half_us;          /* high */

    pulse[1].gpioOn  = 0;
    pulse[1].gpioOff = (1 << gpio);
    pulse[1].usDelay = half_us;          /* low */

    gpioWaveClear();
    gpioWaveAddGeneric(2, pulse);
    int wave_id = gpioWaveCreate();
    if (wave_id < 0) {
        fprintf(stderr, "wave creation failed (freq %.1f Hz)\n", freq);
        return;
    }

    /* How many whole repeats fit into the requested duration? */
    unsigned wave_us   = 2 * half_us;                /* length of one wave */
    unsigned total_us  = (unsigned)(duration_s * 1e6);
    unsigned repeats   = total_us / wave_us;
    unsigned remainder = total_us % wave_us;

    /* Send the repeated part */
    gpioWaveTxSend(wave_id, PI_WAVE_MODE_REPEAT);
    gpioDelay(repeats * wave_us);

    /* Send any leftover part */
    if (remainder) {
        gpioWaveTxStop();                     /* stop repeat mode */
        gpioWaveTxSend(wave_id, PI_WAVE_MODE_ONE_SHOT);
        gpioDelay(remainder);
    }

    gpioWaveTxStop();
    gpioWaveDelete(wave_id);
}

/* ------------------------------------------------- */
int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <GPIO>\n", argv[0]);
        return 1;
    }

    int gpio = atoi(argv[1]);

    if (gpioInitialise() < 0) {
        fprintf(stderr, "pigpio initialisation failed\n");
        return 1;
    }

    gpioSetMode(gpio, PI_OUTPUT);
    gpioWrite(gpio, 0);

    for (int i = 0; i < NOTE_COUNT; ++i) {
        printf("Playing %.0f Hz for 1 s (note %d/%d)\n",
               notes[i], i + 1, NOTE_COUNT);
        play_tone(gpio, notes[i], 1.0);
        /* short pause between notes (optional) */
        gpioDelay(200000);   /* 0.2 s silence */
    }

    gpioTerminate();
    return 0;
}
