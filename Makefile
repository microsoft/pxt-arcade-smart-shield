PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)as

_IGNORE0 := $(shell test -f Makefile.user || cp sample-Makefile.user Makefile.user)
_IGNORE1 := $(shell test -f jacdac-c/jacdac/README.md || git submodule update --init --recursive 1>&2)

include Makefile.user

DRV = stm32f0xx_hal_driver
DEFINES = -DUSE_FULL_ASSERT -DUSE_FULL_LL_DRIVER
WARNFLAGS = -Wall -Werror
CFLAGS = $(DEFINES) \
	-mthumb -mfloat-abi=soft  \
	-Os -g3 -Wall -ffunction-sections -fdata-sections \
	$(WARNFLAGS)
BUILT = built/$(TARGET)
JD_DISPLAY_HEADER_PATH = jacdac-c/jacdac/dist/c
JD_PROTO_HEADER_PATH = jacdac-c/inc
HEADERS = $(wildcard src/*.h) $(wildcard $(JD_DISPLAY_HEADER_PATH)/*.h) $(wildcard $(JD_PROTO_HEADER_PATH)/*.h)

include targets/$(TARGET)/config.mk

ifneq ($(BMP),)
BMP_PORT = $(shell ls -1 /dev/cu.usbmodem????????1 | head -1)
endif

C_SRC += $(wildcard src/*.c) 
C_SRC += $(HALSRC)

AS_SRC = targets/$(TARGET)/startup.s
LD_SCRIPT = targets/$(TARGET)/linker.ld

V = @

OBJ = $(addprefix $(BUILT)/,$(C_SRC:.c=.o) $(AS_SRC:.s=.o))

CPPFLAGS = \
	-I$(DRV)/Inc \
	-I$(DRV)/Inc/Legacy \
	-Icmsis_device_f0/Include \
	-Icmsis_core/Include \
	-Itargets/$(TARGET) \
	-Isrc \
	-I$(JD_DISPLAY_HEADER_PATH) \
	-I$(JD_PROTO_HEADER_PATH) \
	-Ijacdac-c \
	-I$(BUILT)

LDFLAGS = -specs=nosys.specs -specs=nano.specs \
	-T"$(LD_SCRIPT)" -Wl,-Map=$(BUILT)/output.map -Wl,--gc-sections

MAKE_FLAGS ?= -j8

all:
	$(MAKE) $(MAKE_FLAGS) $(BUILT)/binary.hex

drop:
	$(MAKE) TARGET=g031 all
	$(MAKE) TARGET=f031 all

r: run

run: all prep-built-gdb
ifeq ($(BMP),)
	$(OPENOCD) -c "program $(BUILT)/binary.elf verify reset exit"
else
	echo "load" >> built/debug.gdb
	echo "quit" >> built/debug.gdb
	arm-none-eabi-gdb --command=built/debug.gdb < /dev/null
endif

prep-built-gdb:
	echo "file $(BUILT)/binary.elf" > built/debug.gdb
ifeq ($(BMP),)
	echo "target extended-remote | $(OPENOCD) -f ./scripts/gdbdebug.cfg" >> built/debug.gdb
else
	echo "target extended-remote $(BMP_PORT)" >> built/debug.gdb
	echo "monitor swdp_scan" >> built/debug.gdb
	echo "attach 1" >> built/debug.gdb
endif

gdb: prep-built-gdb
	arm-none-eabi-gdb --command=built/debug.gdb

$(BUILT)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo CC $<
	$(V)$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

$(wildcard $(BUILT)/src/*.o): $(HEADERS)

$(BUILT)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo AS $<
	$(V)$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

$(BUILT)/binary.elf: $(OBJ) Makefile
	@echo LD $@
	$(V)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) -lm

$(BUILT)/binary.hex: $(BUILT)/binary.elf
	@echo HEX $<
	$(PREFIX)objcopy -O ihex $< $@
	$(PREFIX)size $<

clean:
	rm -rf built
