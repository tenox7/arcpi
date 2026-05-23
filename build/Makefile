CROSS   ?= arm-linux-gnueabihf-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

ARMDIR    := ../loader/arm
PORTEDDIR := ../loader/ported
INCDIR    := ../loader/inc
PAYLOADDIR := ../loader/payload
OBJDIR    := ../obj

# Freestanding Cortex-A7. NT-source flags: lowercase-named shim headers in
# loader/inc come first; _ARM_ selects the ARM branches added to the copied NT
# headers; _NTSYSTEM_ matches the original bldr build (NTSYSAPI -> empty);
# -fms-extensions allows the MS-style unnamed nested structs/unions NT uses;
# unknown-pragma warnings come from leftover MSVC #pragma warning(...) lines;
# enum-compare/enum-conversion are intentional in the verbatim NT source, which
# mixes MEMORY_TYPE and TYPE_OF_MEMORY (arc.h aliases LoaderFree=MemoryFree etc.,
# so the values match); unused-but-set-variable is dead-but-harmless 1994 code;
# unused-value silences NT's UNREFERENCED_PARAMETER(P) macro, which expands to a
# bare `(P)` expression-statement (used throughout BOOT/LIB, e.g. blio/fatboot).
CFLAGS  := -mcpu=cortex-a7 -marm -mfloat-abi=soft \
           -ffreestanding -fno-pic -fms-extensions -fshort-wchar \
           -fno-unwind-tables -fno-asynchronous-unwind-tables \
           -fno-stack-protector -O2 -Wall -Wextra \
           -Wno-unknown-pragmas -Wno-unused-parameter \
           -Wno-missing-field-initializers -Wno-parentheses -Wno-unused-label \
           -Wno-enum-compare -Wno-enum-conversion -Wno-unused-but-set-variable \
           -Wno-unused-value \
           -I$(INCDIR) -D_ARM_ -D_NTSYSTEM_
# Link base. 0x8000 is the Raspberry Pi firmware's native 32-bit kernel load
# address; QEMU honors it via the ELF LMA (the raw .bin is for SD boot). Override
# with `make TEXT_BASE=0x00010000` to reproduce the earlier QEMU raw -kernel layout.
TEXT_BASE ?= 0x00008000
LDFLAGS := -nostdlib -no-pie -Wl,-T,loader.ld -Wl,--defsym,TEXT_BASE=$(TEXT_BASE) \
           -Wl,--build-id=none -Wl,--no-warn-rwx-segments -Wl,-z,noexecstack

# Handoff-test payload (the serial shell): a standalone freestanding ARM binary,
# no NT headers. Built, raw-binary'd, then wrapped as a linkable blob object
# (_binary_payload_bin_{start,end,size}) embedded in the loader. The loader copies
# it to its link base and jumps. payload.ld's base MUST match PAYLOAD_LOAD_BASE in
# loader/arm/init.c.
PCFLAGS  := -mcpu=cortex-a7 -marm -mfloat-abi=soft -ffreestanding -fno-pic \
            -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables \
            -O2 -Wall -Wextra
PLDFLAGS := -nostdlib -no-pie -Wl,-T,payload.ld -Wl,--build-id=none -Wl,--no-warn-rwx-segments -Wl,-z,noexecstack

ARM_OBJS    := $(OBJDIR)/start.o $(OBJDIR)/uart.o $(OBJDIR)/main.o \
               $(OBJDIR)/clib.o $(OBJDIR)/arcemul.o $(OBJDIR)/memory.o \
               $(OBJDIR)/fsstub.o $(OBJDIR)/stubs.o $(OBJDIR)/init.o \
               $(OBJDIR)/mailbox.o $(OBJDIR)/fb.o $(OBJDIR)/fbcon.o \
               $(OBJDIR)/console.o $(OBJDIR)/usb.o $(OBJDIR)/timer.o \
               $(OBJDIR)/arcdos_rt.o
PORTED_OBJS := $(OBJDIR)/osloader.o $(OBJDIR)/blmisc.o $(OBJDIR)/blmemory.o \
               $(OBJDIR)/blio.o $(OBJDIR)/arcdos.o
OBJS        := $(ARM_OBJS) $(PORTED_OBJS) $(OBJDIR)/payload_blob.o

# arcdos.c is written against the ADK ARC API (plain Open/Read/Print, ADK arc.h
# names) which collide with the loader's NT headers. Compile it in isolation: the
# self-contained loader/arcdos-inc/ compat header ONLY, never -I$(INCDIR). The
# bridge (arcdos_rt.o, built with the normal CFLAGS) connects the two ABIs.
ARCDOS_CFLAGS := -mcpu=cortex-a7 -marm -mfloat-abi=soft -ffreestanding -fno-pic \
                 -fms-extensions -fshort-wchar -fno-stack-protector \
                 -fno-unwind-tables -fno-asynchronous-unwind-tables -O2 -Wall \
                 -Wno-unused-parameter -Wno-unused-variable \
                 -Wno-unused-but-set-variable -Wno-parentheses -Wno-pointer-sign \
                 -I../loader/arcdos-inc -D_ARM_
$(OBJDIR)/arcdos.o: CFLAGS := $(ARCDOS_CFLAGS)

all: $(OBJDIR)/loader.bin

$(OBJDIR)/%.o: $(ARMDIR)/%.S
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(ARMDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(PORTEDDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- handoff-test payload (serial shell) ---
$(OBJDIR)/p_start.o: $(PAYLOADDIR)/start.S
	$(CC) $(PCFLAGS) -c $< -o $@

$(OBJDIR)/p_shell.o: $(PAYLOADDIR)/shell.c
	$(CC) $(PCFLAGS) -c $< -o $@

$(OBJDIR)/payload.elf: $(OBJDIR)/p_start.o $(OBJDIR)/p_shell.o payload.ld
	$(CC) $(PCFLAGS) $(PLDFLAGS) $(OBJDIR)/p_start.o $(OBJDIR)/p_shell.o -o $@

$(OBJDIR)/payload.bin: $(OBJDIR)/payload.elf
	$(OBJCOPY) -O binary $< $@

# Wrap the raw payload as a linkable blob; run in $(OBJDIR) so the emitted symbols
# are the stable _binary_payload_bin_{start,end,size} (no absolute path baked in).
$(OBJDIR)/payload_blob.o: $(OBJDIR)/payload.bin
	cd $(OBJDIR) && $(OBJCOPY) -I binary -O elf32-littlearm -B arm payload.bin payload_blob.o

$(OBJDIR)/loader.elf: $(OBJS) loader.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

$(OBJDIR)/loader.bin: $(OBJDIR)/loader.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJDIR)/*.o $(OBJDIR)/*.elf $(OBJDIR)/*.bin
