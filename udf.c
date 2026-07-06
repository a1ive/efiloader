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
#include "fscommon.h"
#include "mm.h"
#include "print.h"
#include "udf.h"

#define UDF_TAG_AVDP 0x0002
#define UDF_TAG_PD 0x0005
#define UDF_TAG_LVD 0x0006
#define UDF_TAG_TD 0x0008
#define UDF_TAG_FSD 0x0100
#define UDF_TAG_FID 0x0101
#define UDF_TAG_AED 0x0102
#define UDF_TAG_FE 0x0105
#define UDF_TAG_EFE 0x010A

#define UDF_AD_SHORT 0
#define UDF_AD_LONG 1
#define UDF_AD_IN_ICB 3

#define UDF_FID_DELETED 0x04
#define UDF_FID_PARENT 0x08

#define UDF_FILE_TYPE_DIRECTORY 0x04
#define UDF_MAX_PARTITIONS 8U
#define UDF_MAX_PARTITION_MAPS 8U
#define UDF_MAX_VDS_BLOCKS 4096U
#define UDF_MAX_AED_DEPTH 16U
#define UDF_VRS_BLOCK_SIZE 2048U

typedef struct _UDF_LONG_AD
{
	UINT32 Length;
	UINT32 Block;
	UINT16 PartitionReference;
} UDF_LONG_AD;

typedef struct _UDF_PARTITION
{
	UINT16 Number;
	UINT32 Start;
	UINT32 Length;
} UDF_PARTITION;

typedef struct _UDF_PARTITION_MAP
{
	UINT16 PartitionNumber;
	UINTN PartitionIndex;
} UDF_PARTITION_MAP;

typedef struct _UDF_CONTEXT
{
	EFI_DISK* Disk;
	UINTN BlockSize;
	UINTN BlockShift;
	UDF_PARTITION Partitions[UDF_MAX_PARTITIONS];
	UINTN PartitionCount;
	UDF_PARTITION_MAP Maps[UDF_MAX_PARTITION_MAPS];
	UINTN MapCount;
	UDF_LONG_AD RootIcb;
} UDF_CONTEXT;

typedef struct _UDF_NODE
{
	UINT8* Entry;
	UINT16 PartitionReference;
} UDF_NODE;

static VOID UdfReadLongAd(const UINT8* Buffer, UDF_LONG_AD* LongAd)
{
	LongAd->Length = FsReadLe32(Buffer);
	LongAd->Block = FsReadLe32(Buffer + 4);
	LongAd->PartitionReference = FsReadLe16(Buffer + 8);
}

static EFI_STATUS UdfTranslateBlock(
	UDF_CONTEXT* Context,
	UINT16 PartitionReference,
	UINT32 RelativeBlock,
	UINT32* PhysicalBlock)
{
	UDF_PARTITION* Partition;

	if ((PartitionReference >= Context->MapCount) ||
		(Context->Maps[PartitionReference].PartitionIndex >=
			Context->PartitionCount))
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	Partition =
		&Context->Partitions[
			Context->Maps[PartitionReference].PartitionIndex];
	if ((RelativeBlock >= Partition->Length) ||
		(Partition->Start > (UINT32)-1 - RelativeBlock))
	{
		return EFI_STATUS_NOT_FOUND;
	}

	*PhysicalBlock = Partition->Start + RelativeBlock;
	return EFI_SUCCESS;
}

static BOOLEAN UdfHasVolumeRecognitionSequence(EFI_DISK* Disk)
{
	UINT8 Descriptor[7];
	UINTN DescriptorIndex;

	for (DescriptorIndex = 0; DescriptorIndex < 64; DescriptorIndex++)
	{
		if (EFI_ERROR(EfiDiskRead(
			Disk,
			32768ULL + ((UINT64)DescriptorIndex * UDF_VRS_BLOCK_SIZE),
			sizeof(Descriptor),
			Descriptor)))
		{
			return FALSE;
		}

		if ((memcmp(Descriptor + 1, "NSR02", 5) == 0) ||
			(memcmp(Descriptor + 1, "NSR03", 5) == 0))
		{
			return TRUE;
		}

		if (memcmp(Descriptor + 1, "TEA01", 5) == 0)
		{
			break;
		}
	}

	return FALSE;
}

static EFI_STATUS UdfTryMount(UDF_CONTEXT* Context, UINTN BlockSize)
{
	UINT8* Block;
	UINT32 AnchorBlock;
	UINT32 AnchorCandidates[2] = { 256, 512 };
	UINT32 VdsStart;
	UINT32 VdsLength;
	UINT32 VdsBlockCount;
	UINT32 VdsIndex;
	UINT32 LogicalBlock;
	UINT32 FileSetBlock;
	UINT32 MapTableLength;
	UINT32 MapCount;
	UINT32 MapOffset;
	UINT16 PartitionNumber;
	UINT16 Tag;
	UINTN AnchorIndex;
	UINTN BlockShift;
	UINTN Index;
	UINTN PartitionIndex;
	BOOLEAN HaveAnchor;
	BOOLEAN HaveLvd;
	UDF_LONG_AD FileSet;
	EFI_STATUS Status;

	if (!FsGetSectorShift(BlockSize, &BlockShift))
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	Status = EfiAllocatePool(BlockSize, (VOID**)&Block);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	HaveAnchor = FALSE;
	VdsStart = 0;
	VdsLength = 0;
	for (AnchorIndex = 0; AnchorIndex < 2; AnchorIndex++)
	{
		AnchorBlock = AnchorCandidates[AnchorIndex];
		Status = EfiDiskRead(
			Context->Disk,
			(UINT64)AnchorBlock * BlockSize,
			BlockSize,
			Block);
		if (EFI_ERROR(Status))
		{
			continue;
		}

		if ((FsReadLe16(Block) == UDF_TAG_AVDP) &&
			(FsReadLe32(Block + 12) == AnchorBlock))
		{
			VdsLength = FsReadLe32(Block + 16);
			VdsStart = FsReadLe32(Block + 20);
			HaveAnchor = TRUE;
			break;
		}
	}

	if (!HaveAnchor || (VdsLength < BlockSize))
	{
		Status = EFI_STATUS_UNSUPPORTED;
		goto cleanup;
	}

	Context->BlockSize = BlockSize;
	Context->BlockShift = BlockShift;
	Context->PartitionCount = 0;
	Context->MapCount = 0;
	HaveLvd = FALSE;
	memset(&FileSet, 0, sizeof(FileSet));
	if (VdsLength > (UINT32)-1 - ((UINT32)BlockSize - 1))
	{
		Status = EFI_STATUS_UNSUPPORTED;
		goto cleanup;
	}

	VdsBlockCount =
		(VdsLength + (UINT32)BlockSize - 1) >> BlockShift;
	if (VdsBlockCount > UDF_MAX_VDS_BLOCKS)
	{
		Status = EFI_STATUS_UNSUPPORTED;
		goto cleanup;
	}

	for (VdsIndex = 0; VdsIndex < VdsBlockCount; VdsIndex++)
	{
		if (VdsStart > (UINT32)-1 - VdsIndex)
		{
			Status = EFI_STATUS_UNSUPPORTED;
			goto cleanup;
		}

		LogicalBlock = VdsStart + VdsIndex;
		Status = EfiDiskRead(
			Context->Disk,
			(UINT64)LogicalBlock * BlockSize,
			BlockSize,
			Block);
		if (EFI_ERROR(Status))
		{
			goto cleanup;
		}

		Tag = FsReadLe16(Block);
		if (Tag == UDF_TAG_TD)
		{
			break;
		}

		if (Tag == UDF_TAG_PD)
		{
			if ((BlockSize < 196) ||
				(Context->PartitionCount >= UDF_MAX_PARTITIONS))
			{
				Status = EFI_STATUS_UNSUPPORTED;
				goto cleanup;
			}

			Context->Partitions[Context->PartitionCount].Number =
				FsReadLe16(Block + 22);
			Context->Partitions[Context->PartitionCount].Start =
				FsReadLe32(Block + 188);
			Context->Partitions[Context->PartitionCount].Length =
				FsReadLe32(Block + 192);
			Context->PartitionCount++;
		}
		else if (Tag == UDF_TAG_LVD)
		{
			if ((BlockSize < 440) ||
				(FsReadLe32(Block + 212) != BlockSize))
			{
				Status = EFI_STATUS_UNSUPPORTED;
				goto cleanup;
			}

			MapTableLength = FsReadLe32(Block + 264);
			MapCount = FsReadLe32(Block + 268);
			if ((MapCount > UDF_MAX_PARTITION_MAPS) ||
				(MapTableLength > BlockSize - 440))
			{
				Status = EFI_STATUS_UNSUPPORTED;
				goto cleanup;
			}

			Context->MapCount = 0;
			MapOffset = 440;
			for (Index = 0; Index < MapCount; Index++)
			{
				if ((MapOffset > BlockSize - 2) ||
					(Block[MapOffset] != 1) ||
					(Block[MapOffset + 1] != 6) ||
					(MapOffset + 6 > 440 + MapTableLength))
				{
					Status = EFI_STATUS_UNSUPPORTED;
					goto cleanup;
				}

				Context->Maps[Context->MapCount].PartitionNumber =
					FsReadLe16(Block + MapOffset + 4);
				Context->Maps[Context->MapCount].PartitionIndex =
					(UINTN)-1;
				Context->MapCount++;
				MapOffset += 6;
			}

			UdfReadLongAd(Block + 248, &FileSet);
			HaveLvd = TRUE;
		}
	}

	if (!HaveLvd || (Context->PartitionCount == 0) ||
		(Context->MapCount == 0))
	{
		Status = EFI_STATUS_UNSUPPORTED;
		goto cleanup;
	}

	for (Index = 0; Index < Context->MapCount; Index++)
	{
		PartitionNumber = Context->Maps[Index].PartitionNumber;
		for (PartitionIndex = 0;
			PartitionIndex < Context->PartitionCount;
			PartitionIndex++)
		{
			if (Context->Partitions[PartitionIndex].Number ==
				PartitionNumber)
			{
				Context->Maps[Index].PartitionIndex = PartitionIndex;
				break;
			}
		}

		if (PartitionIndex == Context->PartitionCount)
		{
			Status = EFI_STATUS_UNSUPPORTED;
			goto cleanup;
		}
	}

	Status = UdfTranslateBlock(
		Context,
		FileSet.PartitionReference,
		FileSet.Block,
		&FileSetBlock);
	if (EFI_ERROR(Status))
	{
		goto cleanup;
	}

	Status = EfiDiskRead(
		Context->Disk,
		(UINT64)FileSetBlock * BlockSize,
		BlockSize,
		Block);
	if (EFI_ERROR(Status) || (FsReadLe16(Block) != UDF_TAG_FSD) ||
		(BlockSize < 416))
	{
		Status = EFI_STATUS_UNSUPPORTED;
		goto cleanup;
	}

	UdfReadLongAd(Block + 400, &Context->RootIcb);
	Status = EFI_SUCCESS;

cleanup:
	EfiFreePool(Block);
	return Status;
}

static EFI_STATUS UdfMount(EFI_DISK* Disk, UDF_CONTEXT* Context)
{
	UINTN BlockSize;
	EFI_STATUS Status;

	if (!UdfHasVolumeRecognitionSequence(Disk))
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	memset(Context, 0, sizeof(*Context));
	Context->Disk = Disk;
	for (BlockSize = 512; BlockSize <= 4096; BlockSize <<= 1)
	{
		Status = UdfTryMount(Context, BlockSize);
		if (!EFI_ERROR(Status))
		{
			return EFI_SUCCESS;
		}
	}

	return EFI_STATUS_UNSUPPORTED;
}

static UINTN UdfFileEntryDataOffset(UINT16 Tag)
{
	return (Tag == UDF_TAG_FE) ? 176 : 216;
}

static UINT32 UdfFileEntryExtendedAttributeLength(const UDF_NODE* Node)
{
	UINT16 Tag;

	Tag = FsReadLe16(Node->Entry);
	return FsReadLe32(Node->Entry + ((Tag == UDF_TAG_FE) ? 168 : 208));
}

static UINT32 UdfFileEntryAllocationLength(const UDF_NODE* Node)
{
	UINT16 Tag;

	Tag = FsReadLe16(Node->Entry);
	return FsReadLe32(Node->Entry + ((Tag == UDF_TAG_FE) ? 172 : 212));
}

static UINT64 UdfFileEntrySize(const UDF_NODE* Node)
{
	return FsReadLe64(Node->Entry + 56);
}

static UINT16 UdfFileEntryFlags(const UDF_NODE* Node)
{
	return FsReadLe16(Node->Entry + 34);
}

static UINT8 UdfFileEntryType(const UDF_NODE* Node)
{
	return Node->Entry[27];
}

static VOID UdfFreeNode(UDF_NODE* Node)
{
	EfiFreePool(Node->Entry);
	Node->Entry = NULL;
}

static EFI_STATUS UdfReadIcb(
	UDF_CONTEXT* Context,
	const UDF_LONG_AD* Icb,
	UDF_NODE* Node)
{
	UINT32 PhysicalBlock;
	UINT16 Tag;
	EFI_STATUS Status;

	memset(Node, 0, sizeof(*Node));
	Status = UdfTranslateBlock(
		Context,
		Icb->PartitionReference,
		Icb->Block,
		&PhysicalBlock);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = EfiAllocatePool(Context->BlockSize, (VOID**)&Node->Entry);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = EfiDiskRead(
		Context->Disk,
		(UINT64)PhysicalBlock * Context->BlockSize,
		Context->BlockSize,
		Node->Entry);
	if (EFI_ERROR(Status))
	{
		UdfFreeNode(Node);
		return Status;
	}

	Tag = FsReadLe16(Node->Entry);
	if ((Tag != UDF_TAG_FE) && (Tag != UDF_TAG_EFE))
	{
		UdfFreeNode(Node);
		return EFI_STATUS_UNSUPPORTED;
	}

	Node->PartitionReference = Icb->PartitionReference;
	return EFI_SUCCESS;
}

static EFI_STATUS UdfReadAllocationDescriptors(
	UDF_CONTEXT* Context,
	UDF_NODE* Node,
	const UINT8* Descriptors,
	UINTN DescriptorLength,
	UINTN DescriptorType,
	UINT8* Output,
	UINT64 OutputSize,
	UINT64* OutputOffset,
	UINTN Depth)
{
	UINT8* Extension;
	UINT32 ExtentLengthField;
	UINT32 ExtentLength;
	UINT32 RelativeBlock;
	UINT32 PhysicalBlock;
	UINT16 PartitionReference;
	UINTN DescriptorSize;
	UINTN Offset;
	UINTN ExtensionDescriptorLength;
	UINT64 CopyLength;
	UINT64 PhysicalOffset;
	EFI_STATUS Status;

	if ((Depth > UDF_MAX_AED_DEPTH) ||
		((DescriptorType != UDF_AD_SHORT) &&
		 (DescriptorType != UDF_AD_LONG)))
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	DescriptorSize = (DescriptorType == UDF_AD_SHORT) ? 8 : 16;
	Offset = 0;
	while (Offset + DescriptorSize <= DescriptorLength)
	{
		ExtentLengthField = FsReadLe32(Descriptors + Offset);
		ExtentLength = ExtentLengthField & 0x3FFFFFFF;
		if (ExtentLength == 0)
		{
			Offset += DescriptorSize;
			continue;
		}

		RelativeBlock = FsReadLe32(Descriptors + Offset + 4);
		PartitionReference = (DescriptorType == UDF_AD_SHORT) ?
			Node->PartitionReference :
			FsReadLe16(Descriptors + Offset + 8);

		if ((ExtentLengthField >> 30) == 3)
		{
			if ((ExtentLength < 24) ||
				(ExtentLength > Context->BlockSize))
			{
				return EFI_STATUS_UNSUPPORTED;
			}

			Status = UdfTranslateBlock(
				Context,
				PartitionReference,
				RelativeBlock,
				&PhysicalBlock);
			if (EFI_ERROR(Status))
			{
				return Status;
			}

			Status = EfiAllocatePool(ExtentLength, (VOID**)&Extension);
			if (EFI_ERROR(Status))
			{
				return Status;
			}

			Status = EfiDiskRead(
				Context->Disk,
				(UINT64)PhysicalBlock * Context->BlockSize,
				ExtentLength,
				Extension);
			if (EFI_ERROR(Status) ||
				(FsReadLe16(Extension) != UDF_TAG_AED))
			{
				EfiFreePool(Extension);
				return EFI_ERROR(Status) ?
					Status : EFI_STATUS_UNSUPPORTED;
			}

			ExtensionDescriptorLength = FsReadLe32(Extension + 20);
			if (ExtensionDescriptorLength > ExtentLength - 24)
			{
				EfiFreePool(Extension);
				return EFI_STATUS_UNSUPPORTED;
			}

			Status = UdfReadAllocationDescriptors(
				Context,
				Node,
				Extension + 24,
				ExtensionDescriptorLength,
				DescriptorType,
				Output,
				OutputSize,
				OutputOffset,
				Depth + 1);
			EfiFreePool(Extension);
			if (EFI_ERROR(Status))
			{
				return Status;
			}

			Offset += DescriptorSize;
			continue;
		}

		if (*OutputOffset >= OutputSize)
		{
			break;
		}

		CopyLength = ExtentLength;
		if (CopyLength > OutputSize - *OutputOffset)
		{
			CopyLength = OutputSize - *OutputOffset;
		}

		if ((ExtentLengthField >> 30) == 0)
		{
			Status = UdfTranslateBlock(
				Context,
				PartitionReference,
				RelativeBlock,
				&PhysicalBlock);
			if (EFI_ERROR(Status))
			{
				return Status;
			}

			PhysicalOffset =
				FsShiftLeft32To64(PhysicalBlock, Context->BlockShift);
			Status = EfiDiskRead(
				Context->Disk,
				PhysicalOffset,
				(UINTN)CopyLength,
				Output + (UINTN)*OutputOffset);
			if (EFI_ERROR(Status))
			{
				return Status;
			}
		}
		else
		{
			memset(
				Output + (UINTN)*OutputOffset,
				0,
				(UINTN)CopyLength);
		}

		*OutputOffset += CopyLength;
		Offset += DescriptorSize;
	}

	return EFI_SUCCESS;
}

static EFI_STATUS UdfReadNodeData(
	UDF_CONTEXT* Context,
	UDF_NODE* Node,
	UINT64 SizeLimit,
	VOID** Buffer,
	UINTN* BufferSize)
{
	UINT8* Data;
	UINT32 ExtendedAttributeLength;
	UINT32 AllocationLength;
	UINT64 FileSize;
	UINT64 OutputOffset;
	UINTN DataOffset;
	UINTN DescriptorType;
	EFI_STATUS Status;

	*Buffer = NULL;
	*BufferSize = 0;
	FileSize = UdfFileEntrySize(Node);
	if ((FileSize == 0) || (FileSize > SizeLimit) ||
		(FileSize > (UINT64)((UINTN)-1)))
	{
		return EFI_STATUS_BUFFER_TOO_SMALL;
	}

	DataOffset = UdfFileEntryDataOffset(FsReadLe16(Node->Entry));
	ExtendedAttributeLength =
		UdfFileEntryExtendedAttributeLength(Node);
	AllocationLength = UdfFileEntryAllocationLength(Node);
	if ((DataOffset > Context->BlockSize) ||
		(ExtendedAttributeLength > Context->BlockSize - DataOffset) ||
		(AllocationLength >
			Context->BlockSize - DataOffset - ExtendedAttributeLength))
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	Status = EfiAllocatePool((UINTN)FileSize, (VOID**)&Data);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	DescriptorType = UdfFileEntryFlags(Node) & 7;
	if (DescriptorType == UDF_AD_IN_ICB)
	{
		if ((FileSize > AllocationLength) ||
			(FileSize >
				Context->BlockSize - DataOffset - ExtendedAttributeLength))
		{
			Status = EFI_STATUS_UNSUPPORTED;
			goto cleanup;
		}

		memcpy(
			Data,
			Node->Entry + DataOffset + ExtendedAttributeLength,
			(UINTN)FileSize);
	}
	else
	{
		OutputOffset = 0;
		Status = UdfReadAllocationDescriptors(
			Context,
			Node,
			Node->Entry + DataOffset + ExtendedAttributeLength,
			AllocationLength,
			DescriptorType,
			Data,
			FileSize,
			&OutputOffset,
			0);
		if (EFI_ERROR(Status) || (OutputOffset != FileSize))
		{
			if (!EFI_ERROR(Status))
			{
				Status = EFI_STATUS_UNSUPPORTED;
			}

			goto cleanup;
		}
	}

	*Buffer = Data;
	*BufferSize = (UINTN)FileSize;
	return EFI_SUCCESS;

cleanup:
	EfiFreePool(Data);
	return Status;
}

static BOOLEAN UdfNameEquals(
	const UINT8* Name,
	UINTN NameLength,
	const CHAR16* Component,
	UINTN ComponentLength)
{
	CHAR16 Character;
	UINTN CharacterCount;
	UINTN Index;

	if (NameLength == 0)
	{
		return FALSE;
	}

	if (Name[0] == 8)
	{
		CharacterCount = NameLength - 1;
		if (CharacterCount != ComponentLength)
		{
			return FALSE;
		}

		for (Index = 0; Index < CharacterCount; Index++)
		{
			Character = Name[Index + 1];
			if (FsFoldAsciiCharacter(Character) !=
				FsFoldAsciiCharacter(Component[Index]))
			{
				return FALSE;
			}
		}

		return TRUE;
	}

	if ((Name[0] == 16) && (((NameLength - 1) & 1) == 0))
	{
		CharacterCount = (NameLength - 1) >> 1;
		if (CharacterCount != ComponentLength)
		{
			return FALSE;
		}

		for (Index = 0; Index < CharacterCount; Index++)
		{
			Character =
				(CHAR16)(((CHAR16)Name[Index * 2 + 1] << 8) |
					Name[Index * 2 + 2]);
			if (FsFoldAsciiCharacter(Character) !=
				FsFoldAsciiCharacter(Component[Index]))
			{
				return FALSE;
			}
		}

		return TRUE;
	}

	return FALSE;
}

static EFI_STATUS UdfFindEntry(
	UDF_CONTEXT* Context,
	UDF_NODE* Directory,
	const CHAR16* Component,
	UINTN ComponentLength,
	UDF_NODE* FoundNode)
{
	UINT8* DirectoryData;
	UINT8* FileIdentifier;
	UINTN DirectorySize;
	UINTN Offset;
	UINTN FileIdentifierLength;
	UINTN ImplementationUseLength;
	UINTN RecordLength;
	UDF_LONG_AD Icb;
	EFI_STATUS Status;

	DirectorySize = 0;
	Status = UdfReadNodeData(
		Context,
		Directory,
		FS_MAX_DIRECTORY_SIZE,
		(VOID**)&DirectoryData,
		&DirectorySize);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Status = EFI_STATUS_NOT_FOUND;
	Offset = 0;
	while (Offset + 38 <= DirectorySize)
	{
		FileIdentifier = DirectoryData + Offset;
		if (FsReadLe16(FileIdentifier) != UDF_TAG_FID)
		{
			Status = EFI_STATUS_UNSUPPORTED;
			break;
		}

		FileIdentifierLength = FileIdentifier[19];
		ImplementationUseLength = FsReadLe16(FileIdentifier + 36);
		if ((ImplementationUseLength > DirectorySize - Offset - 38) ||
			(FileIdentifierLength >
				DirectorySize - Offset - 38 - ImplementationUseLength))
		{
			Status = EFI_STATUS_UNSUPPORTED;
			break;
		}

		RecordLength =
			38 + ImplementationUseLength + FileIdentifierLength;
		RecordLength = (RecordLength + 3) & ~(UINTN)3;
		if ((RecordLength == 0) ||
			(RecordLength > DirectorySize - Offset))
		{
			Status = EFI_STATUS_UNSUPPORTED;
			break;
		}

		if (((FileIdentifier[18] &
			(UDF_FID_DELETED | UDF_FID_PARENT)) == 0) &&
			UdfNameEquals(
				FileIdentifier + 38 + ImplementationUseLength,
				FileIdentifierLength,
				Component,
				ComponentLength))
		{
			UdfReadLongAd(FileIdentifier + 20, &Icb);
			Status = UdfReadIcb(Context, &Icb, FoundNode);
			break;
		}

		Offset += RecordLength;
	}

	EfiFreePool(DirectoryData);
	return Status;
}

EFI_STATUS UdfReadFile(
	EFI_DISK* Disk,
	CHAR16* FilePath,
	VOID** FileBuffer,
	UINTN* FileSize)
{
	UDF_CONTEXT Context;
	UDF_NODE CurrentNode;
	UDF_NODE FoundNode;
	CHAR16* Path;
	CHAR16* Component;
	UINTN ComponentLength;
	BOOLEAN LastComponent;
	EFI_STATUS Status;

	Status = UdfMount(Disk, &Context);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	memset(&CurrentNode, 0, sizeof(CurrentNode));
	Status = UdfReadIcb(&Context, &Context.RootIcb, &CurrentNode);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Path = FilePath;
	while (FsGetNextPathComponent(
		&Path,
		&Component,
		&ComponentLength,
		&LastComponent))
	{
		if (FsIsDotComponent(Component, ComponentLength))
		{
			continue;
		}

		if (FsIsDotDotComponent(Component, ComponentLength) ||
			(UdfFileEntryType(&CurrentNode) != UDF_FILE_TYPE_DIRECTORY))
		{
			Status = EFI_STATUS_INVALID_PARAMETER;
			goto cleanup;
		}

		memset(&FoundNode, 0, sizeof(FoundNode));
		Status = UdfFindEntry(
			&Context,
			&CurrentNode,
			Component,
			ComponentLength,
			&FoundNode);
		if (EFI_ERROR(Status))
		{
			goto cleanup;
		}

		if (!LastComponent &&
			(UdfFileEntryType(&FoundNode) != UDF_FILE_TYPE_DIRECTORY))
		{
			UdfFreeNode(&FoundNode);
			Status = EFI_STATUS_NOT_FOUND;
			goto cleanup;
		}

		UdfFreeNode(&CurrentNode);
		CurrentNode = FoundNode;
	}

	if (UdfFileEntryType(&CurrentNode) == UDF_FILE_TYPE_DIRECTORY)
	{
		Status = EFI_STATUS_NOT_FOUND;
		goto cleanup;
	}

	Status = UdfReadNodeData(
		&Context,
		&CurrentNode,
		FS_MAX_FILE_SIZE,
		FileBuffer,
		FileSize);
	if (!EFI_ERROR(Status))
	{
		SerialPrint(
			L"UDF file=%s size=0x%016llX\n",
			FilePath,
			(UINT64)*FileSize);
	}

cleanup:
	UdfFreeNode(&CurrentNode);
	return Status;
}
