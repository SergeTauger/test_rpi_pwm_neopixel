#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "../3rdparty/c-periphery/src/pwm.h"

// Simple standalone buzzer test using Linux PWM (c-periphery)
// Usage: ./test_buzzer <note_idx 0-5> <halftone 0|1>

int buzz_beep_freq[] = {
    1047, 1175, 1319, 1397, 1568, 1760
};

static int pick_frequency_from_index(int note_index) {
    // Map 0..5 to the note constants as in indication_settings.h
    // 0:C6, 1:D6, 2:E6, 3:F6, 4:G6, 5:A6
    /*
    switch (note_index) {
        case 0: return NOTE_LASER;    // C6 1047 Hz
        case 1: return NOTE_COMM;     // D6 1175 Hz
        case 2: return NOTE_STREAM0;  // E6 1319 Hz
        case 3: return NOTE_STREAM1;  // F6 1397 Hz
        case 4: return NOTE_STREAM2;  // G6 1568 Hz
        case 5: return NOTE_STREAM3;  // A6 1760 Hz
        default: return -1;
    }
    */
    if ((note_index < 0) || (note_index > (sizeof(buzz_beep_freq)/sizeof(buzz_beep_freq[0]) - 1))){
        return -1;
    }
    return buzz_beep_freq[note_index];
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <note_idx 0-5> <halftone 0|1>\n", argv[0]);
        return 1;
    }

    int note_idx = atoi(argv[1]);
    int half_flag = atoi(argv[2]);
    int base_freq = pick_frequency_from_index(note_idx);
    if (base_freq <= 0) {
        fprintf(stderr, "Invalid note index. Use 0..5.\n");
        return 1;
    }

    // Compute frequency: half tone or full tone
    int freq_1 = base_freq;
    int freq_2 = half_flag ? (base_freq / 2) : base_freq;

    // Configure PWM: You must set correct chip+channel for your buzzer PWM pin.
    // Commonly on Raspberry Pi, PWM channels are exposed via pwmchipN/pwmM.
    // Adjust these if needed.
    unsigned int pwm_chip = 0;     // /sys/class/pwm/pwmchip0
    unsigned int pwm_channel = 1;  // dma9

    pwm_t *pwm = pwm_new();
    if (!pwm) {
        fprintf(stderr, "Failed to allocate pwm handle\n");
        return 1;
    }
    if (pwm_open(pwm, pwm_chip, pwm_channel) < 0) {
        fprintf(stderr, "pwm_open failed: %s\n", pwm_errmsg(pwm));
        pwm_free(pwm);
        return 1;
    }

    // Helper to play one tone for the given duration (ms)
    int play_tone_ms(pwm_t *p, int freq_hz, int duration_ms) {
        if (freq_hz <= 0) return 0;
        double duty_cycle = 0.5; // 50%
        if (pwm_set_frequency(p, (double)freq_hz) < 0) {
            fprintf(stderr, "pwm_set_frequency for frequency %d failed: %s\n", freq_hz,  pwm_errmsg(p));
            return -1;
        }
        if (pwm_set_duty_cycle(p, duty_cycle) < 0) {
            fprintf(stderr, "pwm_set_duty_cycle failed: %s\n", pwm_errmsg(p));
            return -1;
        }
        if (pwm_enable(p) < 0) {
            fprintf(stderr, "pwm_enable failed: %s\n", pwm_errmsg(p));
            return -1;
        }
        usleep((useconds_t)duration_ms * 1000);
        // Stop
        (void)pwm_disable(p);
        if (pwm_set_duty_cycle(p, 0) < 0) {
            fprintf(stderr, "pwm_set_duty_cycle cleanup failed: %s\n", pwm_errmsg(p));
            return -1;
        }

        return 0;
    }

    int rc = 0;
    if (!half_flag) {
        rc = play_tone_ms(pwm, freq_1, 1000);
    } else {
        rc = play_tone_ms(pwm, freq_1, 500);
        if (rc == 0) rc = play_tone_ms(pwm, freq_2, 500);
    }

    pwm_close(pwm);
    pwm_free(pwm);
    return rc == 0 ? 0 : 1;
}


