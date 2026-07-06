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
#include "efidisk.h"
#include "efiguid.h"
#include "efipath.h"
#include "fscommon.h"
#include "loader.h"
#include "mm.h"
#include "print.h"

#define EFI_DISK_SCRATCH_TARGET_SIZE (128ULL * 1024ULL)
#define EFI_OPTICAL_MINIMUM_BLOCK_SIZE 2048U

struct _EFI_DISK
{
	EFI_BLOCK_IO_PROTOCOL* BlockIo;
	VOID* ScratchAllocation;
	UINT8* Scratch;
	UINTN ScratchSize;
	UINTN BlockSize;
	UINTN BlockShift;
};

static EFI_STATUS FindOpticalBlockIo(EFI_BLOCK_IO_PROTOCOL** BlockIo)
{
	EFI_DEVICE_PATH_PROTOCOL* BootDevicePath;
	EFI_DEVICE_PATH_PROTOCOL* CandidateDevicePath;
	EFI_BLOCK_IO_PROTOCOL* CandidateBlockIo;
	EFI_BLOCK_IO_PROTOCOL* BestBlockIo;
	EFI_BLOCK_IO_PROTOCOL* FallbackBlockIo;
	EFI_HANDLE* Handles;
	UINTN HandleCount;
	UINTN HandleIndex;
	UINTN PrefixSize;
	UINTN BestPrefixSize;
	BOOLEAN ExactMatch;
	EFI_STATUS Status;

	Status = EfiGetParentDevicePath(&BootDevicePath);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	HandleCount = 0;
	Status = gContext.BootServices->LocateHandleBuffer(
		ByProtocol,
		&gEfiBlockIoProtocolGuid,
		NULL,
		&HandleCount,
		&Handles);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	BestBlockIo = NULL;
	FallbackBlockIo = NULL;
	BestPrefixSize = 0;
	for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++)
	{
		Status = gContext.BootServices->HandleProtocol(
			Handles[HandleIndex],
			&gEfiDevicePathProtocolGuid,
			(VOID**)&CandidateDevicePath);
		if (EFI_ERROR(Status) || (CandidateDevicePath == NULL))
		{
			continue;
		}

		Status = gContext.BootServices->HandleProtocol(
			Handles[HandleIndex],
			&gEfiBlockIoProtocolGuid,
			(VOID**)&CandidateBlockIo);
		if (EFI_ERROR(Status) || (CandidateBlockIo == NULL) ||
			(CandidateBlockIo->Media == NULL) ||
			(CandidateBlockIo->ReadBlocks == NULL) ||
			!CandidateBlockIo->Media->MediaPresent)
		{
			continue;
		}

		if ((FallbackBlockIo == NULL) &&
			CandidateBlockIo->Media->RemovableMedia &&
			!CandidateBlockIo->Media->LogicalPartition &&
			(CandidateBlockIo->Media->BlockSize >=
				EFI_OPTICAL_MINIMUM_BLOCK_SIZE))
		{
			FallbackBlockIo = CandidateBlockIo;
		}

		if (CandidateBlockIo->Media->BlockSize <
			EFI_OPTICAL_MINIMUM_BLOCK_SIZE)
		{
			continue;
		}

		if (!EfiIsDevicePathPrefix(
			CandidateDevicePath,
			BootDevicePath,
			&PrefixSize,
			&ExactMatch))
		{
			continue;
		}

		if (ExactMatch)
		{
			if ((BestBlockIo == NULL) &&
				!CandidateBlockIo->Media->LogicalPartition)
			{
				BestBlockIo = CandidateBlockIo;
			}

			continue;
		}

		if ((BestBlockIo == NULL) || (PrefixSize >= BestPrefixSize))
		{
			BestBlockIo = CandidateBlockIo;
			BestPrefixSize = PrefixSize;
		}
	}

	EfiFreePool(Handles);
	if (BestBlockIo == NULL)
	{
		BestBlockIo = FallbackBlockIo;
	}

	if (BestBlockIo == NULL)
	{
		return EFI_STATUS_NOT_FOUND;
	}

	SerialPrint(
		L"Optical BlockIo=%p media=%x block=0x%x last=0x%016llX\n",
		(VOID*)BestBlockIo,
		(UINT64)BestBlockIo->Media->MediaId,
		(UINT64)BestBlockIo->Media->BlockSize,
		(UINT64)BestBlockIo->Media->LastBlock);
	*BlockIo = BestBlockIo;
	return EFI_SUCCESS;
}

static EFI_STATUS InitializeDisk(
	EFI_BLOCK_IO_PROTOCOL* BlockIo,
	EFI_DISK* Disk)
{
	UINTN Alignment;
	UINTN AllocationSize;
	UINTN ScratchSize;
	UINTN BlockShift;
	UINTN AlignmentShift;
	UINTN Address;
	EFI_STATUS Status;

	if (!FsGetSectorShift(BlockIo->Media->BlockSize, &BlockShift))
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	Alignment = BlockIo->Media->IoAlign;
	if (Alignment < BlockIo->Media->BlockSize)
	{
		Alignment = BlockIo->Media->BlockSize;
	}

	if (!FsGetSectorShift(Alignment, &AlignmentShift))
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	ScratchSize = EFI_DISK_SCRATCH_TARGET_SIZE;
	if (ScratchSize < BlockIo->Media->BlockSize)
	{
		ScratchSize = BlockIo->Media->BlockSize;
	}
	else
	{
		ScratchSize &= ~((UINTN)BlockIo->Media->BlockSize - 1);
	}

	if (ScratchSize > ((UINTN)-1 - (Alignment - 1)))
	{
		return EFI_STATUS_BUFFER_TOO_SMALL;
	}

	AllocationSize = ScratchSize + Alignment - 1;
	Status = EfiAllocatePool(AllocationSize, &Disk->ScratchAllocation);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Address = (UINTN)Disk->ScratchAllocation;
	Address = (Address + Alignment - 1) & ~(Alignment - 1);
	Disk->BlockIo = BlockIo;
	Disk->Scratch = (UINT8*)Address;
	Disk->ScratchSize = ScratchSize;
	Disk->BlockSize = BlockIo->Media->BlockSize;
	Disk->BlockShift = BlockShift;
	return EFI_SUCCESS;
}

EFI_STATUS EfiDiskOpenBootOptical(EFI_DISK** Disk)
{
	EFI_BLOCK_IO_PROTOCOL* BlockIo;
	EFI_DISK* NewDisk;
	EFI_STATUS Status;

	if (Disk == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*Disk = NULL;
	Status = FindOpticalBlockIo(&BlockIo);
	if (EFI_ERROR(Status))
	{
		SerialPrint(L"FindOpticalBlockIo status=0x%016llX\n", Status);
		return Status;
	}

	Status = EfiAllocateZeroPool(sizeof(*NewDisk), (VOID**)&NewDisk);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = InitializeDisk(BlockIo, NewDisk);
	if (EFI_ERROR(Status))
	{
		EfiFreePool(NewDisk);
		return Status;
	}

	*Disk = NewDisk;
	return EFI_SUCCESS;
}

VOID EfiDiskClose(EFI_DISK* Disk)
{
	EfiFreePool(Disk->ScratchAllocation);
	EfiFreePool(Disk);
}

EFI_STATUS EfiDiskRead(
	EFI_DISK* Disk,
	UINT64 Offset,
	UINTN Size,
	VOID* Buffer)
{
	UINT8* Destination;
	UINTN BlockOffset;
	UINTN BlocksToRead;
	UINTN BytesToCopy;
	UINTN BytesToRead;
	UINTN RequiredBytes;
	EFI_LBA Lba;
	EFI_STATUS Status;

	if (Size == 0)
	{
		return EFI_SUCCESS;
	}

	Destination = (UINT8*)Buffer;
	while (Size != 0)
	{
		Lba = FsShiftRight64(Offset, Disk->BlockShift);
		BlockOffset = (UINTN)(Offset & (Disk->BlockSize - 1));
		BytesToCopy = Disk->ScratchSize - BlockOffset;
		if (BytesToCopy > Size)
		{
			BytesToCopy = Size;
		}

		RequiredBytes = BlockOffset + BytesToCopy;
		BlocksToRead =
			(RequiredBytes + Disk->BlockSize - 1) >> Disk->BlockShift;
		BytesToRead = BlocksToRead << Disk->BlockShift;
		if ((BlocksToRead == 0) ||
			(Lba > Disk->BlockIo->Media->LastBlock) ||
			((EFI_LBA)(BlocksToRead - 1) >
				(Disk->BlockIo->Media->LastBlock - Lba)))
		{
			return EFI_STATUS_NOT_FOUND;
		}

		Status = Disk->BlockIo->ReadBlocks(
			Disk->BlockIo,
			Disk->BlockIo->Media->MediaId,
			Lba,
			BytesToRead,
			Disk->Scratch);
		if (EFI_ERROR(Status))
		{
			return Status;
		}

		memcpy(Destination, Disk->Scratch + BlockOffset, BytesToCopy);
		if (Offset > ((UINT64)-1 - BytesToCopy))
		{
			return EFI_STATUS_BUFFER_TOO_SMALL;
		}

		Offset += BytesToCopy;
		Destination += BytesToCopy;
		Size -= BytesToCopy;
	}

	return EFI_SUCCESS;
}
