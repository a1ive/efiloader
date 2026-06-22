# EFILOADER GNU Make build for GCC/MinGW cross toolchains.

ARCH ?= x64
BUILD_DIR ?= build/$(ARCH)
TARGET ?= $(BUILD_DIR)/efiloader.efi

C_SRCS := bootconfig.c compiler.c loader.c main.c print.c serial.c

ifeq ($(ARCH),x64)
CROSS_COMPILE ?= x86_64-w64-mingw32-
ARCH_DEFS := -D_M_X64
ARCH_CFLAGS := -m64 -mno-red-zone
NASM_FORMAT := win64
ASM_OBJ := $(BUILD_DIR)/arch_amd64.obj
ENTRY := BlApplicationEntry
IMAGE_BASE := 0x18000000
else ifeq ($(ARCH),ia32)
CROSS_COMPILE ?= i686-w64-mingw32-
ARCH_DEFS := -D_M_IX86
ARCH_CFLAGS := -m32
NASM_FORMAT := win32
ASM_OBJ := $(BUILD_DIR)/arch_ia32.obj
ENTRY := _BlApplicationEntry@4
IMAGE_BASE := 0x18000000
else ifeq ($(ARCH),arm64)
CROSS_COMPILE ?= aarch64-w64-mingw32-
ARCH_DEFS := -D_M_ARM64
ARCH_CFLAGS :=
ASM_OBJ := $(BUILD_DIR)/arch_arm64_gcc.o
ENTRY := BlApplicationEntry
IMAGE_BASE := 0x180000000
else
$(error Unsupported ARCH '$(ARCH)'; use ARCH=x64, ARCH=ia32, or ARCH=arm64)
endif

CC := $(CROSS_COMPILE)gcc
NASM ?= nasm
AWK ?= awk

CFLAGS ?= -Os -Wall -Wextra
CPPFLAGS += $(ARCH_DEFS)
CFLAGS += $(ARCH_CFLAGS) -ffreestanding -fno-builtin -fno-stack-protector \
	-fno-asynchronous-unwind-tables -fno-unwind-tables -fshort-wchar
LDFLAGS += -nostdlib -Wl,--entry,$(ENTRY) -Wl,--subsystem,16 \
	-Wl,--image-base,$(IMAGE_BASE) -Wl,--disable-reloc-section

C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
OBJS := $(C_OBJS) $(ASM_OBJ)
DEPS := $(C_OBJS:.o=.d)

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/%.obj: %.asm
	@mkdir -p $(@D)
	$(AWK) -v arch=$(ARCH) '		BEGIN { is_ia32 = (arch == "ia32") } 		/^[[:space:]]*;/ { print; next } 		/^[[:space:]]*$$/ { print; next } 		/^[[:space:]]*\.(code|686p)[[:space:]]*$$/ { next } 		/^[[:space:]]*\.model[[:space:]]/ { next } 		/^[[:space:]]*END[[:space:]]*$$/ { next } 		/^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]+PROC[[:space:]]*$$/ { name = $$1; if (is_ia32) name = "_" name; print "global " name; print name ":"; next } 		/^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]+ENDP[[:space:]]*$$/ { next } 		{ line = $$0; if (line ~ /^[[:space:]]*lea[[:space:]]+r8,[[:space:]]*AfterCsReload/) { print "    lea     r8, [rel AfterCsReload]"; next } gsub(/fword ptr /, "", line); gsub(/qword ptr /, "qword ", line); gsub(/dword ptr /, "dword ", line); gsub(/word ptr /, "word ", line); print line }' $< > $(@:.obj=.nasm)
	$(NASM) -f $(NASM_FORMAT) -o $@ $(@:.obj=.nasm)

clean:
	$(RM) -r build

-include $(DEPS)
