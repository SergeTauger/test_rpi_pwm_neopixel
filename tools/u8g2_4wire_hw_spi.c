#include "u8g2port.h"


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>


#include "ws2811.h"

// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                21
#define DMA                     10
#define STRIP_TYPE            WS2811_STRIP_RGB

#define LED_COUNT				3
#define BL_LED_IDX				2

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
            .hardware = WS2811_TARGET_PCM,
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
    0x00200000,  // red
    0x00201000,  // orange
	0x00002020,  // lightblue
    0x00202000,  // yellow
    0x00000020,  // blue
    0x00100010,  // purple
	0x00002000,  // green
    0x00200010,  // pink
};

int main(void) {
	ws2811_return_t ret;
	if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
        return ret;
    }
	ledstring.channel[0].leds[BL_LED_IDX] = 0x00A0A0A0;
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

	printf("Initialized ...\n");
	sleep_ms(3000);
	for (size_t k = 0; k < 8; k++){
		ledstring.channel[0].leds[BL_LED_IDX] = dotcolors[k];
	    if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
	    {
	        fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
	        return ret;
	    }
	    sleep_ms(3000);
	}
	u8g2_SetPowerSave(&u8g2, 1);
	// Close and deallocate SPI resources
	done_spi();
	// Close and deallocate GPIO resources
	done_user_data(&u8g2);
	printf("Done\n");

	ledstring.channel[0].leds[BL_LED_IDX] = 0;
    if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "ws2811 cleanup failed: %s\n", ws2811_get_return_t_str(ret));
        return ret;
    }
	ws2811_fini(&ledstring);
	return 0;
}
