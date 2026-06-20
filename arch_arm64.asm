;
;  EFILOADER
;  Copyright (C) 2026  a1ive <https://github.com/a1ive/efiloader>
;
;  EFILOADER is free software: you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, either version 3 of the License, or
;  (at your option) any later version.
;
;  EFILOADER is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with EFILOADER.  If not, see <http://www.gnu.org/licenses/>.
;

	AREA |.text|, CODE, READONLY, ALIGN=4
	EXPORT ArchRestoreFirmwareContext

; VOID ArchRestoreFirmwareContext(
;     const BL_FIRMWARE_PROCESSOR_STATE* State,
;     const BL_FIRMWARE_PROCESSOR_STATE* PreviousState)
;
; State layout starts at BL_FIRMWARE_DESCRIPTOR_ARM64 + 0x18:
;   +0x00 UINT64 SCTLR
;   +0x08 UINT64 VBAR
;   +0x10 UINT64 Reserved18
;   +0x18 UINT64 Reserved20
;   +0x20 UINT64 TPIDR
;   +0x28 UINT32 CurrentEl
;   +0x30 UINT64 TTBR0
;   +0x38 UINT64 TTBR1
;   +0x40 UINT64 TCR
;   +0x48 UINT64 MAIR
;   +0x50 UINT32 InterruptsEnabled

ArchRestoreFirmwareContext PROC
	cbz		x0, RestoreDone

	msr		daifset, #2

	ldr		x8, [x0, #0x30]
	msr		TTBR0_EL1, x8
	isb		sy

	ldr		x8, [x0, #0x38]
	msr		TTBR1_EL1, x8
	isb		sy

	ldr		x8, [x0, #0x48]
	msr		MAIR_EL1, x8
	isb		sy

	ldr		x8, [x0, #0x40]
	msr		TCR_EL1, x8
	isb		sy

	mov		x8, #0
	tlbi	VMALLE1, x8
	dsb		sy
	isb		sy

	ldr		x8, [x0, #0x20]
	msr		TPIDR_EL1, x8
	dsb		sy

	ldr		x8, [x0]
	msr		SCTLR_EL1, x8
	dsb		sy
	isb		sy

	ldr		x8, [x0, #0x08]
	msr		VBAR_EL1, x8
	isb		sy

	ldr		w8, [x0, #0x50]
	cbz		w8, RestoreDone
	msr		daifclr, #2

RestoreDone
	ret
	ENDP

	END
