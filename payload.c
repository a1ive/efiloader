/*
 *  EFILOADER
 *  Copyright (C) 2026  a1ive <https://github.com/a1ive/efiloader>
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

#include "payload.h"
#include "mm.h"
#include "loader.h"
#include "pe.h"
#include "print.h"
#include "resource.h"
#include "serial.h"

static VOID ConnectAll(EFI_HANDLE DriverImageHandle)
{
	EFI_HANDLE DriverImageHandles[2];
	EFI_HANDLE* Handles = NULL;
	UINTN HandleCount = 0;
	UINTN Index;
	EFI_STATUS Status;

	SerialPrint(L"LocateHandleBuffer(AllHandles)\n");
	Status = gContext.BootServices->LocateHandleBuffer(AllHandles, NULL, NULL, &HandleCount, &Handles);
	PrintEfiStatus(L"LocateHandleBuffer(AllHandles)", Status);
	SerialPrint(L"Controller handle count=0x%016llX buffer=%p\n", (UINT64)HandleCount, (VOID*)Handles);
	if (EFI_ERROR(Status) || (Handles == NULL))
	{
		return;
	}

	DriverImageHandles[0] = DriverImageHandle;
	DriverImageHandles[1] = NULL;
	for (Index = 0; Index < HandleCount; Index++)
	{
		SerialPrint(L"ConnectController handle=%p driver=%p before\n", Handles[Index], DriverImageHandle);
		Status = gContext.BootServices->ConnectController(Handles[Index], DriverImageHandles, NULL, TRUE);
		PrintEfiStatus(L"ConnectController", Status);
	}

	EfiFreePool(Handles);
	SerialPrint(L"ConnectAll Done\n");
}

NTSTATUS PayloadLocate(
	const VOID* LoaderImageBase,
	UINTN LoaderImageSize,
	EMBEDDED_PAYLOAD* Payload)
{
	const VOID* ResourceData;
	UINT32 ResourceSize;
	UINT16 Subsystem;
	NTSTATUS Status;

	if (Payload == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}

	Payload->Data = NULL;
	Payload->Size = 0;
	Payload->Subsystem = 0;
	Payload->Type = EmbeddedPayloadNone;

	ResourceData = NULL;
	ResourceSize = 0;
	Status = PeFindResData(LoaderImageBase, LoaderImageSize, EFI_RESOURCE_TYPE_RCDATA, IDR_EFI_PAYLOAD, &ResourceData, &ResourceSize);
	if (Status != STATUS_SUCCESS)
	{
		return Status;
	}

	if ((ResourceData == NULL) || (ResourceSize == 0))
	{
		return STATUS_INVALID_PARAMETER;
	}

	Subsystem = 0;
	Status = PeGetImageSubsystem(ResourceData, ResourceSize, &Subsystem);
	if (Status != STATUS_SUCCESS)
	{
		return Status;
	}

	Payload->Data = ResourceData;
	Payload->Size = ResourceSize;
	Payload->Subsystem = Subsystem;
	switch (Subsystem)
	{
	case IMAGE_SUBSYSTEM_EFI_APPLICATION:
		Payload->Type = EmbeddedPayloadApplication;
		break;

	case IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER:
	case IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER:
	case IMAGE_SUBSYSTEM_EFI_ROM:
		Payload->Type = EmbeddedPayloadDriver;
		break;

	default:
		Payload->Type = EmbeddedPayloadUnsupported;
		break;
	}

	return STATUS_SUCCESS;
}

VOID PayloadBoot(
	const EMBEDDED_PAYLOAD* Payload)
{
	EFI_HANDLE ImageHandle;
	NTSTATUS Status;

	if ((Payload == NULL) || (Payload->Type == EmbeddedPayloadNone))
	{
		EfiPrint(L"Embedded payload not found\n");
		return;
	}

	if (Payload->Type == EmbeddedPayloadUnsupported)
	{
		EfiPrint(L"Embedded payload subsystem 0x%x is not supported\n", (UINT64)Payload->Subsystem);
		return;
	}

	EfiPrint(L"Loading embedded EFI %s\n",
		(Payload->Type == EmbeddedPayloadDriver) ? L"driver" : L"application");
	SerialPrint(L"Embedded payload data=%p size=0x%016llX subsystem=0x%04X\n",
		(VOID*)Payload->Data,
		(UINT64)Payload->Size,
		(UINT64)Payload->Subsystem);

	ImageHandle = NULL;
	Status = EfiStartApplication(NULL, Payload->Data, Payload->Size, &ImageHandle);
	SerialPrint(L"EfiStartApplication(buffer) status=0x%08X ImageHandle=%p\n", (UINT64)(UINT32)Status, ImageHandle);
	if (Status != STATUS_SUCCESS)
	{
		EfiPrint(L"Embedded payload failed to start\n");
		return;
	}

	if (Payload->Type == EmbeddedPayloadDriver)
	{
		EfiPrint(L"Embedded driver started; connecting controllers\n");
		ConnectAll(ImageHandle);
		return;
	}

	EfiPrint(L"Embedded EFI application returned\n");
}
