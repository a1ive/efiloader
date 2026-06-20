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

EFI_CONTEXT gContext;

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

NTSTATUS BLAPI BlApplicationEntry(BOOT_APPLICATION_PARAMETER_BLOCK* BootParameters)
{
	CHAR16 TargetImagePath[MAX_PATH] = DEFAULT_IMAGE_PATH;
	BL_LOADED_APPLICATION_ENTRY LoadedApplicationEntry;
	BL_FIRMWARE_DESCRIPTOR* Firmware = NULL;
	UINTN FirmwareDescriptorSize;
	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	SerialInitialize();
	SerialPrint(L"BlApplicationEntry\n");

	if (BootParameters == NULL || BootParameters->FirmwareParametersOffset == 0)
	{
		SerialPrint(L"BP invalid\n");
		Status = STATUS_INVALID_PARAMETER;
		goto fail;
	}

	Firmware = (BL_FIRMWARE_DESCRIPTOR*)((UINT8*)BootParameters + BootParameters->FirmwareParametersOffset);
	FirmwareDescriptorSize = (BootParameters->ReturnArgumentsOffset > BootParameters->FirmwareParametersOffset) ?
		((UINTN)BootParameters->ReturnArgumentsOffset - (UINTN)BootParameters->FirmwareParametersOffset) :
		sizeof(*Firmware);

	SerialPrint(L"BP size=0x%016llX fw=0x%016llX ret=0x%016llX\n",
		(UINT64)BootParameters->Size,
		(UINT64)BootParameters->FirmwareParametersOffset,
		(UINT64)BootParameters->ReturnArgumentsOffset);

	SerialPrint(L"FW version=0x%016llX size=0x%016llX image=%p st=%p\n",
		(UINT64)Firmware->Version,
		(UINT64)FirmwareDescriptorSize,
		Firmware->ImageHandle,
		(VOID*)Firmware->SystemTable);

	if (Firmware->Version < 1)
	{
		SerialPrint(L"Unsupported FW version %x\n", (UINT64)Firmware->Version);
		Status = STATUS_NOT_SUPPORTED;
		goto fail;
	}

#if defined(_M_X64)
	if (Firmware->Version >= 3)
	{
		SerialPrint(L"FW cr3=0x%016llX gdtr=0x%016llX:0x%016llX idtr=0x%016llX:0x%016llX\n",
			(UINT64)Firmware->ProcessorState.Cr3,
			(UINT64)Firmware->ProcessorState.Gdtr.Base,
			(UINT64)Firmware->ProcessorState.Gdtr.Limit,
			(UINT64)Firmware->ProcessorState.Idtr.Base,
			(UINT64)Firmware->ProcessorState.Idtr.Limit);

		SerialPrint(L"FW cs=0x%016llX ds=0x%016llX es=0x%016llX fs=0x%016llX gs=0x%016llX ss=0x%016llX trans=0x%016llX\n",
			(UINT64)Firmware->ProcessorState.Cs,
			(UINT64)Firmware->ProcessorState.Ds,
			(UINT64)Firmware->ProcessorState.Es,
			(UINT64)Firmware->ProcessorState.Fs,
			(UINT64)Firmware->ProcessorState.Gs,
			(UINT64)Firmware->ProcessorState.Ss£¬
			(UINT64)Firmware->ProcessorState.TranslationEnabled);
	}
	else if (Firmware->Version == 2)
	{
		SerialPrint(L"FW cr3=0x%016llX gdtr=0x%016llX:0x%016llX idtr=0x%016llX:0x%016llX\n",
			(UINT64)Firmware->ProcessorState.Cr3,
			(UINT64)Firmware->ProcessorState.Gdtr.Base,
			(UINT64)Firmware->ProcessorState.Gdtr.Limit,
			(UINT64)Firmware->ProcessorState.Idtr.Base,
			(UINT64)Firmware->ProcessorState.Idtr.Limit);

		SerialPrint(L"FW cs=0x%016llX ds=0x%016llX es=0x%016llX fs=0x%016llX gs=0x%016llX ss=0x%016llX\n",
			(UINT64)Firmware->ProcessorState.Cs,
			(UINT64)Firmware->ProcessorState.Ds,
			(UINT64)Firmware->ProcessorState.Es,
			(UINT64)Firmware->ProcessorState.Fs,
			(UINT64)Firmware->ProcessorState.Gs,
			(UINT64)Firmware->ProcessorState.Ss);
	}
	else
	{
		SerialPrint(L"FW cr3=0x%016llX\n",
			(UINT64)Firmware->ProcessorState.Cr3);
	}
#else
	SerialPrint(L"FW IA32 descriptor size=0x%016llX\n",
		(UINT64)FirmwareDescriptorSize);
#endif

	Status = EfiInitializeContext(Firmware);
	SerialPrint(L"EfiInitializeContext status=0x%08X\n", (UINT64)(UINT32)Status);
	if (Status != STATUS_SUCCESS)
	{
		SerialPrint(L"FW invalid\n");
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

	SerialPrint(L"Restoring FirmwareContext\n");
#if defined(_M_X64)
	if (Firmware->Version >= 2)
	{
		ArchRestoreFirmwareContext(&Firmware->ProcessorState, NULL);
		SerialPrint(L"FirmwareContext restored\n");
	}
	else
	{
		SerialPrint(L"Restoring Firmware CR3\n");
		ArchRestoreFirmwarePageTable(Firmware->ProcessorState.Cr3);
		SerialPrint(L"Firmware CR3 restored\n");
	}
#else
	ArchRestoreFirmwareContext(NULL, NULL);
#endif

	EfiPrint(L"EFILOADER: Copyright (c) 2026 A1ive <https://github.com/a1ive/efiloader>\n");
	EfiPrint(L"Firmware Descriptor v%x\n", (UINT64)Firmware->Version);

	Status = EfiStartApplication(TargetImagePath);

fail:
	for (;;)
	{
		__halt();
	}
}
