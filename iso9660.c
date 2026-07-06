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
#include "iso9660.h"
#include "mm.h"
#include "print.h"

#define ISO_BLOCK_SIZE 2048U
#define ISO_FIRST_VOLUME_DESCRIPTOR 16U
#define ISO_MAX_VOLUME_DESCRIPTORS 240U
#define ISO_MAX_EXTENTS 32U
#define ISO_DIRECTORY_FLAG 0x02
#define ISO_MULTI_EXTENT_FLAG 0x80
#define ISO_SUSP_HEADER_SIZE 4U
#define ISO_SUSP_MAX_AREA_SIZE (64U * 1024U)
#define ISO_SUSP_MAX_CE_HOPS 32U
#define ISO_ROCK_RIDGE_NAME_MAX_BYTES (MAX_PATH * 4U)

typedef struct _ISO_EXTENT
{
	UINT64 Offset;
	UINT32 Length;
} ISO_EXTENT;

typedef struct _ISO_NODE
{
	ISO_EXTENT Extents[ISO_MAX_EXTENTS];
	UINTN ExtentCount;
	UINT64 Size;
	BOOLEAN Directory;
} ISO_NODE;

typedef struct _ISO_SUSP_RESULT
{
	UINT8 Name[ISO_ROCK_RIDGE_NAME_MAX_BYTES];
	UINTN NameLength;
	BOOLEAN HaveExtensionReference;
	BOOLEAN HaveName;
	BOOLEAN NameContinues;
} ISO_SUSP_RESULT;

static EFI_STATUS IsoNodeFromRecord(const UINT8* Record, ISO_NODE* Node)
{
	UINT32 ExtentBlock;
	UINT32 ExtentLength;
	UINT8 ExtendedAttributeBlocks;

	if (Record[0] < 34)
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	ExtentBlock = FsReadLe32(Record + 2);
	ExtentLength = FsReadLe32(Record + 10);
	ExtendedAttributeBlocks = Record[1];
	memset(Node, 0, sizeof(*Node));
	Node->ExtentCount = 1;
	Node->Extents[0].Offset =
		((UINT64)ExtentBlock + ExtendedAttributeBlocks) * ISO_BLOCK_SIZE;
	Node->Extents[0].Length = ExtentLength;
	Node->Size = ExtentLength;
	Node->Directory = (Record[25] & ISO_DIRECTORY_FLAG) ? TRUE : FALSE;
	return EFI_SUCCESS;
}

static EFI_STATUS IsoGetSystemUseArea(
	const UINT8* Record,
	UINTN RecordLength,
	UINT8 SuspSkip,
	const UINT8** SystemUse,
	UINTN* SystemUseLength)
{
	UINTN NameLength;
	UINTN Offset;

	if ((RecordLength < 34) || (Record[0] != RecordLength))
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	NameLength = Record[32];
	if (NameLength > RecordLength - 33)
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	Offset = 33 + NameLength;
	if ((NameLength & 1) == 0)
	{
		Offset++;
	}

	if ((Offset > RecordLength) ||
		(SuspSkip > RecordLength - Offset))
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	Offset += SuspSkip;
	*SystemUse = Record + Offset;
	*SystemUseLength = RecordLength - Offset;
	return EFI_SUCCESS;
}

static EFI_STATUS IsoParseSusp(
	EFI_DISK* Disk,
	const UINT8* InitialArea,
	UINTN InitialAreaLength,
	BOOLEAN CollectName,
	ISO_SUSP_RESULT* Result)
{
	const UINT8* Area;
	const UINT8* Entry;
	VOID* AreaAllocation;
	VOID* NewAllocation;
	UINTN AreaLength;
	UINTN EntryLength;
	UINTN Offset;
	UINTN HopCount;
	UINT32 CeBlock;
	UINT32 CeOffset;
	UINT32 CeLength;
	UINT64 CePhysicalOffset;
	UINT8 NameFlags;
	BOOLEAN HaveCe;
	EFI_STATUS Status;

	memset(Result, 0, sizeof(*Result));
	if (InitialAreaLength == 0)
	{
		return EFI_SUCCESS;
	}

	Area = InitialArea;
	AreaLength = InitialAreaLength;
	AreaAllocation = NULL;
	HopCount = 0;
	for (;;)
	{
		Offset = 0;
		HaveCe = FALSE;
		CeBlock = 0;
		CeOffset = 0;
		CeLength = 0;
		while (Offset + ISO_SUSP_HEADER_SIZE <= AreaLength)
		{
			Entry = Area + Offset;
			EntryLength = Entry[2];
			if (EntryLength == 0)
			{
				break;
			}

			if ((EntryLength < ISO_SUSP_HEADER_SIZE) ||
				(EntryLength > AreaLength - Offset))
			{
				Status = EFI_STATUS_UNSUPPORTED;
				goto cleanup;
			}

			if ((Entry[0] == 'S') && (Entry[1] == 'T'))
			{
				break;
			}

			if ((Entry[0] == 'C') && (Entry[1] == 'E'))
			{
				if (HaveCe || (EntryLength < 28))
				{
					Status = EFI_STATUS_UNSUPPORTED;
					goto cleanup;
				}

				CeBlock = FsReadLe32(Entry + 4);
				CeOffset = FsReadLe32(Entry + 12);
				CeLength = FsReadLe32(Entry + 20);
				HaveCe = TRUE;
			}
			else if ((Entry[0] == 'E') && (Entry[1] == 'R'))
			{
				if (EntryLength < 8)
				{
					Status = EFI_STATUS_UNSUPPORTED;
					goto cleanup;
				}

				Result->HaveExtensionReference = TRUE;
			}
			else if (CollectName &&
				(Entry[0] == 'N') && (Entry[1] == 'M'))
			{
				if (EntryLength < 5)
				{
					Status = EFI_STATUS_UNSUPPORTED;
					goto cleanup;
				}

				NameFlags = Entry[4];
				if (Result->HaveName && !Result->NameContinues)
				{
					Status = EFI_STATUS_UNSUPPORTED;
					goto cleanup;
				}

				if ((NameFlags & 0x02) != 0)
				{
					Result->Name[0] = '.';
					Result->NameLength = 1;
				}
				else if ((NameFlags & 0x04) != 0)
				{
					Result->Name[0] = '.';
					Result->Name[1] = '.';
					Result->NameLength = 2;
				}
				else if ((NameFlags & 0x08) != 0)
				{
					Result->Name[0] = '/';
					Result->NameLength = 1;
				}
				else
				{
					if ((UINTN)(EntryLength - 5) >
						sizeof(Result->Name) - Result->NameLength)
					{
						Status = EFI_STATUS_BUFFER_TOO_SMALL;
						goto cleanup;
					}

					memcpy(
						Result->Name + Result->NameLength,
						Entry + 5,
						EntryLength - 5);
					Result->NameLength += EntryLength - 5;
				}

				Result->HaveName = TRUE;
				Result->NameContinues =
					((NameFlags & 0x01) != 0) ? TRUE : FALSE;
			}

			Offset += EntryLength;
		}

		if (!HaveCe)
		{
			break;
		}

		HopCount++;
		if ((HopCount > ISO_SUSP_MAX_CE_HOPS) ||
			(CeLength < ISO_SUSP_HEADER_SIZE) ||
			(CeLength > ISO_SUSP_MAX_AREA_SIZE))
		{
			Status = EFI_STATUS_UNSUPPORTED;
			goto cleanup;
		}

		CePhysicalOffset = (UINT64)CeBlock * ISO_BLOCK_SIZE;
		if (CePhysicalOffset > (UINT64)-1 - CeOffset)
		{
			Status = EFI_STATUS_BUFFER_TOO_SMALL;
			goto cleanup;
		}

		CePhysicalOffset += CeOffset;
		Status = EfiAllocatePool(CeLength, &NewAllocation);
		if (EFI_ERROR(Status))
		{
			goto cleanup;
		}

		Status = EfiDiskRead(
			Disk,
			CePhysicalOffset,
			CeLength,
			NewAllocation);
		if (EFI_ERROR(Status))
		{
			EfiFreePool(NewAllocation);
			goto cleanup;
		}

		EfiFreePool(AreaAllocation);
		AreaAllocation = NewAllocation;
		Area = (const UINT8*)AreaAllocation;
		AreaLength = CeLength;
	}

	if (Result->NameContinues)
	{
		Status = EFI_STATUS_UNSUPPORTED;
		goto cleanup;
	}

	Status = EFI_SUCCESS;

cleanup:
	EfiFreePool(AreaAllocation);
	return Status;
}

static EFI_STATUS Utf8ToUtf16(
	const UINT8* Input,
	UINTN InputLength,
	CHAR16* Output,
	UINTN OutputCapacity,
	UINTN* OutputLength)
{
	UINT32 CodePoint;
	UINT32 MinimumCodePoint;
	UINTN ContinuationCount;
	UINTN InputIndex;
	UINTN OutputIndex;
	UINTN Index;
	UINT8 Byte;

	InputIndex = 0;
	OutputIndex = 0;
	while (InputIndex < InputLength)
	{
		Byte = Input[InputIndex];
		if (Byte < 0x80)
		{
			CodePoint = Byte;
			ContinuationCount = 0;
			MinimumCodePoint = 0;
		}
		else if ((Byte & 0xE0) == 0xC0)
		{
			CodePoint = Byte & 0x1F;
			ContinuationCount = 1;
			MinimumCodePoint = 0x80;
		}
		else if ((Byte & 0xF0) == 0xE0)
		{
			CodePoint = Byte & 0x0F;
			ContinuationCount = 2;
			MinimumCodePoint = 0x800;
		}
		else if ((Byte & 0xF8) == 0xF0)
		{
			CodePoint = Byte & 0x07;
			ContinuationCount = 3;
			MinimumCodePoint = 0x10000;
		}
		else
		{
			return EFI_STATUS_UNSUPPORTED;
		}

		if (ContinuationCount > InputLength - InputIndex - 1)
		{
			return EFI_STATUS_UNSUPPORTED;
		}

		for (Index = 0; Index < ContinuationCount; Index++)
		{
			Byte = Input[InputIndex + Index + 1];
			if ((Byte & 0xC0) != 0x80)
			{
				return EFI_STATUS_UNSUPPORTED;
			}

			CodePoint = (CodePoint << 6) | (Byte & 0x3F);
		}

		if ((CodePoint < MinimumCodePoint) ||
			(CodePoint > 0x10FFFF) ||
			((CodePoint >= 0xD800) && (CodePoint <= 0xDFFF)))
		{
			return EFI_STATUS_UNSUPPORTED;
		}

		if (CodePoint <= 0xFFFF)
		{
			if (OutputIndex >= OutputCapacity)
			{
				return EFI_STATUS_BUFFER_TOO_SMALL;
			}

			Output[OutputIndex++] = (CHAR16)CodePoint;
		}
		else
		{
			if ((OutputIndex > OutputCapacity) ||
				(OutputCapacity - OutputIndex < 2))
			{
				return EFI_STATUS_BUFFER_TOO_SMALL;
			}

			CodePoint -= 0x10000;
			Output[OutputIndex] =
				(CHAR16)(0xD800 | (CodePoint >> 10));
			Output[OutputIndex + 1] =
				(CHAR16)(0xDC00 | (CodePoint & 0x3FF));
			OutputIndex += 2;
		}

		InputIndex += ContinuationCount + 1;
	}

	*OutputLength = OutputIndex;
	return EFI_SUCCESS;
}

__declspec(noinline) static EFI_STATUS IsoDetectRockRidge(
	EFI_DISK* Disk,
	ISO_NODE* Root,
	BOOLEAN* RockRidge,
	UINT8* SuspSkip)
{
	UINT8 DirectoryBlock[ISO_BLOCK_SIZE];
	const UINT8* SystemUse;
	UINTN RecordLength;
	UINTN SystemUseLength;
	ISO_SUSP_RESULT Result;
	EFI_STATUS Status;

	*RockRidge = FALSE;
	*SuspSkip = 0;
	if ((Root->ExtentCount == 0) ||
		(Root->Extents[0].Length < 34))
	{
		return EFI_STATUS_UNSUPPORTED;
	}

	Status = EfiDiskRead(
		Disk,
		Root->Extents[0].Offset,
		sizeof(DirectoryBlock),
		DirectoryBlock);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	RecordLength = DirectoryBlock[0];
	Status = IsoGetSystemUseArea(
		DirectoryBlock,
		RecordLength,
		0,
		&SystemUse,
		&SystemUseLength);
	if (EFI_ERROR(Status) || (SystemUseLength < 7))
	{
		return EFI_SUCCESS;
	}

	if ((SystemUse[0] != 'S') || (SystemUse[1] != 'P') ||
		(SystemUse[2] < 7) || (SystemUse[3] != 1) ||
		(SystemUse[4] != 0xBE) || (SystemUse[5] != 0xEF))
	{
		return EFI_SUCCESS;
	}

	*SuspSkip = SystemUse[6];
	Status = IsoParseSusp(
		Disk,
		SystemUse,
		SystemUseLength,
		FALSE,
		&Result);
	if (!EFI_ERROR(Status) && Result.HaveExtensionReference)
	{
		*RockRidge = TRUE;
	}

	return Status;
}

__declspec(noinline) static EFI_STATUS IsoRockRidgeNameEquals(
	EFI_DISK* Disk,
	const UINT8* Record,
	UINTN RecordLength,
	UINT8 SuspSkip,
	const CHAR16* Component,
	UINTN ComponentLength,
	BOOLEAN* HaveName,
	BOOLEAN* Matches)
{
	CHAR16 ConvertedName[MAX_PATH];
	const UINT8* SystemUse;
	UINTN ConvertedLength;
	UINTN SystemUseLength;
	ISO_SUSP_RESULT Result;
	EFI_STATUS Status;

	*HaveName = FALSE;
	*Matches = FALSE;
	Status = IsoGetSystemUseArea(
		Record,
		RecordLength,
		SuspSkip,
		&SystemUse,
		&SystemUseLength);
	if (EFI_ERROR(Status) || (SystemUseLength == 0))
	{
		return EFI_ERROR(Status) ? Status : EFI_SUCCESS;
	}

	Status = IsoParseSusp(
		Disk,
		SystemUse,
		SystemUseLength,
		TRUE,
		&Result);
	if (EFI_ERROR(Status) || !Result.HaveName)
	{
		return Status;
	}

	Status = Utf8ToUtf16(
		Result.Name,
		Result.NameLength,
		ConvertedName,
		MAX_PATH,
		&ConvertedLength);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	*HaveName = TRUE;
	*Matches = FsUnicodeNameEquals(
		ConvertedName,
		ConvertedLength,
		Component,
		ComponentLength);
	return EFI_SUCCESS;
}

static EFI_STATUS IsoMount(
	EFI_DISK* Disk,
	ISO_NODE* Root,
	BOOLEAN* Joliet,
	BOOLEAN* RockRidge,
	UINT8* SuspSkip)
{
	UINT8 Descriptor[ISO_BLOCK_SIZE];
	ISO_NODE PrimaryRoot;
	ISO_NODE JolietRoot;
	BOOLEAN HavePrimary;
	BOOLEAN HaveJoliet;
	BOOLEAN HaveRockRidge;
	UINT8 PrimarySuspSkip;
	UINTN DescriptorIndex;
	EFI_STATUS Status;

	memset(&PrimaryRoot, 0, sizeof(PrimaryRoot));
	memset(&JolietRoot, 0, sizeof(JolietRoot));
	HavePrimary = FALSE;
	HaveJoliet = FALSE;
	HaveRockRidge = FALSE;
	PrimarySuspSkip = 0;
	for (DescriptorIndex = 0;
		DescriptorIndex < ISO_MAX_VOLUME_DESCRIPTORS;
		DescriptorIndex++)
	{
		Status = EfiDiskRead(
			Disk,
			(UINT64)(ISO_FIRST_VOLUME_DESCRIPTOR + DescriptorIndex) *
				ISO_BLOCK_SIZE,
			sizeof(Descriptor),
			Descriptor);
		if (EFI_ERROR(Status))
		{
			return EFI_STATUS_UNSUPPORTED;
		}

		if (memcmp(Descriptor + 1, "CD001", 5) != 0)
		{
			return EFI_STATUS_UNSUPPORTED;
		}

		if (Descriptor[0] == 1)
		{
			Status = IsoNodeFromRecord(Descriptor + 156, &PrimaryRoot);
			if (EFI_ERROR(Status))
			{
				return EFI_STATUS_UNSUPPORTED;
			}

			HavePrimary = TRUE;
		}
		else if ((Descriptor[0] == 2) &&
			(Descriptor[88] == 0x25) &&
			(Descriptor[89] == 0x2F) &&
			((Descriptor[90] == 0x40) ||
			 (Descriptor[90] == 0x43) ||
			 (Descriptor[90] == 0x45)))
		{
			Status = IsoNodeFromRecord(Descriptor + 156, &JolietRoot);
			if (EFI_ERROR(Status))
			{
				return EFI_STATUS_UNSUPPORTED;
			}

			HaveJoliet = TRUE;
		}
		else if (Descriptor[0] == 255)
		{
			break;
		}
	}

	if (HavePrimary)
	{
		Status = IsoDetectRockRidge(
			Disk,
			&PrimaryRoot,
			&HaveRockRidge,
			&PrimarySuspSkip);
		if (EFI_ERROR(Status))
		{
			return Status;
		}
	}

	if (HaveRockRidge)
	{
		*Root = PrimaryRoot;
		*Joliet = FALSE;
		*RockRidge = TRUE;
		*SuspSkip = PrimarySuspSkip;
		return EFI_SUCCESS;
	}

	if (HaveJoliet)
	{
		*Root = JolietRoot;
		*Joliet = TRUE;
		*RockRidge = FALSE;
		*SuspSkip = 0;
		return EFI_SUCCESS;
	}

	if (HavePrimary)
	{
		*Root = PrimaryRoot;
		*Joliet = FALSE;
		*RockRidge = FALSE;
		*SuspSkip = 0;
		return EFI_SUCCESS;
	}

	return EFI_STATUS_UNSUPPORTED;
}

static EFI_STATUS IsoReadNode(
	EFI_DISK* Disk,
	ISO_NODE* Node,
	UINT64 SizeLimit,
	VOID** Buffer,
	UINTN* BufferSize)
{
	UINT8* Destination;
	UINTN ExtentIndex;
	UINTN Size;
	UINTN Written;
	EFI_STATUS Status;

	if ((Node->Size == 0) || (Node->Size > SizeLimit) ||
		(Node->Size > (UINT64)((UINTN)-1)))
	{
		return EFI_STATUS_BUFFER_TOO_SMALL;
	}

	Size = (UINTN)Node->Size;
	*Buffer = NULL;
	*BufferSize = 0;
	Status = EfiAllocatePool(Size, Buffer);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	Destination = (UINT8*)*Buffer;
	Written = 0;
	for (ExtentIndex = 0; ExtentIndex < Node->ExtentCount; ExtentIndex++)
	{
		if (Node->Extents[ExtentIndex].Length > Size - Written)
		{
			Status = EFI_STATUS_UNSUPPORTED;
			goto cleanup;
		}

		Status = EfiDiskRead(
			Disk,
			Node->Extents[ExtentIndex].Offset,
			Node->Extents[ExtentIndex].Length,
			Destination);
		if (EFI_ERROR(Status))
		{
			goto cleanup;
		}

		Destination += Node->Extents[ExtentIndex].Length;
		Written += Node->Extents[ExtentIndex].Length;
	}

	if (Written != Size)
	{
		Status = EFI_STATUS_UNSUPPORTED;
		goto cleanup;
	}

	*BufferSize = Size;
	return EFI_SUCCESS;

cleanup:
	EfiFreePool(*Buffer);
	*Buffer = NULL;
	return Status;
}

static BOOLEAN IsoRecordNameEquals(
	const UINT8* Name,
	UINTN NameLength,
	BOOLEAN Joliet,
	const CHAR16* Component,
	UINTN ComponentLength)
{
	CHAR16 ConvertedName[256];
	UINTN CharacterCount;
	UINTN Index;

	if (Joliet)
	{
		if ((NameLength & 1) != 0)
		{
			return FALSE;
		}

		CharacterCount = NameLength >> 1;
		if (CharacterCount > 255)
		{
			return FALSE;
		}

		for (Index = 0; Index < CharacterCount; Index++)
		{
			ConvertedName[Index] =
				(CHAR16)(((CHAR16)Name[Index * 2] << 8) |
					Name[Index * 2 + 1]);
		}
	}
	else
	{
		CharacterCount = NameLength;
		if (CharacterCount > 255)
		{
			return FALSE;
		}

		for (Index = 0; Index < CharacterCount; Index++)
		{
			ConvertedName[Index] = Name[Index];
		}
	}

	for (Index = 0; Index < CharacterCount; Index++)
	{
		if (ConvertedName[Index] == ';')
		{
			CharacterCount = Index;
			break;
		}
	}

	if ((CharacterCount != 0) &&
		(ConvertedName[CharacterCount - 1] == '.'))
	{
		CharacterCount--;
	}

	return FsUnicodeNameEquals(
		ConvertedName,
		CharacterCount,
		Component,
		ComponentLength);
}

static EFI_STATUS IsoFindEntry(
	EFI_DISK* Disk,
	ISO_NODE* Directory,
	BOOLEAN Joliet,
	BOOLEAN RockRidge,
	UINT8 SuspSkip,
	const CHAR16* Component,
	UINTN ComponentLength,
	ISO_NODE* FoundNode)
{
	UINT8* DirectoryData;
	UINT8* FirstRecord;
	UINT8* Record;
	UINT8* NextRecord;
	UINTN DirectorySize;
	UINTN Offset;
	UINTN NextOffset;
	UINTN RecordLength;
	UINTN NameLength;
	BOOLEAN HaveRockRidgeName;
	BOOLEAN NameMatches;
	ISO_NODE Node;
	EFI_STATUS Status;

	DirectorySize = 0;
	Status = IsoReadNode(
		Disk,
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
	while (Offset < DirectorySize)
	{
		Record = DirectoryData + Offset;
		RecordLength = Record[0];
		if (RecordLength == 0)
		{
			NextOffset = (Offset + ISO_BLOCK_SIZE) &
				~((UINTN)ISO_BLOCK_SIZE - 1);
			if (NextOffset <= Offset)
			{
				Status = EFI_STATUS_UNSUPPORTED;
				break;
			}

			Offset = NextOffset;
			continue;
		}

		if ((RecordLength < 34) ||
			(RecordLength > DirectorySize - Offset) ||
			(RecordLength > ISO_BLOCK_SIZE -
				(Offset & (ISO_BLOCK_SIZE - 1))))
		{
			Status = EFI_STATUS_UNSUPPORTED;
			break;
		}

		NameLength = Record[32];
		if ((NameLength > RecordLength - 33) ||
			((NameLength == 1) &&
			 ((Record[33] == 0) || (Record[33] == 1))))
		{
			Offset += RecordLength;
			continue;
		}

		HaveRockRidgeName = FALSE;
		NameMatches = FALSE;
		if (RockRidge)
		{
			Status = IsoRockRidgeNameEquals(
				Disk,
				Record,
				RecordLength,
				SuspSkip,
				Component,
				ComponentLength,
				&HaveRockRidgeName,
				&NameMatches);
			if (EFI_ERROR(Status))
			{
				break;
			}
		}

		if (!HaveRockRidgeName)
		{
			NameMatches = IsoRecordNameEquals(
				Record + 33,
				NameLength,
				Joliet,
				Component,
				ComponentLength);
		}

		if (!NameMatches)
		{
			Offset += RecordLength;
			continue;
		}

		FirstRecord = Record;
		Status = IsoNodeFromRecord(Record, &Node);
		if (EFI_ERROR(Status))
		{
			break;
		}

		while ((Record[25] & ISO_MULTI_EXTENT_FLAG) != 0)
		{
			NextOffset = Offset + RecordLength;
			while ((NextOffset < DirectorySize) &&
				(DirectoryData[NextOffset] == 0))
			{
				NextOffset = (NextOffset + ISO_BLOCK_SIZE) &
					~((UINTN)ISO_BLOCK_SIZE - 1);
			}

			if ((NextOffset >= DirectorySize) ||
				(Node.ExtentCount >= ISO_MAX_EXTENTS))
			{
				Status = EFI_STATUS_UNSUPPORTED;
				break;
			}

			NextRecord = DirectoryData + NextOffset;
			RecordLength = NextRecord[0];
			if ((RecordLength < 34) ||
				(RecordLength > DirectorySize - NextOffset) ||
				(NextRecord[32] > RecordLength - 33) ||
				(NextRecord[32] != FirstRecord[32]) ||
				(memcmp(
					NextRecord + 33,
					FirstRecord + 33,
					FirstRecord[32]) != 0))
			{
				Status = EFI_STATUS_UNSUPPORTED;
				break;
			}

			Node.Extents[Node.ExtentCount].Offset =
				((UINT64)FsReadLe32(NextRecord + 2) + NextRecord[1]) *
					ISO_BLOCK_SIZE;
			Node.Extents[Node.ExtentCount].Length =
				FsReadLe32(NextRecord + 10);
			if (Node.Size > ((UINT64)-1 -
				Node.Extents[Node.ExtentCount].Length))
			{
				Status = EFI_STATUS_BUFFER_TOO_SMALL;
				break;
			}

			Node.Size += Node.Extents[Node.ExtentCount].Length;
			Node.ExtentCount++;
			Offset = NextOffset;
			Record = NextRecord;
		}

		if (EFI_ERROR(Status))
		{
			break;
		}

		*FoundNode = Node;
		Status = EFI_SUCCESS;
		break;
	}

	EfiFreePool(DirectoryData);
	return Status;
}

EFI_STATUS Iso9660ReadFile(
	EFI_DISK* Disk,
	CHAR16* FilePath,
	VOID** FileBuffer,
	UINTN* FileSize)
{
	ISO_NODE CurrentNode;
	ISO_NODE FoundNode;
	CHAR16* Path;
	CHAR16* Component;
	UINTN ComponentLength;
	BOOLEAN LastComponent;
	BOOLEAN Joliet;
	BOOLEAN RockRidge;
	UINT8 SuspSkip;
	EFI_STATUS Status;

	Status = IsoMount(
		Disk,
		&CurrentNode,
		&Joliet,
		&RockRidge,
		&SuspSkip);
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
			!CurrentNode.Directory)
		{
			return EFI_STATUS_INVALID_PARAMETER;
		}

		Status = IsoFindEntry(
			Disk,
			&CurrentNode,
			Joliet,
			RockRidge,
			SuspSkip,
			Component,
			ComponentLength,
			&FoundNode);
		if (EFI_ERROR(Status))
		{
			return Status;
		}

		if (!LastComponent && !FoundNode.Directory)
		{
			return EFI_STATUS_NOT_FOUND;
		}

		CurrentNode = FoundNode;
	}

	if (CurrentNode.Directory)
	{
		return EFI_STATUS_NOT_FOUND;
	}

	Status = IsoReadNode(
		Disk,
		&CurrentNode,
		FS_MAX_FILE_SIZE,
		FileBuffer,
		FileSize);
	if (!EFI_ERROR(Status))
	{
		SerialPrint(
			L"ISO9660%s file=%s size=0x%016llX\n",
			RockRidge ? L"/RockRidge" :
				(Joliet ? L"/Joliet" : L""),
			FilePath,
			(UINT64)*FileSize);
	}

	return Status;
}
