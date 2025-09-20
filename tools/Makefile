# tools/Makefile

ROOT := /home/serge/coding/u8g2_rpi
TOOLS_DIR := $(ROOT)/tools

# c-periphery
PERIPHERY_DIR := $(ROOT)/3rdparty/c-periphery
PERIPHERY_INC := -I$(PERIPHERY_DIR)/src
PERIPHERY_LIB := $(PERIPHERY_DIR)/build/libperiphery.a

# rpi_ws281x
WS281X_DIR := $(ROOT)/3rdparty/rpi_ws281x
WS281X_INC := -I$(WS281X_DIR)
WS281X_LIB := $(WS281X_DIR)/build/libws2811.a

# u8g2 port (for SPI + GPIO glue used by test_buzz_spi_bl)
U8G2_PORT_DIR := $(ROOT)/3rdparty/u8g2/sys/arm-linux/port
U8G2_CSRC_DIR := $(ROOT)/3rdparty/u8g2/csrc
U8G2_INC := -I$(U8G2_PORT_DIR) -I$(U8G2_CSRC_DIR)

U8G2_LIB := $(TOOLS_DIR)/libu8g2tools.a
U8G2_SRC := $(wildcard $(U8G2_CSRC_DIR)/*.c)
U8G2_OBJ_DIR := $(TOOLS_DIR)/u8g2_csrc
U8G2_OBJS := $(patsubst $(U8G2_CSRC_DIR)/%.c,$(U8G2_OBJ_DIR)/%.o,$(U8G2_SRC))


CC := gcc
CFLAGS := -O2 -Wall -Wextra -D __ARM_LINUX__ -DPERIPHERY_GPIO_CDEV_SUPPORT=1
LDFLAGS := -lpigpio -lrt -pthread -lm

# Discover tests
TEST_SOURCES := $(wildcard $(TOOLS_DIR)/test_*.c)
TEST_BINS := $(patsubst $(TOOLS_DIR)/%.c,$(TOOLS_DIR)/%,$(TEST_SOURCES))

.PHONY: all clean
all: $(TEST_BINS)

# Generic clean
clean:
	rm -f $(TOOLS_DIR)/test_buttons $(TOOLS_DIR)/test_buzzer $(TOOLS_DIR)/test_buzz_spi_bl) \
	      $(TOOLS_DIR)/u8g2port.o

# --- Per-target rules ---

# test_buttons: needs c-periphery (GPIO)
$(TOOLS_DIR)/test_buttons: $(TOOLS_DIR)/test_buttons.c $(PERIPHERY_LIB)
	$(CC) $(CFLAGS) -I$(ROOT)/src $(PERIPHERY_INC) -o $@ $< $(PERIPHERY_LIB) $(LDFLAGS)

# test_buzzer: needs c-periphery (PWM) and project headers
$(TOOLS_DIR)/test_buzzer: $(TOOLS_DIR)/test_buzzer.c $(PERIPHERY_LIB)
	$(CC) $(CFLAGS) -I$(ROOT)/src $(PERIPHERY_INC) -o $@ $< $(PERIPHERY_LIB) $(LDFLAGS)

#build u8g2 as static lib
$(U8G2_LIB): $(U8G2_OBJS)
	ar rcs $@ $(U8G2_OBJS)

$(U8G2_OBJ_DIR)/%.o: $(U8G2_CSRC_DIR)/%.c
	@mkdir -p $(U8G2_OBJ_DIR)
	$(CC) $(CFLAGS) $(U8G2_INC) -c -o $@ $<


# u8g2port.o: glue used by test_buzz_spi_bl
$(TOOLS_DIR)/u8g2port.o: $(U8G2_PORT_DIR)/u8g2port.c
	$(CC) $(CFLAGS) $(U8G2_INC) $(PERIPHERY_INC) $(WS281X_INC) -c -o $@ $<

# test_cperiphery_buzz_spi_bl: needs c-periphery, ws281x, and u8g2 port
# include the u8g2 lib built when linking test_buzz_spi_bl
$(TOOLS_DIR)/test_cperiphery_buzz_spi_bl: $(TOOLS_DIR)/test_cperiphery_buzz_spi_bl.c $(TOOLS_DIR)/u8g2port.o $(PERIPHERY_LIB) $(WS281X_LIB) $(U8G2_LIB)
	$(CC) $(CFLAGS) $(PERIPHERY_INC) $(WS281X_INC) $(U8G2_INC) -o $@ $< $(TOOLS_DIR)/u8g2port.o \
		$(U8G2_LIB) $(WS281X_LIB) $(PERIPHERY_LIB) $(LDFLAGS)

# test_pigpio_buzz_spi_bl: needs c-periphery, ws281x, and u8g2 port
# include the u8g2 lib built when linking test_buzz_spi_bl
$(TOOLS_DIR)/test_pigpio_buzz_spi_bl: $(TOOLS_DIR)/test_pigpio_buzz_spi_bl.c $(TOOLS_DIR)/u8g2port.o $(PERIPHERY_LIB) $(WS281X_LIB) $(U8G2_LIB)
	$(CC) $(CFLAGS) $(PERIPHERY_INC) $(WS281X_INC) $(U8G2_INC) -o $@ $< $(TOOLS_DIR)/u8g2port.o \
		$(U8G2_LIB) $(WS281X_LIB) $(PERIPHERY_LIB) $(LDFLAGS)
