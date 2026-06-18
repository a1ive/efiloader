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

#include "loader.h"

#include "compiler.h"
#include "print.h"

#define DEVICE_PATH_NODE_LENGTH(CharCount) (sizeof(EFI_DEVICE_PATH_PROTOCOL) + ((CharCount) * sizeof(CHAR16)))
#define DEVICE_PATH_LENGTH_LOW(Length) ((UINT8)((Length) & 0xFF))
#define DEVICE_PATH_LENGTH_HIGH(Length) ((UINT8)(((Length) >> 8) & 0xFF))

static EFI_GUID gEfiLoadedImageProtocolGuid =
{
	0x5B1B31A1,
	0x9562,
	0x11D2,
	{ 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

static EFI_GUID gEfiDevicePathProtocolGuid =
{
	0x09576E91,
	0x6D3F,
	0x11D2,
	{ 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

static UINT8 gRelativeImageDevicePath[
	sizeof(EFI_DEVICE_PATH_PROTOCOL) +
	((MAX_PATH + 1) * sizeof(CHAR16)) +
	sizeof(EFI_DEVICE_PATH_PROTOCOL)];
static UINT8 gFullImageDevicePath[1024];

static inline VOID PrintEfiCallStatus(CHAR16* Name, EFI_STATUS Status)
{
	SerialPrint(L"%s status=0x%016llX\n", Name, (UINT64)Status);
}

static EFI_STATUS BuildFilePathDevicePath(
	CHAR16* ImagePath,
	EFI_DEVICE_PATH_PROTOCOL** DevicePath,
	UINTN* DevicePathSize)
{
	EFI_DEVICE_PATH_PROTOCOL* Header;
	EFI_DEVICE_PATH_PROTOCOL* End;
	CHAR16* PathName;
	UINTN PathLength;
	UINTN PrefixLength;
	UINTN NodeCharacters;
	UINTN NodeLength;
	UINTN Index;

	*DevicePath = NULL;
	*DevicePathSize = 0;
	PathLength = 0;
	while (ImagePath[PathLength] != 0)
	{
		PathLength++;
	}

	if (PathLength == 0)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	PrefixLength = (ImagePath[0] == '\\') ? 0 : 1;
	NodeCharacters = PrefixLength + PathLength + 1;
	if (NodeCharacters > MAX_PATH)
	{
		return EFI_STATUS_BUFFER_TOO_SMALL;
	}

	NodeLength = DEVICE_PATH_NODE_LENGTH(NodeCharacters);
	Header = (EFI_DEVICE_PATH_PROTOCOL*)gRelativeImageDevicePath;
	PathName = (CHAR16*)(gRelativeImageDevicePath + sizeof(EFI_DEVICE_PATH_PROTOCOL));
	End = (EFI_DEVICE_PATH_PROTOCOL*)(gRelativeImageDevicePath + NodeLength);

	Header->Type = MEDIA_DEVICE_PATH;
	Header->SubType = MEDIA_FILEPATH_DP;
	Header->Length[0] = DEVICE_PATH_LENGTH_LOW(NodeLength);
	Header->Length[1] = DEVICE_PATH_LENGTH_HIGH(NodeLength);

	Index = 0;
	if (PrefixLength != 0)
	{
		PathName[Index] = '\\';
		Index++;
	}

	while (ImagePath[Index - PrefixLength] != 0)
	{
		PathName[Index] = ImagePath[Index - PrefixLength];
		Index++;
	}

	PathName[Index] = 0;

	End->Type = END_DEVICE_PATH_TYPE;
	End->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	End->Length[0] = DEVICE_PATH_LENGTH_LOW(sizeof(EFI_DEVICE_PATH_PROTOCOL));
	End->Length[1] = DEVICE_PATH_LENGTH_HIGH(sizeof(EFI_DEVICE_PATH_PROTOCOL));

	*DevicePath = Header;
	*DevicePathSize = NodeLength + sizeof(EFI_DEVICE_PATH_PROTOCOL);
	return EFI_SUCCESS;
}

static UINTN GetDevicePathSize(EFI_DEVICE_PATH_PROTOCOL* DevicePath)
{
	EFI_DEVICE_PATH_PROTOCOL* DevicePathCursor;
	UINTN DevicePathSize;
	UINTN DevicePathNodeSize;

	DevicePathSize = 0;
	DevicePathCursor = DevicePath;
	while ((DevicePathCursor->Type != END_DEVICE_PATH_TYPE) ||
		(DevicePathCursor->SubType != END_ENTIRE_DEVICE_PATH_SUBTYPE))
	{
		DevicePathNodeSize = ((UINTN)DevicePathCursor->Length[0]) | (((UINTN)DevicePathCursor->Length[1]) << 8);
		if (DevicePathNodeSize < sizeof(EFI_DEVICE_PATH_PROTOCOL))
		{
			return 0;
		}

		DevicePathSize += DevicePathNodeSize;
		DevicePathCursor = (EFI_DEVICE_PATH_PROTOCOL*)((UINT8*)DevicePathCursor + DevicePathNodeSize);
	}

	return DevicePathSize;
}

static EFI_STATUS BuildFullImageDevicePath(
	EFI_BOOT_SERVICES* BootServices,
	EFI_HANDLE ParentHandle,
	EFI_DEVICE_PATH_PROTOCOL* RelativeDevicePath,
	UINTN RelativeDevicePathSize,
	EFI_DEVICE_PATH_PROTOCOL** FullDevicePath)
{
	EFI_LOADED_IMAGE_PROTOCOL* LoadedImage = NULL;
	EFI_DEVICE_PATH_PROTOCOL* BootDevicePath = NULL;
	UINTN BootDevicePathSize;
	UINTN FullSize;
	EFI_STATUS EfiStatus;

	*FullDevicePath = NULL;

	EfiStatus = BootServices->HandleProtocol(ParentHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&LoadedImage);
	PrintEfiCallStatus(L"HandleProtocol(LoadedImage)", EfiStatus);
	SerialPrint(L"LoadedImage=%p\n", (VOID*)LoadedImage);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	if ((LoadedImage == NULL) || (LoadedImage->DeviceHandle == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	EfiStatus = BootServices->HandleProtocol(LoadedImage->DeviceHandle, &gEfiDevicePathProtocolGuid, (VOID**)&BootDevicePath);
	PrintEfiCallStatus(L"HandleProtocol(DevicePath)", EfiStatus);
	SerialPrint(L"DeviceHandle=%p DevicePath=%p\n", LoadedImage->DeviceHandle, (VOID*)BootDevicePath);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	if (BootDevicePath == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	BootDevicePathSize = GetDevicePathSize(BootDevicePath);
	FullSize = BootDevicePathSize + RelativeDevicePathSize;
	SerialPrint(L"DevicePath sizes base=0x%016llX file=0x%016llX full=0x%016llX\n",
		(UINT64)BootDevicePathSize,
		(UINT64)RelativeDevicePathSize,
		(UINT64)FullSize);

	if ((BootDevicePathSize == 0) || (FullSize > sizeof(gFullImageDevicePath)))
	{
		return EFI_STATUS_BUFFER_TOO_SMALL;
	}

	memcpy(gFullImageDevicePath, BootDevicePath, BootDevicePathSize);
	memcpy(gFullImageDevicePath + BootDevicePathSize, RelativeDevicePath, RelativeDevicePathSize);
	*FullDevicePath = (EFI_DEVICE_PATH_PROTOCOL*)gFullImageDevicePath;
	return EFI_SUCCESS;
}

static inline EFI_STATUS
LoadEfiImage(EFI_BOOT_SERVICES* BootServices, EFI_HANDLE ParentHandle, EFI_DEVICE_PATH_PROTOCOL* DevicePath, EFI_HANDLE* ImageHandle)
{
	EFI_STATUS EfiStatus;

	*ImageHandle = NULL;
	EfiStatus = BootServices->LoadImage(FALSE, ParentHandle, DevicePath, NULL, 0, ImageHandle);
	PrintEfiCallStatus(L"LoadImage", EfiStatus);
	SerialPrint(L"ImageHandle=%p\n", *ImageHandle);
	return EfiStatus;
}

static inline EFI_STATUS
StartEfiImage(EFI_BOOT_SERVICES* BootServices, EFI_HANDLE ImageHandle)
{
	CHAR16* ExitData;
	UINTN ExitDataSize;
	EFI_STATUS EfiStatus;

	ExitDataSize = 0;
	ExitData = NULL;
	EfiStatus = BootServices->StartImage(ImageHandle, &ExitDataSize, &ExitData);
	PrintEfiCallStatus(L"StartImage", EfiStatus);
	SerialPrint(L"ExitDataSize=0x%016llX ExitData=%p\n", (UINT64)ExitDataSize, (VOID*)ExitData);
	return EfiStatus;
}

NTSTATUS EfiStartEfiApplication(BL_FIRMWARE_DESCRIPTOR_X64* Firmware, CHAR16* ImagePath)
{
	EFI_SYSTEM_TABLE* SystemTable = Firmware->SystemTable;
	EFI_BOOT_SERVICES* BootServices = SystemTable->BootServices;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* ConOut = SystemTable->ConOut;
	EFI_HANDLE ParentHandle = Firmware->ImageHandle;
	EFI_DEVICE_PATH_PROTOCOL* RelativeDevicePath;
	EFI_DEVICE_PATH_PROTOCOL* FullDevicePath;
	EFI_HANDLE ImageHandle;
	UINTN RelativeDevicePathSize;
	EFI_STATUS EfiStatus;
	BOOLEAN TryFullDevicePath;

	SerialPrint(L"BS=%p LoadImage=%p StartImage=%p\n",
		(VOID*)BootServices,
		(VOID*)(UINTN)BootServices->LoadImage,
		(VOID*)(UINTN)BootServices->StartImage);

	if (BootServices->SetWatchdogTimer != NULL)
	{
		EfiStatus = BootServices->SetWatchdogTimer(0, 0, 0, NULL);
		PrintEfiCallStatus(L"SetWatchdogTimer", EfiStatus);
	}

	EfiPrint(ConOut, L"LoadImage %s\n", ImagePath);
	SerialPrint(L"Target image path=%s\n", ImagePath);

	TryFullDevicePath = FALSE;
	EfiStatus = BuildFilePathDevicePath(ImagePath, &RelativeDevicePath, &RelativeDevicePathSize);
	PrintEfiCallStatus(L"BuildFilePathDevicePath", EfiStatus);
	if (!EFI_ERROR(EfiStatus))
	{
		EfiStatus = LoadEfiImage(BootServices, ParentHandle, RelativeDevicePath, &ImageHandle);
		if (EFI_ERROR(EfiStatus))
		{
			TryFullDevicePath = TRUE;
		}
	}

	if (TryFullDevicePath)
	{
		EfiStatus = BuildFullImageDevicePath(BootServices, ParentHandle, RelativeDevicePath, RelativeDevicePathSize, &FullDevicePath);
		PrintEfiCallStatus(L"BuildFullImageDevicePath", EfiStatus);
		if (!EFI_ERROR(EfiStatus))
		{
			EfiStatus = LoadEfiImage(BootServices, ParentHandle, FullDevicePath, &ImageHandle);
		}
	}

	if (EFI_ERROR(EfiStatus))
	{
		EfiPrint(ConOut, L"LoadImage failed\n");
		return STATUS_NOT_FOUND;
	}

	EfiPrint(ConOut, L"LoadImage OK, starting image\n");
	EfiStatus = StartEfiImage(BootServices, ImageHandle);

	if (EFI_ERROR(EfiStatus))
	{
		EfiPrint(ConOut, L"StartImage failed\n");
		return STATUS_UNSUCCESSFUL;
	}

	EfiPrint(ConOut, L"StartImage returned\n");
	return STATUS_SUCCESS;
}
