BINARY = btlamp

DBG_SCRIPT = debug.gdb
GDB_PORT = 4444

############
#
# Paths
#
############

toolprefix = arm-none-eabi
toolversion = 6.1.0
sourcedir = src
builddir = build
basetoolsdir = /home/petera/toolchain/${toolprefix}-toolchain-gcc-${toolversion}-hardfloat
tools = ${basetoolsdir}/bin
downloaddir = .download
sdkdir = .nrf52_sdk
softdevdir = .nrf52_softdev
softdevp = ${downloaddir}/${softdevdir}/nrf52_s132.zip
sdkp = ${downloaddir}/${sdkdir}/nrf52_sdk.zip
softdev = nrf52_softdev
sdk = nrf52_sdk
flashscript = nrf52flash.script

CPATH =
SPATH =
INC =
SFILES =
CFILES =

#############
#
# Build tools
#
#############

CROSS_COMPILE=${tools}/arm-none-eabi-
CC = $(CROSS_COMPILE)gcc $(CFLAGS)
AS = $(CROSS_COMPILE)gcc $(AFLAGS)
LD = $(CROSS_COMPILE)ld
GDB = $(CROSS_COMPILE)gdb
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
SIZE = $(CROSS_COMPILE)size
MKDIR = mkdir -p

###############
#
# Build configs
#
###############

LD_SCRIPT = arm.ld
CFLAGS =  $(INC) $(FLAGS) 
CFLAGS += -mcpu=cortex-m4 -mno-thumb-interwork -mthumb -mabi=aapcs
CFLAGS += -Wall -Werror -Os -g3
CFLAGS += -gdwarf-2 -Wno-packed-bitfield-compat
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin --short-enums 
CFLAGS += -nostartfiles -nostdlib 
#CFLAGS += -MP -MD
AFLAGS =  $(CFLAGS)
LFLAGS = --gc-sections -cref
#LFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mcpu=cortex-m4
LFLAGS += -T $(LD_SCRIPT)
OFLAGS_HEX = -O ihex ${builddir}/$(BINARY).elf
OFLAGS_BIN = -O binary ${builddir}/$(BINARY).elf

###############
#
# Files and libs
#
###############

CPATH		+= ${sourcedir}
SPATH		+= ${sourcedir}
INC		+= -I./${sourcedir}

SDK_ROOT = $(sdk)
include sdk.mk
INC += $(INC_PATHS)

AFLAGS += -D__START=main -D__STARTUP_CLEAR_BSS
SFILES += memset.S memcpy.S
CFILES += main.c app.c tnv.c bitmanio_impl.c
CFILES += miniutils.c

LIBS = -L${basetoolsdir}/lib/gcc/${toolprefix}/${toolversion} -lgcc

############
#
# Tasks
#
############

vpath %.c $(CPATH)
vpath %.S $(SPATH)
INCLUDE_DIRECTIVES += $(INC)

SOBJFILES = $(SFILES:%.S=${builddir}/%.o)
OBJFILES = $(CFILES:%.c=${builddir}/%.o)

DEPFILES = $(CFILES:%.c=${builddir}/%.d)

ALLOBJFILES  = $(SOBJFILES)
ALLOBJFILES += $(OBJFILES)

DEPENDENCIES = $(DEPFILES) 

# link object files, create binary for flashing
$(BINARY): mkdirs $(sdk) $(ALLOBJFILES)
	@echo "... linking"
	@${LD} $(LFLAGS) -Map ${builddir}/$(BINARY).map -o ${builddir}/$(BINARY).elf $(ALLOBJFILES) $(LIBS)
	@${OBJCOPY} $(OFLAGS_BIN) ${builddir}/$(BINARY).out
	@${OBJCOPY} $(OFLAGS_HEX) ${builddir}/$(BINARY).hex
	@${OBJDUMP} -hd -j .text -j.data -j .bss -j .bootloader_text -j .bootloader_data -d -S ${builddir}/$(BINARY).elf > ${builddir}/$(BINARY)_disasm.s
	@echo "${BINARY}.out is `du -b ${builddir}/${BINARY}.out | sed 's/\([0-9]*\).*/\1/g '` bytes on flash"

-include $(DEPENDENCIES)

# compile assembly files, arm
$(SOBJFILES) : ${builddir}/%.o:%.S
		@echo "... assembly $@"
		@${MKDIR} $(@D);
		@${AS} -c -o $@ $<
		
# compile c files
$(OBJFILES) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@${MKDIR} $(@D);
		@${CC} -c -o $@ $<

# make dependencies
$(DEPFILES) : ${builddir}/%.d:%.c
		@echo "... depend $@"; \
		rm -f $@; \
		${MKDIR} $(@D); \
		${CC} -M $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*, ${builddir}/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

all: info setup $(BINARY)

setup: download unpack

unpack: $(softdev) $(sdk)

$(softdev): $(softdevdir)
	@ln -s $(wildcard ${softdevdir}/*.hex) $@
	@touch $@
$(sdk): $(sdkdir)
	@ln -s $(sdkdir) $@
	@touch $@
$(softdevdir): $(softdevp)
	@echo ... unpacking softdevice
	-@${MKDIR} ${softdevdir}
	@unzip -qq -o $(softdevp) -d $(softdevdir)
$(sdkdir): $(sdkp)
	@echo ... unpacking sdk
	-@${MKDIR} ${sdkdir}
	@unzip -qq -o $(sdkp) -d $(sdkdir)
	@find $(sdkdir) -type f -name "*.h" -exec sed -i 's/printf/print/g' {} +
	@find $(sdkdir) -type f -name "*.c" -exec sed -i 's/printf/print/g' {} +
	@mv $(sdkdir)/components/libraries/util/app_error.h $(sdkdir)/components/libraries/util/app_error.h.orig

download: mkdirs $(softdevp) $(sdkp)
$(softdevp):
	@echo ... downloading softdevice
	-@${MKDIR} ${downloaddir}
	-@${MKDIR} ${downloaddir}/${softdevdir}
	@wget -q -O $@ https://www.nordicsemi.com/eng/nordic/download_resource/56261/7/12482550
$(sdkp):
	@echo ... downloading sdk
	-@${MKDIR} ${downloaddir}
	-@${MKDIR} ${downloaddir}/${sdkdir}
	@wget -q -O $@ https://www.nordicsemi.com/eng/nordic/download_resource/54291/51/23083258
	
mkdirs:
	-@${MKDIR} ${builddir}

clean:
	@echo ... clean
	@rm -rf ${builddir}
	@rm -f _$(flashscript)

clean-all: clean
	@rm -f ${sdk}
	@rm -f ${softdev}
	@rm -rf ${downloaddir}
	@rm -rf ${softdevdir}
	@rm -rf ${sdkdir}

install: FLASHCMD=flash write_image erase $(shell readlink -f ${builddir}/${BINARY}.hex) 0x00000000 ihex
install: $(BINARY)
	@echo ... flashing ${BINARY}
	@sed -e 's\FLASH\${FLASHCMD}\' $(flashscript) > _$(flashscript)
	@echo "script $(shell readlink -f _$(flashscript))" | nc localhost $(GDB_PORT)
	@${RM} _$(flashscript)

install-softdev: FLASHCMD=flash write_image erase $(shell readlink -f $(softdev)) 0x00000000 ihex
install-softdev: $(softdev)
	@echo ... flashing softdevice
	@sed -e 's\FLASH\${FLASHCMD}\' $(flashscript) > _$(flashscript)
	@echo "script $(shell readlink -f _$(flashscript))" | nc localhost $(GDB_PORT)
	@${RM} _$(flashscript)

debug: $(BINARY)
	@${GDB} ${builddir}/${BINARY}.elf -x $(DBG_SCRIPT)
	
deploy: install-softdev install

info:
	@echo "* Toolchain path:    ${basetoolsdir}"
	@echo "* Building to:       ${builddir}"
	@echo "* Compiler options:  $(CFLAGS)" 
	@echo "* Assembler options: $(AFLAGS)" 
	@echo "* Linker options:    $(LFLAGS)" 
	@echo "* Linker script:     ${LD_SCRIPT}"
	
build-info:
	@echo "*** INCLUDE PATHS"
	@echo "${INC}"
	@echo "*** SOURCE PATHS"
	@echo "${CPATH}"
	@echo "*** ASSEMBLY PATHS"
	@echo "${SPATH}"
	@echo "*** SOURCE FILES"
	@echo "${CFILES}"
	@echo "*** ASSEMBLY FILES"
	@echo "${SFILES}"
	@echo "*** FLAGS"
	@echo "${FLAGS}"

