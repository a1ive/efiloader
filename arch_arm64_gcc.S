/* EFILOADER GCC/MinGW ARM64 assembly support. */

.text
.align 4
.globl ArchRestoreFirmwareContext
#if defined(__MINGW32__) || defined(__MINGW64__)
.def ArchRestoreFirmwareContext; .scl 2; .type 32; .endef
#endif
ArchRestoreFirmwareContext:
    cbz     x0, .LRestoreDone

    msr     daifset, #2

    ldr     x8, [x0, #0x30]
    msr     TTBR0_EL1, x8
    isb     sy

    ldr     x8, [x0, #0x38]
    msr     TTBR1_EL1, x8
    isb     sy

    ldr     x8, [x0, #0x48]
    msr     MAIR_EL1, x8
    isb     sy

    ldr     x8, [x0, #0x40]
    msr     TCR_EL1, x8
    isb     sy

    mov     x8, #0
    tlbi    VMALLE1, x8
    dsb     sy
    isb     sy

    ldr     x8, [x0, #0x20]
    msr     TPIDR_EL1, x8
    dsb     sy

    ldr     x8, [x0]
    msr     SCTLR_EL1, x8
    dsb     sy
    isb     sy

    ldr     x8, [x0, #0x08]
    msr     VBAR_EL1, x8
    isb     sy

    ldr     w8, [x0, #0x50]
    cbz     w8, .LRestoreDone
    msr     daifclr, #2

.LRestoreDone:
    ret
