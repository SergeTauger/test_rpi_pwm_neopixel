#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#define BTN_PORT "/dev/gpiochip0"
#define ENC_BTN_PIN 27
#define ENC_A_PIN 23
#define ENC_B_PIN 24
#define RST_BTN_PIN 22
#include "../3rdparty/c-periphery/src/gpio.h"

// Simple GPIO button + encoder tester using c-periphery (character device GPIO)
// Reads 4 inputs: encoder A, encoder B, encoder push button, reset button.
// Prints button state changes and encoder rotation direction (CW/CCW).

typedef struct {
    const char *name;
    unsigned int line;
    gpio_t *h;
    bool last_level;     // raw level as read
} button_t;

static int open_input(const char *chip_path, unsigned int line, gpio_t **out) {
    gpio_t *g = gpio_new();
    if (!g) {
        fprintf(stderr, "gpio_new failed\n");
        return -1;
    }
    gpio_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.direction = GPIO_DIR_IN;
    cfg.edge = GPIO_EDGE_BOTH;         // report changes
    cfg.bias = GPIO_BIAS_PULL_UP;      // many buttons are active-low
    cfg.drive = GPIO_DRIVE_DEFAULT;
    cfg.inverted = false;              // keep raw logic; we'll derive pressed
    cfg.label = "test_buttons";

    if (gpio_open_advanced(g, chip_path, line, &cfg) < 0) {
        fprintf(stderr, "gpio_open_advanced(line=%u) failed: %s\n", line, gpio_errmsg(g));
        gpio_free(g);
        return -1;
    }
    *out = g;
    return 0;
}

static void close_input(gpio_t **pg) {
    if (*pg) {
        gpio_close(*pg);
        gpio_free(*pg);
        *pg = NULL;
    }
}
static bool is_valid_transition(int prev, int cur) 
{
    // Valid moves along quadrature ring: 0<->1<->3<->2<->0
    if (prev == cur) return false;
    switch ((prev << 2) | cur) {
        case 0b0001: case 0b0100: // 0->1, 1->0
        case 0b0111: case 0b1101: // 1->3, 3->1
        case 0b1000: case 0b0010: // 2->0, 0->2
        case 0b1011: case 0b1110: // 2->3, 3->2
            return true;
        default:
            return false;
    }
};

int main(void) {
    const char *chip = BTN_PORT; // from pin_config.h, e.g., "/dev/gpiochip0"

    button_t buttons[4] = {
        {"ENC_A", ENC_A_PIN, NULL, true},
        {"ENC_B", ENC_B_PIN, NULL, true},
        {"ENC_BTN", ENC_BTN_PIN, NULL, true},
        {"RST_BTN", RST_BTN_PIN, NULL, true},
    };

    // Open all inputs
    for (size_t i = 0; i < 4; i++) {
        if (open_input(chip, buttons[i].line, &buttons[i].h) < 0) {
            fprintf(stderr, "Failed to open %s on line %u\n", buttons[i].name, buttons[i].line);
            for (size_t k = 0; k < i; k++) close_input(&buttons[k].h);
            return 1;
        }
        // Prime last_level
        bool level = true;
        if (gpio_read(buttons[i].h, &level) < 0) {
            fprintf(stderr, "gpio_read(%s) failed: %s\n", buttons[i].name, gpio_errmsg(buttons[i].h));
            for (size_t k = 0; k <= i; k++) close_input(&buttons[k].h);
            return 1;
        }
        buttons[i].last_level = level;
        bool pressed = !level; // assuming pull-up, active-low
        printf("%s initial: level=%d pressed=%d\n", buttons[i].name, (int)level, (int)pressed);
    }

    // Prepare encoder state (quadrature)
    // State encoding: (A<<1)|B -> 0..3
    int enc_last = ((buttons[0].last_level ? 1 : 0) << 1) | (buttons[1].last_level ? 1 : 0);
    // Track the last up-to-4 states to detect a full step
    int enc_seq[4] = { enc_last, -1, -1, -1 };
    int enc_len = 1;



    // Poll loop
    printf("Listening for button changes (Ctrl+C to exit)\n");
    while (1) {
        // Use poll_multiple for edge-driven wakeups
        gpio_t *gpios[4] = { buttons[0].h, buttons[1].h, buttons[2].h, buttons[3].h };
        bool ready[4] = { false, false, false, false };
        int rc = gpio_poll_multiple(gpios, 4, 1000 /*ms*/, ready);
        if (rc < 0) {
            fprintf(stderr, "gpio_poll_multiple failed\n");
            break;
        }
        for (size_t i = 0; i < 4; i++) {
            if (!ready[i]) continue;
            bool level = buttons[i].last_level;
            if (gpio_read(buttons[i].h, &level) < 0) {
                fprintf(stderr, "gpio_read(%s) failed: %s\n", buttons[i].name, gpio_errmsg(buttons[i].h));
                continue;
            }
            if (level != buttons[i].last_level) {
                buttons[i].last_level = level;
                if (i == 0 || i == 1) {
                    // Encoder edge on A or B: read both fresh and determine direction
                    bool level_a = buttons[0].last_level;
                    bool level_b = buttons[1].last_level;
                    // Re-read both to avoid race
                    (void)gpio_read(buttons[0].h, &level_a);
                    (void)gpio_read(buttons[1].h, &level_b);
                    int enc_cur = ((level_a ? 1 : 0) << 1) | (level_b ? 1 : 0);
                    if (enc_cur != enc_last) {
                        // Append to sequence if valid; else reset sequence to current state
                        if (is_valid_transition(enc_last, enc_cur)) {
                            if (enc_len < 4) {
                                enc_seq[enc_len++] = enc_cur;
                            } else {
                                // Shift left and append
                                enc_seq[0] = enc_seq[1];
                                enc_seq[1] = enc_seq[2];
                                enc_seq[2] = enc_seq[3];
                                enc_seq[3] = enc_cur;
                            }
                        } else {
                            enc_len = 1;
                            enc_seq[0] = enc_cur;
                        }

                        // Check for full-step patterns when we have 4 entries
                        if (enc_len >= 4) {
                            // CW sequence: [2,0,1,3] => (A,B): (1,0)->(0,0)->(0,1)->(1,1)
                            // CCW sequence: [1,0,2,3] => (A,B): (0,1)->(0,0)->(1,0)->(1,1)
                            bool is_cw  = (enc_seq[enc_len-4] == 2 && enc_seq[enc_len-3] == 0 && enc_seq[enc_len-2] == 1 && enc_seq[enc_len-1] == 3);
                            bool is_ccw = (enc_seq[enc_len-4] == 1 && enc_seq[enc_len-3] == 0 && enc_seq[enc_len-2] == 2 && enc_seq[enc_len-1] == 3);
                            if (is_cw) {
                                printf("ENCODER: CW (A=%d B=%d)\n", level_a ? 1 : 0, level_b ? 1 : 0);
                                fflush(stdout);
                                // Reset to end state to look for next full sequence
                                enc_len = 1; enc_seq[0] = enc_cur;
                            } else if (is_ccw) {
                                printf("ENCODER: CCW (A=%d B=%d)\n", level_a ? 1 : 0, level_b ? 1 : 0);
                                fflush(stdout);
                                enc_len = 1; enc_seq[0] = enc_cur;
                            }
                        }

                        enc_last = enc_cur;
                    }
                } else {
                    bool pressed = !level; // active-low assumption
                    printf("%s %s (level=%d)\n", buttons[i].name, pressed ? "PRESSED" : "RELEASED", (int)level);
                    fflush(stdout);
                }
            }
        }
    }

    for (size_t i = 0; i < 4; i++) close_input(&buttons[i].h);
    return 0;
}


