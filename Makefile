# devkitARM path
DEVKITARM ?=	/opt/devkitARM

# Prefix
PREFIX	=	$(DEVKITARM)/bin/arm-eabi-

# Executables
CC	=	$(PREFIX)gcc
LD	=	$(PREFIX)gcc
STRIP	=	./stripios

# Flags
ARCH	=	-mcpu=arm926ej-s -mthumb -mthumb-interwork -mbig-endian
CFLAGS	=	$(ARCH) -I. -fomit-frame-pointer -Os -Wall -Wstrict-prototypes -ffunction-sections
LDFLAGS	=	$(ARCH) -nostartfiles -Wl,-T,link.ld,-Map,$(TARGET).map -Wl,--gc-sections -Wl,-static

# Libraries
LIBS	=

# Target
TARGET	=	fat-module

# Objects
OBJS	=	fat_wrapper.o		\
		ipc.o			\
		main.o			\
		mem.o			\
		sdio.o			\
		start.o			\
		syscalls.o		\
		timer.o			\
		usbstorage.o		\
		libfat/cache.o		\
		libfat/directory.o	\
		libfat/disc.o		\
		libfat/fatdir.o		\
		libfat/fatfile.o	\
		libfat/file_allocation_table.o	\
		libfat/filetime.o	\
		libfat/libfat.o		\
		libfat/partition.o


$(TARGET).elf: $(OBJS)
	@echo -e " LD\t$@"
	@$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o $@.orig
	@$(STRIP) $@.orig $@

%.o: %.s
	@echo -e " CC\t$@"
	@$(CC) $(CFLAGS) -D_LANGUAGE_ASSEMBLY -c -x assembler-with-cpp -o $@ $<

%.o: %.c
	@echo -e " CC\t$@"
	@$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo -e "Cleaning..."
	@rm -f $(OBJS) $(TARGET).elf $(TARGET).elf.orig $(TARGET).map
