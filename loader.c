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

#include "loader.h"
#include "mm.h"
#include "compiler.h"
#include "efiguid.h"
#include "efipath.h"
#include "optical.h"
#include "print.h"

#define PXE_MAX_IMAGE_SIZE (256ULL * 1024ULL * 1024ULL)

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
	PrintEfiStatus(TraceName, Status);
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
	Status = EfiGetParentLoadedImage(&LoadedImage);
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
		PrintEfiStatus(L"PxeBaseCode.Start", Status);
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
		PrintEfiStatus(L"PxeBaseCode.Dhcp", Status);
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
	PrintEfiStatus(L"PxeBaseCode.Mtftp(GetFileSize)", Status);
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
	Status = EfiAllocatePool((UINTN)BufferSize, &Buffer);
	PrintEfiStatus(L"AllocatePool(PXE image)", Status);
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
	PrintEfiStatus(L"PxeBaseCode.Mtftp(ReadFile)", Status);
	SerialPrint(L"PXE bytes read=0x%016llX\n", BufferSize);
	if (EFI_ERROR(Status))
	{
		EfiFreePool(Buffer);
		return Status;
	}

	*ImageBuffer = Buffer;
	*ImageSize = (UINTN)BufferSize;
	return EFI_SUCCESS;
}

static EFI_STATUS LoadEfiImage(
	EFI_DEVICE_PATH_PROTOCOL* DevicePath,
	const VOID* ImageBuffer,
	UINTN ImageSize,
	const CHAR16* TraceName,
	EFI_HANDLE* ImageHandle)
{
	EFI_STATUS Status;

	*ImageHandle = NULL;
	Status = gContext.BootServices->LoadImage(
		FALSE,
		gContext.ParentHandle,
		DevicePath,
		(VOID*)ImageBuffer,
		ImageSize,
		ImageHandle);
	PrintEfiStatus(TraceName, Status);
	SerialPrint(L"ImageHandle=%p\n", *ImageHandle);
	return Status;
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
	PrintEfiStatus(L"GetPxeServerIp", Status);
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
	Status = LoadEfiImage(DevicePath, ImageBuffer, ImageSize, L"LoadImage(pxe)", ImageHandle);
	EfiFreePool(ImageBuffer);
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
	PrintEfiStatus(L"BuildPxeFileName", Status);
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
	PrintEfiStatus(L"EnsurePxeBaseCodeStarted", Status);
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
	SerialPrint(L"StartImage handle=%p before\n", ImageHandle);
	Status = gContext.BootServices->StartImage(ImageHandle, &ExitDataSize, &ExitData);
	PrintEfiStatus(L"StartImage", Status);
	SerialPrint(L"ExitDataSize=0x%016llX ExitData=%p\n", (UINT64)ExitDataSize, (VOID*)ExitData);
	return Status;
}

static EFI_STATUS LoadTargetImage(
	CHAR16* ImagePath,
	EFI_HANDLE* ImageHandle,
	EFI_DEVICE_PATH_PROTOCOL** RelativeDevicePath,
	EFI_DEVICE_PATH_PROTOCOL** FullDevicePath)
{
	EFI_DEVICE_PATH_PROTOCOL* ImageDevicePath;
	VOID* OpticalImageBuffer;
	UINTN OpticalImageSize;
	UINTN RelativeDevicePathSize;
	EFI_STATUS Status;

	OpticalImageBuffer = NULL;
	OpticalImageSize = 0;
	Status = EFI_STATUS_INVALID_PARAMETER;
	if ((ImagePath == NULL) || (ImageHandle == NULL) ||
		(RelativeDevicePath == NULL) || (FullDevicePath == NULL))
	{
		goto out;
	}

	*ImageHandle = NULL;
	*RelativeDevicePath = NULL;
	*FullDevicePath = NULL;
	RelativeDevicePathSize = 0;

	Status = EfiCreateFileDevicePath(ImagePath, RelativeDevicePath, &RelativeDevicePathSize);
	PrintEfiStatus(L"FileDevicePath", Status);
	if (EFI_ERROR(Status))
	{
		goto out;
	}

	Status = LoadEfiImage(*RelativeDevicePath, NULL, 0, L"LoadImage(relative)", ImageHandle);
	if (!EFI_ERROR(Status))
	{
		goto out;
	}

	Status = EfiBuildFullImageDevicePath(*RelativeDevicePath, FullDevicePath);
	PrintEfiStatus(L"BuildFullImageDevicePath", Status);
	if (!EFI_ERROR(Status))
	{
		Status = LoadEfiImage(*FullDevicePath, NULL, 0, L"LoadImage(full)", ImageHandle);
		if (!EFI_ERROR(Status))
		{
			goto out;
		}
	}

	ImageDevicePath = (*FullDevicePath != NULL) ? *FullDevicePath : *RelativeDevicePath;
	Status = OpticalReadFile(ImagePath, &OpticalImageBuffer, &OpticalImageSize);
	PrintEfiStatus(L"OpticalReadFile", Status);
	if (!EFI_ERROR(Status))
	{
		Status = LoadEfiImage(ImageDevicePath, OpticalImageBuffer, OpticalImageSize, L"LoadImage(optical)", ImageHandle);
		if (!EFI_ERROR(Status))
		{
			goto out;
		}
	}

	Status = LoadEfiImageFromPxe(ImageDevicePath, ImagePath, ImageHandle);
	PrintEfiStatus(L"LoadEfiImageFromPxe", Status);

out:
	EfiFreePool(OpticalImageBuffer);
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

NTSTATUS EfiStartApplication(
	CHAR16* ImagePath,
	const VOID* ImageBuffer,
	UINTN ImageSize,
	EFI_HANDLE* StartedImageHandle)
{
	EFI_DEVICE_PATH_PROTOCOL* RelativeDevicePath;
	EFI_DEVICE_PATH_PROTOCOL* FullDevicePath;
	EFI_HANDLE ImageHandle;
	BOOLEAN BufferSource;
	EFI_STATUS Status;
	NTSTATUS NtStatus;

	BufferSource = (ImageBuffer != NULL);
	if ((BufferSource && ((ImagePath != NULL) || (ImageSize == 0))) ||
		(!BufferSource && ((ImagePath == NULL) || (ImageSize != 0))))
	{
		return STATUS_INVALID_PARAMETER;
	}

	if (StartedImageHandle != NULL)
	{
		*StartedImageHandle = NULL;
	}

	SerialPrint(L"BS=%p LoadImage=%p StartImage=%p\n",
		(VOID*)gContext.BootServices,
		(VOID*)(UINTN)gContext.BootServices->LoadImage,
		(VOID*)(UINTN)gContext.BootServices->StartImage);

	Status = gContext.BootServices->SetWatchdogTimer(0, 0, 0, NULL);
	PrintEfiStatus(L"SetWatchdogTimer", Status);

	RelativeDevicePath = NULL;
	FullDevicePath = NULL;
	ImageHandle = NULL;
	if (BufferSource)
	{
		SerialPrint(L"LoadImage(buffer) data=%p size=0x%016llX before\n", (VOID*)ImageBuffer, (UINT64)ImageSize);
		Status = LoadEfiImage(NULL, ImageBuffer, ImageSize, L"LoadImage(buffer)", &ImageHandle);
	}
	else
	{
		EfiPrint(L"LoadImage %s\n", ImagePath);
		SerialPrint(L"Target image path=%s\n", ImagePath);
		Status = LoadTargetImage(ImagePath, &ImageHandle, &RelativeDevicePath, &FullDevicePath);
	}

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
	if (StartedImageHandle != NULL)
	{
		*StartedImageHandle = ImageHandle;
	}

cleanup:
	EfiFreePool(FullDevicePath);
	EfiFreePool(RelativeDevicePath);
	return NtStatus;
}
