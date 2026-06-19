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

static EFI_GUID gEfiPxeBaseCodeProtocolGuid =
{
	0x03C4E603,
	0xAC28,
	0x11D3,
	{ 0x9A, 0x2D, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D }
};

#define PXE_MAX_IMAGE_SIZE (64ULL * 1024ULL * 1024ULL)

typedef struct _EFI_PXE_BASE_CODE_PROTOCOL EFI_PXE_BASE_CODE_PROTOCOL;

typedef struct _EFI_PXE_BASE_CODE_DHCPV4_PACKET
{
	UINT8 BootpOpcode;
	UINT8 BootpHwType;
	UINT8 BootpHwAddrLen;
	UINT8 BootpGateHops;
	UINT32 BootpIdent;
	UINT16 BootpSeconds;
	UINT16 BootpFlags;
	UINT8 BootpCiAddr[4];
	UINT8 BootpYiAddr[4];
	UINT8 BootpSiAddr[4];
	UINT8 BootpGiAddr[4];
	UINT8 BootpHwAddr[16];
	UINT8 BootpSrvName[64];
	UINT8 BootpBootFile[128];
	UINT32 DhcpMagik;
	UINT8 DhcpOptions[56];
} EFI_PXE_BASE_CODE_DHCPV4_PACKET;

typedef union _EFI_PXE_BASE_CODE_PACKET
{
	UINT8 Raw[1472];
	EFI_PXE_BASE_CODE_DHCPV4_PACKET Dhcpv4;
} EFI_PXE_BASE_CODE_PACKET;

typedef struct _EFI_PXE_BASE_CODE_MODE
{
	BOOLEAN Started;
	BOOLEAN Ipv6Available;
	BOOLEAN Ipv6Supported;
	BOOLEAN UsingIpv6;
	BOOLEAN BisSupported;
	BOOLEAN BisDetected;
	BOOLEAN AutoArp;
	BOOLEAN SendGUID;
	BOOLEAN DhcpDiscoverValid;
	BOOLEAN DhcpAckReceived;
	BOOLEAN ProxyOfferReceived;
	BOOLEAN PxeDiscoverValid;
	BOOLEAN PxeReplyReceived;
	BOOLEAN PxeBisReplyReceived;
	BOOLEAN IcmpErrorReceived;
	BOOLEAN TftpErrorReceived;
	BOOLEAN MakeCallbacks;
	UINT8 TTL;
	UINT8 ToS;
	EFI_IP_ADDRESS StationIp;
	EFI_IP_ADDRESS SubnetMask;
	EFI_PXE_BASE_CODE_PACKET DhcpDiscover;
	EFI_PXE_BASE_CODE_PACKET DhcpAck;
	EFI_PXE_BASE_CODE_PACKET ProxyOffer;
	EFI_PXE_BASE_CODE_PACKET PxeDiscover;
	EFI_PXE_BASE_CODE_PACKET PxeReply;
	EFI_PXE_BASE_CODE_PACKET PxeBisReply;
} EFI_PXE_BASE_CODE_MODE;

typedef enum _EFI_PXE_BASE_CODE_TFTP_OPCODE
{
	EFI_PXE_BASE_CODE_TFTP_FIRST,
	EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE,
	EFI_PXE_BASE_CODE_TFTP_READ_FILE,
	EFI_PXE_BASE_CODE_TFTP_WRITE_FILE,
	EFI_PXE_BASE_CODE_TFTP_READ_DIRECTORY,
	EFI_PXE_BASE_CODE_MTFTP_GET_FILE_SIZE,
	EFI_PXE_BASE_CODE_MTFTP_READ_FILE,
	EFI_PXE_BASE_CODE_MTFTP_READ_DIRECTORY,
	EFI_PXE_BASE_CODE_MTFTP_LAST
} EFI_PXE_BASE_CODE_TFTP_OPCODE;

typedef EFI_STATUS (*EFI_PXE_BASE_CODE_START)(
	EFI_PXE_BASE_CODE_PROTOCOL* This,
	BOOLEAN UseIpv6);

typedef EFI_STATUS (*EFI_PXE_BASE_CODE_DHCP)(
	EFI_PXE_BASE_CODE_PROTOCOL* This,
	BOOLEAN SortOffers);

typedef EFI_STATUS (*EFI_PXE_BASE_CODE_MTFTP)(
	EFI_PXE_BASE_CODE_PROTOCOL* This,
	EFI_PXE_BASE_CODE_TFTP_OPCODE Operation,
	VOID* BufferPtr,
	BOOLEAN Overwrite,
	UINT64* BufferSize,
	UINTN* BlockSize,
	EFI_IP_ADDRESS* ServerIp,
	UINT8* Filename,
	VOID* Info,
	BOOLEAN DontUseBuffer);

struct _EFI_PXE_BASE_CODE_PROTOCOL
{
	UINT64 Revision;
	EFI_PXE_BASE_CODE_START Start;
	VOID* Stop;
	EFI_PXE_BASE_CODE_DHCP Dhcp;
	VOID* Discover;
	EFI_PXE_BASE_CODE_MTFTP Mtftp;
	VOID* UdpWrite;
	VOID* UdpRead;
	VOID* SetIpFilter;
	VOID* Arp;
	VOID* SetParameters;
	VOID* SetStationIp;
	VOID* SetPackets;
	EFI_PXE_BASE_CODE_MODE* Mode;
};

C_ASSERT(sizeof(EFI_PXE_BASE_CODE_DHCPV4_PACKET) == 296);
C_ASSERT(sizeof(EFI_PXE_BASE_CODE_PACKET) == 1472);
C_ASSERT(FIELD_OFFSET(EFI_PXE_BASE_CODE_MODE, DhcpAck) == 1524);

static UINT8 gRelativeImageDevicePath[
	sizeof(EFI_DEVICE_PATH_PROTOCOL) +
	((MAX_PATH + 1) * sizeof(CHAR16)) +
	sizeof(EFI_DEVICE_PATH_PROTOCOL)];
static UINT8 gFullImageDevicePath[1024];
static UINT8 gPxeFileName[MAX_PATH];

static inline VOID PrintEfiCallStatus(CHAR16* Name, EFI_STATUS Status)
{
	SerialPrint(L"%s status=0x%016llX\n", Name, (UINT64)Status);
}

static EFI_STATUS GetParentLoadedImage(
	EFI_BOOT_SERVICES* BootServices,
	EFI_HANDLE ParentHandle,
	EFI_LOADED_IMAGE_PROTOCOL** LoadedImage)
{
	EFI_STATUS EfiStatus;

	*LoadedImage = NULL;
	EfiStatus = BootServices->HandleProtocol(ParentHandle, &gEfiLoadedImageProtocolGuid, (VOID**)LoadedImage);
	PrintEfiCallStatus(L"HandleProtocol(LoadedImage)", EfiStatus);
	SerialPrint(L"LoadedImage=%p\n", (VOID*)*LoadedImage);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	if (*LoadedImage == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	return EFI_SUCCESS;
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

	EfiStatus = GetParentLoadedImage(BootServices, ParentHandle, &LoadedImage);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	if (LoadedImage->DeviceHandle == NULL)
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

static EFI_STATUS BuildPxeFileName(CHAR16* ImagePath, UINT8* FileName, UINTN FileNameSize)
{
	UINTN SourceIndex;
	UINTN DestinationIndex;
	UINT8 Character;

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

		if (ImagePath[SourceIndex] > 0x7F)
		{
			return EFI_STATUS_INVALID_PARAMETER;
		}

		Character = (UINT8)ImagePath[SourceIndex];
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

static BOOLEAN IsZeroIPv4(const UINT8* Address)
{
	return (Address[0] == 0) &&
		(Address[1] == 0) &&
		(Address[2] == 0) &&
		(Address[3] == 0);
}

static EFI_STATUS GetPxeServerIp(EFI_PXE_BASE_CODE_PACKET* Packet, EFI_IP_ADDRESS* ServerIp)
{
	if ((Packet == NULL) || (ServerIp == NULL) || IsZeroIPv4(Packet->Dhcpv4.BootpSiAddr))
	{
		return EFI_STATUS_NOT_FOUND;
	}

	memset(ServerIp, 0, sizeof(*ServerIp));
	memcpy(ServerIp->v4.Addr, Packet->Dhcpv4.BootpSiAddr, sizeof(ServerIp->v4.Addr));
	SerialPrint(L"PXE server IPv4=0x%08X\n",
		((UINT64)ServerIp->v4.Addr[0] << 24) |
		((UINT64)ServerIp->v4.Addr[1] << 16) |
		((UINT64)ServerIp->v4.Addr[2] << 8) |
		((UINT64)ServerIp->v4.Addr[3]));
	return EFI_SUCCESS;
}

static EFI_STATUS GetParentPxeBaseCode(
	EFI_BOOT_SERVICES* BootServices,
	EFI_HANDLE ParentHandle,
	EFI_PXE_BASE_CODE_PROTOCOL** PxeBaseCode)
{
	EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
	EFI_STATUS EfiStatus;

	*PxeBaseCode = NULL;
	EfiStatus = GetParentLoadedImage(BootServices, ParentHandle, &LoadedImage);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	if (LoadedImage->DeviceHandle == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	EfiStatus = BootServices->HandleProtocol(LoadedImage->DeviceHandle, &gEfiPxeBaseCodeProtocolGuid, (VOID**)PxeBaseCode);
	PrintEfiCallStatus(L"HandleProtocol(PxeBaseCode)", EfiStatus);
	SerialPrint(L"PxeBaseCode=%p\n", (VOID*)*PxeBaseCode);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	if ((*PxeBaseCode == NULL) || ((*PxeBaseCode)->Mode == NULL))
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
	EFI_STATUS EfiStatus;

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

		EfiStatus = PxeBaseCode->Start(PxeBaseCode, FALSE);
		PrintEfiCallStatus(L"PxeBaseCode.Start", EfiStatus);
		if (EFI_ERROR(EfiStatus))
		{
			return EfiStatus;
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

		EfiStatus = PxeBaseCode->Dhcp(PxeBaseCode, FALSE);
		PrintEfiCallStatus(L"PxeBaseCode.Dhcp", EfiStatus);
		if (EFI_ERROR(EfiStatus))
		{
			return EfiStatus;
		}
	}

	return EFI_SUCCESS;
}

static EFI_STATUS DownloadPxeImage(
	EFI_BOOT_SERVICES* BootServices,
	EFI_PXE_BASE_CODE_PROTOCOL* PxeBaseCode,
	EFI_IP_ADDRESS* ServerIp,
	UINT8* FileName,
	VOID** ImageBuffer,
	UINTN* ImageSize)
{
	VOID* Buffer;
	UINT64 BufferSize;
	EFI_STATUS EfiStatus;

	*ImageBuffer = NULL;
	*ImageSize = 0;

	if ((BootServices->AllocatePool == NULL) ||
		(BootServices->FreePool == NULL) ||
		(PxeBaseCode->Mtftp == NULL))
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	BufferSize = 0;
	EfiStatus = PxeBaseCode->Mtftp(
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
	PrintEfiCallStatus(L"PxeBaseCode.Mtftp(GetFileSize)", EfiStatus);
	SerialPrint(L"PXE image size=0x%016llX\n", BufferSize);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	if ((BufferSize == 0) || (BufferSize > PXE_MAX_IMAGE_SIZE))
	{
		return EFI_STATUS_BUFFER_TOO_SMALL;
	}

	Buffer = NULL;
	EfiStatus = BootServices->AllocatePool(EfiLoaderData, (UINTN)BufferSize, &Buffer);
	PrintEfiCallStatus(L"AllocatePool(PXE image)", EfiStatus);
	SerialPrint(L"PXE image buffer=%p\n", Buffer);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	EfiStatus = PxeBaseCode->Mtftp(
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
	PrintEfiCallStatus(L"PxeBaseCode.Mtftp(ReadFile)", EfiStatus);
	SerialPrint(L"PXE bytes read=0x%016llX\n", BufferSize);
	if (EFI_ERROR(EfiStatus))
	{
		BootServices->FreePool(Buffer);
		return EfiStatus;
	}

	*ImageBuffer = Buffer;
	*ImageSize = (UINTN)BufferSize;
	return EFI_SUCCESS;
}

static EFI_STATUS LoadPxeImageFromPacket(
	EFI_BOOT_SERVICES* BootServices,
	EFI_HANDLE ParentHandle,
	EFI_DEVICE_PATH_PROTOCOL* DevicePath,
	EFI_PXE_BASE_CODE_PROTOCOL* PxeBaseCode,
	BOOLEAN PacketValid,
	EFI_PXE_BASE_CODE_PACKET* Packet,
	UINT8* FileName,
	EFI_HANDLE* ImageHandle)
{
	EFI_IP_ADDRESS ServerIp;
	VOID* ImageBuffer;
	UINTN ImageSize;
	EFI_STATUS EfiStatus;

	if (!PacketValid)
	{
		return EFI_STATUS_NOT_FOUND;
	}

	EfiStatus = GetPxeServerIp(Packet, &ServerIp);
	PrintEfiCallStatus(L"GetPxeServerIp", EfiStatus);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	EfiStatus = DownloadPxeImage(BootServices, PxeBaseCode, &ServerIp, FileName, &ImageBuffer, &ImageSize);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	*ImageHandle = NULL;
	EfiStatus = BootServices->LoadImage(FALSE, ParentHandle, DevicePath, ImageBuffer, ImageSize, ImageHandle);
	PrintEfiCallStatus(L"LoadImage(PXE buffer)", EfiStatus);
	SerialPrint(L"ImageHandle=%p\n", *ImageHandle);
	BootServices->FreePool(ImageBuffer);
	return EfiStatus;
}

static EFI_STATUS LoadEfiImageFromPxe(
	EFI_BOOT_SERVICES* BootServices,
	EFI_HANDLE ParentHandle,
	EFI_DEVICE_PATH_PROTOCOL* DevicePath,
	CHAR16* ImagePath,
	EFI_HANDLE* ImageHandle)
{
	EFI_PXE_BASE_CODE_PROTOCOL* PxeBaseCode;
	EFI_STATUS EfiStatus;

	*ImageHandle = NULL;

	EfiStatus = BuildPxeFileName(ImagePath, gPxeFileName, sizeof(gPxeFileName));
	PrintEfiCallStatus(L"BuildPxeFileName", EfiStatus);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	EfiStatus = GetParentPxeBaseCode(BootServices, ParentHandle, &PxeBaseCode);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	EfiStatus = EnsurePxeBaseCodeStarted(PxeBaseCode);
	PrintEfiCallStatus(L"EnsurePxeBaseCodeStarted", EfiStatus);
	if (EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	EfiStatus = LoadPxeImageFromPacket(
		BootServices,
		ParentHandle,
		DevicePath,
		PxeBaseCode,
		PxeBaseCode->Mode->PxeReplyReceived,
		&PxeBaseCode->Mode->PxeReply,
		gPxeFileName,
		ImageHandle);
	if (!EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	EfiStatus = LoadPxeImageFromPacket(
		BootServices,
		ParentHandle,
		DevicePath,
		PxeBaseCode,
		PxeBaseCode->Mode->ProxyOfferReceived,
		&PxeBaseCode->Mode->ProxyOffer,
		gPxeFileName,
		ImageHandle);
	if (!EFI_ERROR(EfiStatus))
	{
		return EfiStatus;
	}

	return LoadPxeImageFromPacket(
		BootServices,
		ParentHandle,
		DevicePath,
		PxeBaseCode,
		PxeBaseCode->Mode->DhcpAckReceived,
		&PxeBaseCode->Mode->DhcpAck,
		gPxeFileName,
		ImageHandle);
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
	EFI_DEVICE_PATH_PROTOCOL* PxeDevicePath;
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

	RelativeDevicePath = NULL;
	FullDevicePath = NULL;
	PxeDevicePath = NULL;
	RelativeDevicePathSize = 0;
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

	if (EFI_ERROR(EfiStatus) && (RelativeDevicePath != NULL))
	{
		PxeDevicePath = (FullDevicePath != NULL) ? FullDevicePath : RelativeDevicePath;
		EfiStatus = LoadEfiImageFromPxe(BootServices, ParentHandle, PxeDevicePath, ImagePath, &ImageHandle);
		PrintEfiCallStatus(L"LoadEfiImageFromPxe", EfiStatus);
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
