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

.code

; VOID ArchRestoreFirmwareContext(
;     const BL_FIRMWARE_PROCESSOR_STATE* State,
;     const BL_FIRMWARE_PROCESSOR_STATE* PreviousState)
;
; State layout starts at BL_FIRMWARE_DESCRIPTOR_X64 + 0x1C:
;   +0x00 UINT64 Cr3
;   +0x08 10-byte GDTR
;   +0x12 10-byte IDTR
;   +0x1C UINT16 LDTR
;   +0x1E UINT16 CS
;   +0x20 UINT16 DS
;   +0x22 UINT16 ES
;   +0x24 UINT16 FS
;   +0x26 UINT16 GS
;   +0x28 UINT16 SS                  ; v2 descriptor ends here, total 0x46
;   +0x2A UINT8  TranslationEnabled  ; v3 only, total 0x48 with padding
; This routine intentionally restores only through SS so the same code path
; works with v2 and v3 firmware descriptors.

ArchRestoreFirmwareContext PROC
    mov     rax, qword ptr [rcx]
    mov     cr3, rax

    lea     rdx, qword ptr [rcx + 8]
    lgdt    fword ptr [rdx]
    lidt    fword ptr [rdx + 10]
    lldt    word ptr [rdx + 20]

    movzx   rax, word ptr [rdx + 22]
    lea     r8, AfterCsReload
    push    rax
    push    r8
    db      048h, 0CBh

AfterCsReload:
    mov     ax, word ptr [rdx + 24]
    mov     ds, ax
    mov     ax, word ptr [rdx + 26]
    mov     es, ax
    mov     ax, word ptr [rdx + 30]
    mov     gs, ax
    mov     ax, word ptr [rdx + 28]
    mov     fs, ax
    mov     ax, word ptr [rdx + 32]
    mov     ss, ax

    sti
    ret
ArchRestoreFirmwareContext ENDP

; VOID ArchRestoreFirmwarePageTable(UINT64 Cr3)
;
; Windows 7 x64 firmware descriptors expose the firmware CR3 but not the full
; descriptor-table and segment state present in newer descriptors.
ArchRestoreFirmwarePageTable PROC
    mov     cr3, rcx
    sti
    ret
ArchRestoreFirmwarePageTable ENDP

END
