/*
 *  EFILOADER
 *  Copyright (C) 2026  a1ive <https://github.com/a1ive>
 *
 *  EFILOADER is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  EFILOADER is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with EFILOADER.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <intrin.h>

#include "arch.h"
#include "bootconfig.h"
#include "loader.h"
#include "print.h"
#include "serial.h"
#include "types.h"

static VOID CopyString16(CHAR16* Destination, const CHAR16* Source, UINTN DestinationCharacters)
{
	UINTN Index;

	if (DestinationCharacters == 0)
	{
		return;
	}

	Index = 0;
	while ((Index + 1 < DestinationCharacters) && (Source[Index] != 0))
	{
		Destination[Index] = Source[Index];
		Index++;
	}

	Destination[Index] = 0;
}

#define DEFAULT_IMAGE_PATH L"\\shell.efi"

NTSTATUS BlApplicationEntry(BOOT_APPLICATION_PARAMETER_BLOCK* BootParameters)
{
	CHAR16 TargetImagePath[MAX_PATH] = DEFAULT_IMAGE_PATH;
	BL_LOADED_APPLICATION_ENTRY LoadedApplicationEntry;
	BL_FIRMWARE_DESCRIPTOR_X64* Firmware = NULL;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* ConOut = NULL;
	EFI_SYSTEM_TABLE* SystemTable = NULL;
	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	SerialInitialize();
	SerialPrint(L"BlApplicationEntry\n");

	if (BootParameters == NULL || BootParameters->FirmwareParametersOffset == 0)
	{
		SerialPrint(L"BP invalid\n");
		Status = STATUS_INVALID_PARAMETER;
		goto fail;
	}

	Firmware = (BL_FIRMWARE_DESCRIPTOR_X64*)((UINT8*)BootParameters + BootParameters->FirmwareParametersOffset);

	SerialPrint(L"FW version=0x%016llX image=%p st=%p\n",
		(UINT64)Firmware->Version,
		Firmware->ImageHandle,
		(VOID*)Firmware->SystemTable);

	SerialPrint(L"FW cr3=0x%016llX gdtr=0x%016llX:0x%016llX idtr=0x%016llX:0x%016llX\n",
		Firmware->ProcessorState.Cr3,
		Firmware->ProcessorState.Gdtr.Base,
		(UINT64)Firmware->ProcessorState.Gdtr.Limit,
		Firmware->ProcessorState.Idtr.Base,
		(UINT64)Firmware->ProcessorState.Idtr.Limit);

	SerialPrint(L"FW cs=0x%016llX ds=0x%016llX es=0x%016llX fs=0x%016llX gs=0x%016llX ss=0x%016llX trans=0x%016llX\n",
		(UINT64)Firmware->ProcessorState.Cs,
		(UINT64)Firmware->ProcessorState.Ds,
		(UINT64)Firmware->ProcessorState.Es,
		(UINT64)Firmware->ProcessorState.Fs,
		(UINT64)Firmware->ProcessorState.Gs,
		(UINT64)Firmware->ProcessorState.Ss,
		(UINT64)Firmware->ProcessorState.TranslationEnabled);

	if (Firmware->Version < 2)
	{
		SerialPrint(L"Unsupported FW version %x\n", Firmware->Version);
		Status = STATUS_NOT_SUPPORTED;
		goto fail;
	}

	if ((Firmware->ImageHandle == NULL) || (Firmware->SystemTable == NULL))
	{
		SerialPrint(L"FW invalid\n");
		Status = STATUS_INVALID_PARAMETER;
		goto fail;
	}

	Status = BlGetLoadedApplicationEntry(BootParameters, &LoadedApplicationEntry);
	SerialPrint(L"BlGetLoadedApplicationEntry status=0x%08X\n", (UINT64)(UINT32)Status);
	if (Status == STATUS_SUCCESS)
	{
		Status = BlGetBootOptionString(LoadedApplicationEntry.BcdData, BCD_BOOTAPP_LOAD_OPTIONS, TargetImagePath, MAX_PATH);
		SerialPrint(L"BCD loadoptions status=0x%08X\n", (UINT64)(UINT32)Status);

		if (Status != STATUS_SUCCESS)
			CopyString16(TargetImagePath, DEFAULT_IMAGE_PATH, MAX_PATH);

		SerialPrint(L"Resolved target image path=%s\n", TargetImagePath);
	}

	SystemTable = Firmware->SystemTable;
	ConOut = SystemTable->ConOut;
	SerialPrint(L"ConOut=%p OutputString=%p\n", (VOID*)ConOut, (VOID*)(UINTN)ConOut->OutputString);

	SerialPrint(L"restoring FirmwareContext\n");
	ArchRestoreFirmwareContext(&Firmware->ProcessorState);
	SerialPrint(L"FirmwareContext restored\n");

	EfiPrint(ConOut, L"EFILOADER: Copyright (c) 2026 A1ive <https://github.com/a1ive>\n");

	Status = EfiStartEfiApplication(Firmware, TargetImagePath);

fail:
	for (;;)
	{
		__halt();
	}
}
