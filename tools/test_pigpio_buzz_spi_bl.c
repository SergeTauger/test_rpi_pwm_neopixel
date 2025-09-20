#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>

#include <pigpio.h>
// #include "/home/printer/coding/u8g2_rpi/3rdparty/c-periphery/src/pwm.h"

/* Avoid type-name clash with rpi_ws281x's internal pwm.h */
// #define pwm_t ws281x_internal_pwm_t
#include "/home/printer/coding/u8g2_rpi/3rdparty/rpi_ws281x/ws2811.h"
// #undef pwm_t

#include "/home/printer/coding/u8g2_rpi/3rdparty/u8g2/sys/arm-linux/port/u8g2port.h"

#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                21
#define DMA                     10
#define STRIP_TYPE            WS2811_STRIP_RGB

#define LED_COUNT               3
#define BL_LED_IDX              2

#define BUZZER_PIN              16

// GPIO chip number for character device
#define GPIO_CHIP_NUM 0
// SPI bus uses upper 4 bits and lower 4 bits, so 0x10 will be /dev/spidev1.0
#define SPI_BUS 0x00
#define OLED_SPI_PIN_RES            6
#define OLED_SPI_PIN_DC             5

// CS pin is controlled by linux spi driver, thus not defined here, but need to be wired
#define OLED_SPI_PIN_CS             U8X8_PIN_NONE

ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = GPIO_PIN,
            .invert = 0,
            .count = LED_COUNT,
            .strip_type = STRIP_TYPE,
            .brightness = 255,
        },
        [1] =
        {
            .gpionum = 0,
            .invert = 0,
            .count = 0,
            .brightness = 0,
        },
    },
};

ws2811_led_t dotcolors[] =
{
    0x00202020,  // white
    0x00200000,  // red
    0x00201000,  // orange
    0x00002020,  // lightblue
    0x00202000,  // yellow
    0x00000020,  // blue
    0x00100010,  // purple
    0x00002000,  // green
    0x00200010,  // pink
};

int buzz_beep_freq[] = {
    1047, 1175, 1319, 1397, 1568, 1760,
};


// Helper to play one tone for the given duration (s)
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

int main(void) {
    ws2811_return_t ret;
    if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
        return ret;
    }
    ledstring.channel[0].leds[BL_LED_IDX] = 0;
    if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
        return ret;
    }

    u8g2_t u8g2;

    // Initialization
    u8g2_Setup_st7567_jlx12864_f(&u8g2, U8G2_R0,
        u8x8_byte_arm_linux_hw_spi, u8x8_arm_linux_gpio_and_delay);
    
    init_spi_hw(&u8g2, GPIO_CHIP_NUM, SPI_BUS, OLED_SPI_PIN_DC,
        OLED_SPI_PIN_RES, OLED_SPI_PIN_CS);

    u8g2_InitDisplay(&u8g2);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetContrast(&u8g2, 180);

    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    u8g2_DrawStr(&u8g2, 20, 20, "U8g2 HW SPI");

    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);
    u8g2_DrawGlyph(&u8g2, 112, 56, 0x2603);

    u8g2_SendBuffer(&u8g2);

    if (gpioInitialise() < 0) {
        fprintf(stderr, "pigpio initialisation failed\n");
        return 1;
    }

    gpioSetMode(BUZZER_PIN, PI_OUTPUT);
    gpioWrite(BUZZER_PIN, 0);

    printf("Initialized\n");
    for (int k=0; k<6; k++){
        printf("Combination %d\n", k);
        //Set backlight color
        ledstring.channel[0].leds[BL_LED_IDX] = dotcolors[k];
        if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
        {
            fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
            break;
        }        
        //Play buzzer
        // play_tone(BUZZER_PIN, buzz_beep_freq[k], 1.0);
        sleep_ms(3000);
    }
    u8g2_SetPowerSave(&u8g2, 1);
    // Close and deallocate SPI resources
    done_spi();
    // Close and deallocate GPIO resources
    done_user_data(&u8g2);
    

    ledstring.channel[0].leds[BL_LED_IDX] = 0;
    if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "ws2811 cleanup failed: %s\n", ws2811_get_return_t_str(ret));
        return ret;
    }
    ws2811_fini(&ledstring);

    gpioTerminate();
    printf("Done\n");
}
