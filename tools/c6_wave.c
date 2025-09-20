/* c6_wave.c – 1 s, 1047 Hz square wave on a GPIO pin
 *
 * Compile:
 *     gcc -O2 -Wall c6_wave.c -lpigpio -lrt -pthread -o c6_wave
 *
 * Run as root (pigpio needs access to /dev/gpio*):
 *     sudo ./c6_wave <GPIO>
 *
 * The program creates a pigpio wave, transmits it once, and then exits.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pigpio.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <GPIO>\n", argv[0]);
        return 1;
    }

    int gpio = atoi(argv[1]);          /* GPIO number to toggle */
    const double freq   = 1047.0;      /* C6 note, Hz */
    const double period = 1.0 / freq; /* seconds */
    const unsigned micros_half = (unsigned)(period * 1e6 / 2.0 + 0.5);
    /* ½‑period in µs, rounded to nearest integer */

    if (gpioInitialise() < 0) {
        fprintf(stderr, "pigpio initialisation failed\n");
        return 1;
    }

    gpioSetMode(gpio, PI_OUTPUT);
    gpioWrite(gpio, 0);                /* start low */

    /* -------------------------------------------------
     * Build a wave that consists of N full cycles.
     * ------------------------------------------------- */
    const unsigned cycles = 1;         /* we will repeat the wave later */
    gpioPulse_t pulses[2 * cycles];    /* high + low for each cycle */

    for (unsigned i = 0; i < cycles; ++i) {
        /* high part */
        pulses[2*i].gpioOn  = (1 << gpio);
        pulses[2*i].gpioOff = 0;
        pulses[2*i].usDelay = micros_half;

        /* low part */
        pulses[2*i+1].gpioOn  = 0;
        pulses[2*i+1].gpioOff = (1 << gpio);
        pulses[2*i+1].usDelay = micros_half;
    }

    gpioWaveClear();                       /* start a fresh wave */
    gpioWaveAddGeneric(2 * cycles, pulses);/* add the pulses */
    int wave_id = gpioWaveCreate();        /* create the wave */

    if (wave_id < 0) {
        fprintf(stderr, "wave creation failed\n");
        gpioTerminate();
        return 1;
    }

    /* -------------------------------------------------
     * Transmit the wave for exactly 1 s.
     * ------------------------------------------------- */
    unsigned total_us = (unsigned)(1.0 * 1e6);          /* 1 s in µs */
    unsigned wave_us  = 2 * micros_half * cycles;      /* length of one wave */
    unsigned repeats  = total_us / wave_us;            /* whole repeats */
    unsigned remainder_us = total_us % wave_us;        /* leftover time */

    /* Send the full‑repeat part */
    gpioWaveTxSend(wave_id, PI_WAVE_MODE_REPEAT);
    gpioDelay(repeats * wave_us);                     /* block for repeats */

    /* Send the leftover part, if any */
    if (remainder_us) {
        gpioWaveTxStop();                              /* stop repeat mode */
        gpioWaveTxSend(wave_id, PI_WAVE_MODE_ONE_SHOT);
        gpioDelay(remainder_us);
    }

    gpioWaveTxStop();          /* ensure transmission stops */
    gpioWaveDelete(wave_id);   /* free the wave */
    gpioTerminate();           /* clean up pigpio */

    return 0;
}
