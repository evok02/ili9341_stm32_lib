V = 1
ifeq ($(V), 1)
	Q := @
endif

DEVICE          = stm32f103c8t6
OPENCM3_DIR    = ./libopencm3
LDSCRIPT       = linkerscript.ld

PREFIX         ?= arm-none-eabi
STFLASH        = st-flash

OPT            = -Os
DEBUG          = -ggdb3
CSTD           = -std=c11

BINARY         = tft_spi

SRC_DIR        = src
INC_DIR        = inc

DEFS			+= -I$(OPENCM3_DIR)/include

OBJS           += $(SRC_DIR)/$(BINARY).o
OBJS           += $(SRC_DIR)/system.o
OBJS           += $(SRC_DIR)/spi.o
OBJS           += $(SRC_DIR)/tft.o
OBJS           += $(SRC_DIR)/mmc.o

include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/gcc-config.mk

CFLAGS         += $(OPT) $(CSTD) $(DEBUG)
CFLAGS         += -Wextra -Wshadow -Wimplicit-function-declaration
CFLAGS         += -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes
CFLAGS         += -fno-common -ffunction-sections -fdata-sections

CPPFLAGS       += -MD -Wall -Wundef
CPPFLAGS       += -I$(INC_DIR) $(DEFS)

LDFLAGS        += --static -nostartfiles
LDFLAGS        += -Wl,-Map=$(BINARY).map -Wl,--cref
LDFLAGS        += -Wl,--gc-sections

LDLIBS         += -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group

.PHONY: all flash clean rebuild list size

all: $(BINARY).elf $(BINARY).bin $(BINARY).hex

$(BINARY).elf: $(OBJS) $(LDSCRIPT) $(LIBDEPS)
	$(Q)$(LD) $(OBJS) $(LDLIBS) $(LDFLAGS) -T$(LDSCRIPT) $(ARCH_FLAGS) -o $@

$(BINARY).bin: $(BINARY).elf
	$(Q)$(OBJCOPY) -Obinary $< $@

$(BINARY).list: $(BINARY).elf
	$(Q)$(OBJDUMP) -S $< > $@

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) $(ARCH_FLAGS) -o $@ -c $<

flash: $(BINARY).bin
	$(Q)$(STFLASH) write $(BINARY).bin 0x08000000

list: $(BINARY).list

size: $(BINARY).elf
	$(Q)$(SIZE) $<

clean:
	$(Q)$(RM) -f $(BINARY).elf $(BINARY).bin $(BINARY).hex $(BINARY).list $(BINARY).map
	$(Q)$(RM) -f $(OBJS) $(OBJS:.o=.d) generated.* $(DEVICE).ld

rebuild: clean all

include $(OPENCM3_DIR)/mk/genlink-rules.mk
include $(OPENCM3_DIR)/mk/gcc-rules.mk
