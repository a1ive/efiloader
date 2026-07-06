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

#include "compiler.h"
#include "efiguid.h"
#include "efipath.h"
#include "loader.h"
#include "mm.h"
#include "print.h"

#define DEVICE_PATH_MAX_NODES 1024
#define DEVICE_PATH_MAX_SCAN_SIZE (1024ULL * 1024ULL)

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

static EFI_STATUS GetDevicePathSize(
	EFI_DEVICE_PATH_PROTOCOL* DevicePath,
	UINTN* DevicePathSize)
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

	Status = EfiAllocatePool(SourceSize, (VOID**)Destination);
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
	Status = EfiAllocatePool(DestinationSize, (VOID**)Destination);
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

BOOLEAN EfiIsDevicePathPrefix(
	EFI_DEVICE_PATH_PROTOCOL* Candidate,
	EFI_DEVICE_PATH_PROTOCOL* Child,
	UINTN* PrefixSize,
	BOOLEAN* ExactMatch)
{
	UINTN CandidateSize;
	UINTN ChildSize;
	UINTN CandidatePayloadSize;
	UINTN ChildPayloadSize;

	if (EFI_ERROR(GetDevicePathSize(Candidate, &CandidateSize)) ||
		EFI_ERROR(GetDevicePathSize(Child, &ChildSize)) ||
		(CandidateSize < END_DEVICE_PATH_LENGTH) ||
		(ChildSize < END_DEVICE_PATH_LENGTH))
	{
		return FALSE;
	}

	CandidatePayloadSize = CandidateSize - END_DEVICE_PATH_LENGTH;
	ChildPayloadSize = ChildSize - END_DEVICE_PATH_LENGTH;
	if ((CandidatePayloadSize > ChildPayloadSize) ||
		(memcmp(Candidate, Child, CandidatePayloadSize) != 0))
	{
		return FALSE;
	}

	*PrefixSize = CandidatePayloadSize;
	*ExactMatch = (CandidatePayloadSize == ChildPayloadSize) ? TRUE : FALSE;
	return TRUE;
}

EFI_STATUS EfiGetParentLoadedImage(
	EFI_LOADED_IMAGE_PROTOCOL** LoadedImage)
{
	EFI_STATUS Status;

	if (LoadedImage == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*LoadedImage = NULL;
	Status = gContext.BootServices->HandleProtocol(
		gContext.ParentHandle,
		&gEfiLoadedImageProtocolGuid,
		(VOID**)LoadedImage);
	PrintEfiStatus(L"HandleProtocol(LoadedImage)", Status);
	SerialPrint(
		L"HandleProtocol(LoadedImage) interface=%p\n",
		(VOID*)*LoadedImage);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	if (*LoadedImage == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	return EFI_SUCCESS;
}

EFI_STATUS EfiGetParentDevicePath(
	EFI_DEVICE_PATH_PROTOCOL** DevicePath)
{
	EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
	EFI_STATUS Status;

	if (DevicePath == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*DevicePath = NULL;
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

	Status = gContext.BootServices->HandleProtocol(
		LoadedImage->DeviceHandle,
		&gEfiDevicePathProtocolGuid,
		(VOID**)DevicePath);
	PrintEfiStatus(L"HandleProtocol(DevicePath)", Status);
	SerialPrint(
		L"HandleProtocol(DevicePath) interface=%p\n",
		(VOID*)*DevicePath);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	if (*DevicePath == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	SerialPrint(
		L"DeviceHandle=%p DevicePath=%p\n",
		LoadedImage->DeviceHandle,
		(VOID*)*DevicePath);
	return EFI_SUCCESS;
}

EFI_STATUS EfiCreateFileDevicePath(
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

	Status = EfiAllocateZeroPool(TotalSize, (VOID**)&FilePath);
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

EFI_STATUS EfiBuildFullImageDevicePath(
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
	Status = EfiGetParentDevicePath(&ParentDevicePath);
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
	PrintEfiStatus(L"AppendDevicePath(parent,file)", Status);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = GetDevicePathSize(*FullDevicePath, &FullDevicePathSize);
	if (EFI_ERROR(Status))
	{
		EfiFreePool(*FullDevicePath);
		*FullDevicePath = NULL;
		return Status;
	}

	SerialPrint(L"DevicePath sizes parent=0x%016llX file=0x%016llX full=0x%016llX\n",
		(UINT64)ParentDevicePathSize,
		(UINT64)RelativeDevicePathSize,
		(UINT64)FullDevicePathSize);
	return EFI_SUCCESS;
}
