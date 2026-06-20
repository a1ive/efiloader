;
;  EFILOADER
;  Copyright (C) 2026  a1ive <https://github.com/a1ive>
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

.686p
.model flat, C
.code

; VOID ArchRestoreFirmwareContext(
;     const BL_FIRMWARE_PROCESSOR_STATE* State,
;     const BL_FIRMWARE_PROCESSOR_STATE* PreviousState)
;
; Matches the IA32 bootmgfw.efi ArchSwitchContext semantics:
;   State[0] == 1: firmware context, disable paging and enable interrupts.
;   State[8].bit1 clear: disable interrupts before entering app context.
;   State[8].bit0 set: enable paging, with CR4.PAE set when State[4] == 1.
;
; Current IA32 BOOT_APPLICATION_PARAMETER_BLOCK samples do not export these
; private contexts in the firmware descriptor, so callers may pass NULL.

ArchRestoreFirmwareContext PROC
    mov     ecx, dword ptr [esp + 4]
    mov     edx, dword ptr [esp + 8]
    test    ecx, ecx
    jz      Done

    cmp     dword ptr [ecx], 1
    jne     RestoreApplicationContext

    mov     eax, cr0
    and     eax, 07FFFFFFFh
    mov     cr0, eax

    test    edx, edx
    jz      FirmwareInterrupts

    cmp     dword ptr [edx + 4], 1
    jne     FirmwareInterrupts

    mov     eax, cr4
    and     eax, 0FFFFFFDFh
    mov     cr4, eax

FirmwareInterrupts:
    sti
    ret

RestoreApplicationContext:
    mov     eax, dword ptr [ecx + 8]
    test    al, 2
    jnz     CheckTranslation

    cli
    mov     eax, dword ptr [ecx + 8]

CheckTranslation:
    test    al, 1
    jz      Done

    cmp     dword ptr [ecx + 4], 1
    jne     EnablePaging

    mov     eax, cr4
    or      eax, 20h
    mov     cr4, eax

EnablePaging:
    mov     eax, cr0
    or      eax, 080000000h
    mov     cr0, eax

Done:
    ret
ArchRestoreFirmwareContext ENDP

END
