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

#define DEVICE_PATH_MAX_NODES 1024
#define DEVICE_PATH_MAX_SCAN_SIZE (1024ULL * 1024ULL)
#define PXE_MAX_IMAGE_SIZE (256ULL * 1024ULL * 1024ULL)

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

static EFI_GUID gEfiPxeBaseCodeProtocolGuid =
{
	0x03C4E603,
	0xAC28,
	0x11D3,
	{ 0x9A, 0x2D, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D }
};

static inline VOID PrintEfiCallStatus(const CHAR16* Name, EFI_STATUS Status)
{
	SerialPrint(L"%s status=0x%016llX\n", Name, (UINT64)Status);
}

static inline UINTN DevicePathNodeLength(EFI_DEVICE_PATH_PROTOCOL* Node)
{
	return ((UINTN)Node->Length[0]) | (((UINTN)Node->Length[1]) << 8);
}

static inline VOID SetDevicePathNodeLength(EFI_DEVICE_PATH_PROTOCOL* Node, UINTN Length)
{
	Node->Length[0] = (UINT8)(Length & 0xFF);
	Node->Length[1] = (UINT8)((Length >> 8) & 0xFF);
}

static inline EFI_DEVICE_PATH_PROTOCOL* NextDevicePathNode(EFI_DEVICE_PATH_PROTOCOL* Node)
{
	return (EFI_DEVICE_PATH_PROTOCOL*)((UINT8*)Node + DevicePathNodeLength(Node));
}

static inline BOOLEAN IsDevicePathEnd(EFI_DEVICE_PATH_PROTOCOL* Node)
{
	return (((Node->Type & EFI_DP_TYPE_MASK) == END_DEVICE_PATH_TYPE) &&
		(Node->SubType == END_ENTIRE_DEVICE_PATH_SUBTYPE));
}

static inline VOID SetDevicePathEndNode(EFI_DEVICE_PATH_PROTOCOL* Node)
{
	Node->Type = END_DEVICE_PATH_TYPE;
	Node->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	Node->Length[0] = (UINT8)END_DEVICE_PATH_LENGTH;
	Node->Length[1] = 0;
}

static EFI_STATUS LoaderAllocatePool(UINTN Size, VOID** Buffer)
{
	EFI_STATUS Status;

	if (Buffer == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*Buffer = NULL;
	if (Size == 0)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	Status = gContext.BootServices->AllocatePool(EfiLoaderData, Size, Buffer);
	if (!EFI_ERROR(Status) && (*Buffer == NULL))
	{
		return EFI_STATUS_OUT_OF_RESOURCES;
	}

	return Status;
}

static EFI_STATUS LoaderAllocateZeroPool(UINTN Size, VOID** Buffer)
{
	EFI_STATUS Status;

	Status = LoaderAllocatePool(Size, Buffer);
	if (!EFI_ERROR(Status))
	{
		memset(*Buffer, 0, Size);
	}

	return Status;
}

static VOID LoaderFreePool(VOID* Buffer)
{
	if (Buffer != NULL)
	{
		gContext.BootServices->FreePool(Buffer);
	}
}

static EFI_STATUS GetDevicePathSize(EFI_DEVICE_PATH_PROTOCOL* DevicePath, UINTN* DevicePathSize)
{
	EFI_DEVICE_PATH_PROTOCOL* Node;
	UINTN NodeLength;
	UINTN NodeCount;
	UINTN TotalSize;

	if ((DevicePath == NULL) || (DevicePathSize == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	Node = DevicePath;
	NodeCount = 0;
	TotalSize = 0;
	for (;;)
	{
		NodeLength = DevicePathNodeLength(Node);
		if (NodeLength < END_DEVICE_PATH_LENGTH)
		{
			return EFI_STATUS_INVALID_PARAMETER;
		}

		if (NodeLength > (((UINTN)-1) - TotalSize))
		{
			return EFI_STATUS_BUFFER_TOO_SMALL;
		}

		TotalSize += NodeLength;
		if (TotalSize > DEVICE_PATH_MAX_SCAN_SIZE)
		{
			return EFI_STATUS_BUFFER_TOO_SMALL;
		}

		if (IsDevicePathEnd(Node))
		{
			*DevicePathSize = TotalSize;
			return EFI_SUCCESS;
		}

		NodeCount++;
		if (NodeCount > DEVICE_PATH_MAX_NODES)
		{
			return EFI_STATUS_INVALID_PARAMETER;
		}

		Node = NextDevicePathNode(Node);
	}
}

static EFI_STATUS DuplicateDevicePath(
	EFI_DEVICE_PATH_PROTOCOL* Source,
	EFI_DEVICE_PATH_PROTOCOL** Destination)
{
	UINTN SourceSize;
	EFI_STATUS Status;

	if (Destination == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*Destination = NULL;
	Status = GetDevicePathSize(Source, &SourceSize);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = LoaderAllocatePool(SourceSize, (VOID**)Destination);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	memcpy(*Destination, Source, SourceSize);
	return EFI_SUCCESS;
}

static EFI_STATUS AppendDevicePath(
	EFI_DEVICE_PATH_PROTOCOL* First,
	EFI_DEVICE_PATH_PROTOCOL* Second,
	EFI_DEVICE_PATH_PROTOCOL** Destination)
{
	UINTN FirstSize;
	UINTN FirstPayloadSize;
	UINTN SecondSize;
	UINTN DestinationSize;
	EFI_STATUS Status;

	if (Destination == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*Destination = NULL;
	if (First == NULL)
	{
		return DuplicateDevicePath(Second, Destination);
	}

	if (Second == NULL)
	{
		return DuplicateDevicePath(First, Destination);
	}

	Status = GetDevicePathSize(First, &FirstSize);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = GetDevicePathSize(Second, &SecondSize);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	if (FirstSize < END_DEVICE_PATH_LENGTH)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	FirstPayloadSize = FirstSize - END_DEVICE_PATH_LENGTH;
	if (SecondSize > (((UINTN)-1) - FirstPayloadSize))
	{
		return EFI_STATUS_BUFFER_TOO_SMALL;
	}

	DestinationSize = FirstPayloadSize + SecondSize;
	Status = LoaderAllocatePool(DestinationSize, (VOID**)Destination);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	memcpy(*Destination, First, FirstPayloadSize);
	memcpy((UINT8*)*Destination + FirstPayloadSize, Second, SecondSize);

	SerialPrint(L"AppendDevicePath first=0x%016llX second=0x%016llX full=0x%016llX\n",
		(UINT64)FirstSize,
		(UINT64)SecondSize,
		(UINT64)DestinationSize);
	return EFI_SUCCESS;
}

static EFI_STATUS CreateFileDevicePath(
	CHAR16* ImagePath,
	EFI_DEVICE_PATH_PROTOCOL** DevicePath,
	UINTN* DevicePathSize)
{
	EFI_FILEPATH_DEVICE_PATH* FilePath;
	EFI_DEVICE_PATH_PROTOCOL* End;
	CHAR16* Destination;
	UINTN ImageLength;
	UINTN PathCharacters;
	UINTN PathSize;
	UINTN NodeSize;
	UINTN TotalSize;
	UINTN SourceIndex;
	UINTN DestinationIndex;
	CHAR16 Character;
	BOOLEAN NeedsRootPrefix;
	EFI_STATUS Status;

	if ((ImagePath == NULL) || (DevicePath == NULL) || (DevicePathSize == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*DevicePath = NULL;
	*DevicePathSize = 0;
	ImageLength = wcslen(ImagePath);
	if (ImageLength == 0)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	NeedsRootPrefix = ((ImagePath[0] != '\\') && (ImagePath[0] != '/')) ? TRUE : FALSE;
	PathCharacters = ImageLength + (NeedsRootPrefix ? 1 : 0) + 1;
	if (PathCharacters <= ImageLength)
	{
		return EFI_STATUS_BUFFER_TOO_SMALL;
	}

	PathSize = PathCharacters * sizeof(CHAR16);
	NodeSize = SIZE_OF_FILEPATH_DEVICE_PATH + PathSize;
	TotalSize = NodeSize + END_DEVICE_PATH_LENGTH;
	if ((PathSize / sizeof(CHAR16)) != PathCharacters)
	{
		return EFI_STATUS_BUFFER_TOO_SMALL;
	}

	if ((NodeSize > 0xFFFF) || (TotalSize < NodeSize))
	{
		return EFI_STATUS_BUFFER_TOO_SMALL;
	}

	Status = LoaderAllocateZeroPool(TotalSize, (VOID**)&FilePath);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	FilePath->Header.Type = MEDIA_DEVICE_PATH;
	FilePath->Header.SubType = MEDIA_FILEPATH_DP;
	SetDevicePathNodeLength(&FilePath->Header, NodeSize);

	Destination = FilePath->PathName;
	DestinationIndex = 0;
	if (NeedsRootPrefix)
	{
		Destination[DestinationIndex] = '\\';
		DestinationIndex++;
	}

	for (SourceIndex = 0; SourceIndex < ImageLength; SourceIndex++)
	{
		Character = ImagePath[SourceIndex];
		if (Character == '/')
		{
			Character = '\\';
		}

		Destination[DestinationIndex] = Character;
		DestinationIndex++;
	}

	Destination[DestinationIndex] = 0;

	End = NextDevicePathNode(&FilePath->Header);
	SetDevicePathEndNode(End);

	*DevicePath = (EFI_DEVICE_PATH_PROTOCOL*)FilePath;
	*DevicePathSize = TotalSize;
	SerialPrint(L"FileDevicePath path=%s size=0x%016llX\n",
		Destination,
		(UINT64)TotalSize);
	return EFI_SUCCESS;
}

NTSTATUS EfiInitializeContext(BL_FIRMWARE_DESCRIPTOR* Firmware)
{
	memset(&gContext, 0, sizeof(gContext));

	if ((Firmware == NULL) || (Firmware->SystemTable == NULL))
	{
		return STATUS_INVALID_PARAMETER;
	}

	gContext.Firmware = Firmware;
	gContext.SystemTable = Firmware->SystemTable;
	gContext.BootServices = Firmware->SystemTable->BootServices;
	gContext.ConOut = Firmware->SystemTable->ConOut;
	gContext.ParentHandle = Firmware->ImageHandle;
	return STATUS_SUCCESS;
}

static EFI_STATUS HandleProtocol(
	EFI_HANDLE Handle,
	EFI_GUID* Protocol,
	const CHAR16* TraceName,
	VOID** Interface)
{
	EFI_STATUS Status;

	if ((Handle == NULL) || (Protocol == NULL) || (Interface == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*Interface = NULL;
	Status = gContext.BootServices->HandleProtocol(Handle, Protocol, Interface);
	PrintEfiCallStatus(TraceName, Status);
	SerialPrint(L"%s interface=%p\n", TraceName, *Interface);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	if (*Interface == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	return EFI_SUCCESS;
}

static EFI_STATUS GetParentLoadedImage(EFI_LOADED_IMAGE_PROTOCOL** LoadedImage)
{
	return HandleProtocol(
		gContext.ParentHandle,
		&gEfiLoadedImageProtocolGuid,
		L"HandleProtocol(LoadedImage)",
		(VOID**)LoadedImage);
}

static EFI_STATUS GetParentDevicePath(EFI_DEVICE_PATH_PROTOCOL** DevicePath)
{
	EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
	EFI_STATUS Status;

	if (DevicePath == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*DevicePath = NULL;
	LoadedImage = NULL;
	Status = GetParentLoadedImage(&LoadedImage);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	if (LoadedImage->DeviceHandle == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	Status = HandleProtocol(
		LoadedImage->DeviceHandle,
		&gEfiDevicePathProtocolGuid,
		L"HandleProtocol(DevicePath)",
		(VOID**)DevicePath);
	SerialPrint(L"DeviceHandle=%p DevicePath=%p\n",
		LoadedImage->DeviceHandle,
		(VOID*)*DevicePath);
	return Status;
}

static EFI_STATUS BuildFullImageDevicePath(
	EFI_DEVICE_PATH_PROTOCOL* RelativeDevicePath,
	EFI_DEVICE_PATH_PROTOCOL** FullDevicePath)
{
	EFI_DEVICE_PATH_PROTOCOL* ParentDevicePath;
	UINTN ParentDevicePathSize;
	UINTN RelativeDevicePathSize;
	UINTN FullDevicePathSize;
	EFI_STATUS Status;

	if ((RelativeDevicePath == NULL) || (FullDevicePath == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*FullDevicePath = NULL;
	ParentDevicePath = NULL;
	Status = GetParentDevicePath(&ParentDevicePath);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = GetDevicePathSize(ParentDevicePath, &ParentDevicePathSize);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = GetDevicePathSize(RelativeDevicePath, &RelativeDevicePathSize);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = AppendDevicePath(ParentDevicePath, RelativeDevicePath, FullDevicePath);
	PrintEfiCallStatus(L"AppendDevicePath(parent,file)", Status);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = GetDevicePathSize(*FullDevicePath, &FullDevicePathSize);
	if (EFI_ERROR(Status))
	{
		LoaderFreePool(*FullDevicePath);
		*FullDevicePath = NULL;
		return Status;
	}

	SerialPrint(L"DevicePath sizes parent=0x%016llX file=0x%016llX full=0x%016llX\n",
		(UINT64)ParentDevicePathSize,
		(UINT64)RelativeDevicePathSize,
		(UINT64)FullDevicePathSize);
	return EFI_SUCCESS;
}

static EFI_STATUS BuildPxeFileName(CHAR16* ImagePath, UINT8* FileName, UINTN FileNameSize)
{
	UINTN SourceIndex;
	UINTN DestinationIndex;
	UINT8 Character;
	CHAR16 WideCharacter;

	if ((ImagePath == NULL) || (FileName == NULL) || (FileNameSize == 0))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	SourceIndex = 0;
	while ((ImagePath[SourceIndex] == '\\') || (ImagePath[SourceIndex] == '/'))
	{
		SourceIndex++;
	}

	DestinationIndex = 0;
	while (ImagePath[SourceIndex] != 0)
	{
		if ((DestinationIndex + 1) >= FileNameSize)
		{
			return EFI_STATUS_BUFFER_TOO_SMALL;
		}

		WideCharacter = ImagePath[SourceIndex];
		if (WideCharacter > 0x7F)
		{
			return EFI_STATUS_INVALID_PARAMETER;
		}

		Character = (UINT8)WideCharacter;
		if (Character == '\\')
		{
			Character = '/';
		}

		FileName[DestinationIndex] = Character;
		DestinationIndex++;
		SourceIndex++;
	}

	if (DestinationIndex == 0)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	FileName[DestinationIndex] = 0;
	return EFI_SUCCESS;
}

static EFI_STATUS GetPxeServerIp(EFI_PXE_BASE_CODE_PACKET* Packet, EFI_IP_ADDRESS* ServerIp)
{
	UINT8 ZeroAddress[4] = { 0 };
	UINT64 Address;

	if ((Packet == NULL) || (ServerIp == NULL) ||
		(memcmp(Packet->Dhcpv4.BootpSiAddr, ZeroAddress, sizeof(ZeroAddress)) == 0))
	{
		return EFI_STATUS_NOT_FOUND;
	}

	memset(ServerIp, 0, sizeof(*ServerIp));
	memcpy(ServerIp->v4.Addr, Packet->Dhcpv4.BootpSiAddr, sizeof(ServerIp->v4.Addr));

	Address =
		((UINT64)ServerIp->v4.Addr[0] << 24) |
		((UINT64)ServerIp->v4.Addr[1] << 16) |
		((UINT64)ServerIp->v4.Addr[2] << 8) |
		((UINT64)ServerIp->v4.Addr[3]);
	SerialPrint(L"PXE server IPv4=0x%08X\n", Address);
	return EFI_SUCCESS;
}

static EFI_STATUS GetParentPxeBaseCode(EFI_PXE_BASE_CODE_PROTOCOL** PxeBaseCode)
{
	EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
	EFI_STATUS Status;

	if (PxeBaseCode == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*PxeBaseCode = NULL;
	LoadedImage = NULL;
	Status = GetParentLoadedImage(&LoadedImage);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	if (LoadedImage->DeviceHandle == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	Status = HandleProtocol(
		LoadedImage->DeviceHandle,
		&gEfiPxeBaseCodeProtocolGuid,
		L"HandleProtocol(PxeBaseCode)",
		(VOID**)PxeBaseCode);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	if ((*PxeBaseCode)->Mode == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	SerialPrint(L"PXE revision=0x%016llX mode=%p started=0x%X ipv6=0x%X dhcp=0x%X proxy=0x%X pxe=0x%X\n",
		(*PxeBaseCode)->Revision,
		(VOID*)(*PxeBaseCode)->Mode,
		(UINT64)(*PxeBaseCode)->Mode->Started,
		(UINT64)(*PxeBaseCode)->Mode->UsingIpv6,
		(UINT64)(*PxeBaseCode)->Mode->DhcpAckReceived,
		(UINT64)(*PxeBaseCode)->Mode->ProxyOfferReceived,
		(UINT64)(*PxeBaseCode)->Mode->PxeReplyReceived);
	return EFI_SUCCESS;
}

static EFI_STATUS EnsurePxeBaseCodeStarted(EFI_PXE_BASE_CODE_PROTOCOL* PxeBaseCode)
{
	EFI_STATUS Status;

	if ((PxeBaseCode == NULL) || (PxeBaseCode->Mode == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	if (PxeBaseCode->Mode->UsingIpv6)
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	if (!PxeBaseCode->Mode->Started)
	{
		if (PxeBaseCode->Start == NULL)
		{
			return EFI_STATUS_UNSUPPORTED;
		}

		Status = PxeBaseCode->Start(PxeBaseCode, FALSE);
		PrintEfiCallStatus(L"PxeBaseCode.Start", Status);
		if (EFI_ERROR(Status))
		{
			return Status;
		}
	}

	if (PxeBaseCode->Mode->UsingIpv6)
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	if (!PxeBaseCode->Mode->DhcpAckReceived &&
		!PxeBaseCode->Mode->ProxyOfferReceived &&
		!PxeBaseCode->Mode->PxeReplyReceived)
	{
		if (PxeBaseCode->Dhcp == NULL)
		{
			return EFI_STATUS_NOT_FOUND;
		}

		Status = PxeBaseCode->Dhcp(PxeBaseCode, FALSE);
		PrintEfiCallStatus(L"PxeBaseCode.Dhcp", Status);
		if (EFI_ERROR(Status))
		{
			return Status;
		}
	}

	return EFI_SUCCESS;
}

static EFI_STATUS DownloadPxeImage(
	EFI_PXE_BASE_CODE_PROTOCOL* PxeBaseCode,
	EFI_IP_ADDRESS* ServerIp,
	UINT8* FileName,
	VOID** ImageBuffer,
	UINTN* ImageSize)
{
	VOID* Buffer;
	UINT64 BufferSize;
	EFI_STATUS Status;

	if ((PxeBaseCode == NULL) || (ServerIp == NULL) ||
		(FileName == NULL) || (ImageBuffer == NULL) || (ImageSize == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*ImageBuffer = NULL;
	*ImageSize = 0;
	if (PxeBaseCode->Mtftp == NULL)
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	BufferSize = 0;
	Status = PxeBaseCode->Mtftp(
		PxeBaseCode,
		EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE,
		NULL,
		FALSE,
		&BufferSize,
		NULL,
		ServerIp,
		FileName,
		NULL,
		FALSE);
	PrintEfiCallStatus(L"PxeBaseCode.Mtftp(GetFileSize)", Status);
	SerialPrint(L"PXE image size=0x%016llX\n", BufferSize);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	if ((BufferSize == 0) || (BufferSize > PXE_MAX_IMAGE_SIZE) || (BufferSize > (UINT64)((UINTN)-1)))
	{
		return EFI_STATUS_BUFFER_TOO_SMALL;
	}

	Buffer = NULL;
	Status = LoaderAllocatePool((UINTN)BufferSize, &Buffer);
	PrintEfiCallStatus(L"AllocatePool(PXE image)", Status);
	SerialPrint(L"PXE image buffer=%p\n", Buffer);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = PxeBaseCode->Mtftp(
		PxeBaseCode,
		EFI_PXE_BASE_CODE_TFTP_READ_FILE,
		Buffer,
		FALSE,
		&BufferSize,
		NULL,
		ServerIp,
		FileName,
		NULL,
		FALSE);
	PrintEfiCallStatus(L"PxeBaseCode.Mtftp(ReadFile)", Status);
	SerialPrint(L"PXE bytes read=0x%016llX\n", BufferSize);
	if (EFI_ERROR(Status))
	{
		LoaderFreePool(Buffer);
		return Status;
	}

	*ImageBuffer = Buffer;
	*ImageSize = (UINTN)BufferSize;
	return EFI_SUCCESS;
}

static EFI_STATUS LoadPxeImageFromPacket(
	EFI_DEVICE_PATH_PROTOCOL* DevicePath,
	EFI_PXE_BASE_CODE_PROTOCOL* PxeBaseCode,
	const CHAR16* PacketName,
	BOOLEAN PacketValid,
	EFI_PXE_BASE_CODE_PACKET* Packet,
	UINT8* FileName,
	EFI_HANDLE* ImageHandle)
{
	EFI_IP_ADDRESS ServerIp;
	VOID* ImageBuffer;
	UINTN ImageSize;
	EFI_STATUS Status;

	if ((DevicePath == NULL) || (PxeBaseCode == NULL) ||
		(Packet == NULL) || (FileName == NULL) || (ImageHandle == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	if (!PacketValid)
	{
		SerialPrint(L"PXE packet %s not valid\n", PacketName);
		return EFI_STATUS_NOT_FOUND;
	}

	Status = GetPxeServerIp(Packet, &ServerIp);
	PrintEfiCallStatus(L"GetPxeServerIp", Status);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	ImageBuffer = NULL;
	ImageSize = 0;
	Status = DownloadPxeImage(PxeBaseCode, &ServerIp, FileName, &ImageBuffer, &ImageSize);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	*ImageHandle = NULL;
	Status = gContext.BootServices->LoadImage(
		FALSE,
		gContext.ParentHandle,
		DevicePath,
		ImageBuffer,
		ImageSize,
		ImageHandle);
	PrintEfiCallStatus(L"LoadImage(PXE buffer)", Status);
	SerialPrint(L"ImageHandle=%p\n", *ImageHandle);
	LoaderFreePool(ImageBuffer);
	return Status;
}

static EFI_STATUS LoadEfiImageFromPxe(
	EFI_DEVICE_PATH_PROTOCOL* DevicePath,
	CHAR16* ImagePath,
	EFI_HANDLE* ImageHandle)
{
	EFI_PXE_BASE_CODE_PROTOCOL* PxeBaseCode;
	UINT8 PxeFileName[MAX_PATH];
	EFI_STATUS Status;

	if ((DevicePath == NULL) || (ImagePath == NULL) || (ImageHandle == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*ImageHandle = NULL;
	memset(PxeFileName, 0, sizeof(PxeFileName));

	Status = BuildPxeFileName(ImagePath, PxeFileName, sizeof(PxeFileName));
	PrintEfiCallStatus(L"BuildPxeFileName", Status);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	PxeBaseCode = NULL;
	Status = GetParentPxeBaseCode(&PxeBaseCode);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = EnsurePxeBaseCodeStarted(PxeBaseCode);
	PrintEfiCallStatus(L"EnsurePxeBaseCodeStarted", Status);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = LoadPxeImageFromPacket(
		DevicePath,
		PxeBaseCode,
		L"PxeReply",
		PxeBaseCode->Mode->PxeReplyReceived,
		&PxeBaseCode->Mode->PxeReply,
		PxeFileName,
		ImageHandle);
	if (!EFI_ERROR(Status))
	{
		return Status;
	}

	Status = LoadPxeImageFromPacket(
		DevicePath,
		PxeBaseCode,
		L"ProxyOffer",
		PxeBaseCode->Mode->ProxyOfferReceived,
		&PxeBaseCode->Mode->ProxyOffer,
		PxeFileName,
		ImageHandle);
	if (!EFI_ERROR(Status))
	{
		return Status;
	}

	return LoadPxeImageFromPacket(
		DevicePath,
		PxeBaseCode,
		L"DhcpAck",
		PxeBaseCode->Mode->DhcpAckReceived,
		&PxeBaseCode->Mode->DhcpAck,
		PxeFileName,
		ImageHandle);
}

static EFI_STATUS LoadEfiImageFromDevicePath(
	EFI_DEVICE_PATH_PROTOCOL* DevicePath,
	const CHAR16* TraceName,
	EFI_HANDLE* ImageHandle)
{
	EFI_STATUS Status;

	if ((DevicePath == NULL) || (ImageHandle == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*ImageHandle = NULL;
	Status = gContext.BootServices->LoadImage(FALSE, gContext.ParentHandle, DevicePath, NULL, 0, ImageHandle);
	PrintEfiCallStatus(TraceName, Status);
	SerialPrint(L"ImageHandle=%p\n", *ImageHandle);
	return Status;
}

static EFI_STATUS StartEfiImage(EFI_HANDLE ImageHandle)
{
	CHAR16* ExitData;
	UINTN ExitDataSize;
	EFI_STATUS Status;

	if (ImageHandle == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	ExitDataSize = 0;
	ExitData = NULL;
	Status = gContext.BootServices->StartImage(ImageHandle, &ExitDataSize, &ExitData);
	PrintEfiCallStatus(L"StartImage", Status);
	SerialPrint(L"ExitDataSize=0x%016llX ExitData=%p\n", (UINT64)ExitDataSize, (VOID*)ExitData);
	return Status;
}

static EFI_STATUS LoadTargetImage(
	CHAR16* ImagePath,
	EFI_HANDLE* ImageHandle,
	EFI_DEVICE_PATH_PROTOCOL** RelativeDevicePath,
	EFI_DEVICE_PATH_PROTOCOL** FullDevicePath)
{
	EFI_DEVICE_PATH_PROTOCOL* PxeDevicePath;
	UINTN RelativeDevicePathSize;
	EFI_STATUS Status;

	if ((ImagePath == NULL) || (ImageHandle == NULL) ||
		(RelativeDevicePath == NULL) || (FullDevicePath == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*ImageHandle = NULL;
	*RelativeDevicePath = NULL;
	*FullDevicePath = NULL;
	RelativeDevicePathSize = 0;

	Status = CreateFileDevicePath(ImagePath, RelativeDevicePath, &RelativeDevicePathSize);
	PrintEfiCallStatus(L"FileDevicePath", Status);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = LoadEfiImageFromDevicePath(
		*RelativeDevicePath,
		L"LoadImage(relative)",
		ImageHandle);
	if (!EFI_ERROR(Status))
	{
		return Status;
	}

	Status = BuildFullImageDevicePath(*RelativeDevicePath, FullDevicePath);
	PrintEfiCallStatus(L"BuildFullImageDevicePath", Status);
	if (!EFI_ERROR(Status))
	{
		Status = LoadEfiImageFromDevicePath(
			*FullDevicePath,
			L"LoadImage(full)",
			ImageHandle);
		if (!EFI_ERROR(Status))
		{
			return Status;
		}
	}

	PxeDevicePath = (*FullDevicePath != NULL) ? *FullDevicePath : *RelativeDevicePath;
	Status = LoadEfiImageFromPxe(PxeDevicePath, ImagePath, ImageHandle);
	PrintEfiCallStatus(L"LoadEfiImageFromPxe", Status);
	return Status;
}

static NTSTATUS EfiStatusToNtStatus(EFI_STATUS Status)
{
	if (!EFI_ERROR(Status))
	{
		return STATUS_SUCCESS;
	}

	switch (Status)
	{
	case EFI_STATUS_INVALID_PARAMETER:
		return STATUS_INVALID_PARAMETER;

	case EFI_STATUS_UNSUPPORTED:
		return STATUS_NOT_SUPPORTED;

	case EFI_STATUS_BUFFER_TOO_SMALL:
		return STATUS_BUFFER_TOO_SMALL;

	case EFI_STATUS_NOT_FOUND:
		return STATUS_NOT_FOUND;

	default:
		return STATUS_UNSUCCESSFUL;
	}
}

NTSTATUS EfiStartApplication(CHAR16* ImagePath)
{
	EFI_DEVICE_PATH_PROTOCOL* RelativeDevicePath;
	EFI_DEVICE_PATH_PROTOCOL* FullDevicePath;
	EFI_HANDLE ImageHandle;
	EFI_STATUS Status;
	NTSTATUS NtStatus;

	SerialPrint(L"BS=%p LoadImage=%p StartImage=%p\n",
		(VOID*)gContext.BootServices,
		(VOID*)(UINTN)gContext.BootServices->LoadImage,
		(VOID*)(UINTN)gContext.BootServices->StartImage);

	Status = gContext.BootServices->SetWatchdogTimer(0, 0, 0, NULL);
	PrintEfiCallStatus(L"SetWatchdogTimer", Status);

	EfiPrint(L"LoadImage %s\n", ImagePath);
	SerialPrint(L"Target image path=%s\n", ImagePath);

	RelativeDevicePath = NULL;
	FullDevicePath = NULL;
	ImageHandle = NULL;
	Status = LoadTargetImage(
		ImagePath,
		&ImageHandle,
		&RelativeDevicePath,
		&FullDevicePath);
	if (EFI_ERROR(Status))
	{
		EfiPrint(L"LoadImage failed\n");
		NtStatus = EfiStatusToNtStatus(Status);
		goto cleanup;
	}

	EfiPrint(L"LoadImage OK, starting image\n");
	Status = StartEfiImage(ImageHandle);
	if (EFI_ERROR(Status))
	{
		EfiPrint(L"StartImage failed\n");
		NtStatus = STATUS_UNSUCCESSFUL;
		goto cleanup;
	}

	EfiPrint(L"StartImage returned\n");
	NtStatus = STATUS_SUCCESS;

cleanup:
	LoaderFreePool(FullDevicePath);
	LoaderFreePool(RelativeDevicePath);
	return NtStatus;
}
