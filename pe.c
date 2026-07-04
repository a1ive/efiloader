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

#include "pe.h"

static inline BOOLEAN IsRangeValid(UINTN Offset, UINTN Length, UINTN BufferSize)
{
	return (Offset <= BufferSize) && (Length <= (BufferSize - Offset));
}

static NTSTATUS GetOptionalHeader(
	const VOID* ImageBase,
	UINTN ImageSize,
	const UINT8** OptionalHeader,
	UINT16* OptionalHeaderSize,
	UINT16* OptionalHeaderMagic)
{
	const IMAGE_DOS_HEADER_MIN* DosHeader;
	const IMAGE_FILE_HEADER_MIN* FileHeader;
	const UINT8* Bytes;
	UINTN NtHeadersOffset;
	UINTN FileHeaderOffset;
	UINTN OptionalHeaderOffset;

	if ((ImageBase == NULL) || (OptionalHeader == NULL) || (OptionalHeaderSize == NULL) || (OptionalHeaderMagic == NULL))
	{
		return STATUS_INVALID_PARAMETER;
	}

	*OptionalHeader = NULL;
	*OptionalHeaderSize = 0;
	*OptionalHeaderMagic = 0;

	if (!IsRangeValid(0, sizeof(*DosHeader), ImageSize))
	{
		return STATUS_INVALID_PARAMETER;
	}

	Bytes = (const UINT8*)ImageBase;
	DosHeader = (const IMAGE_DOS_HEADER_MIN*)Bytes;
	if ((DosHeader->e_magic != IMAGE_DOS_SIGNATURE) || (DosHeader->e_lfanew < 0))
	{
		return STATUS_INVALID_PARAMETER;
	}

	NtHeadersOffset = (UINTN)DosHeader->e_lfanew;
	if (!IsRangeValid(NtHeadersOffset, sizeof(UINT32) + sizeof(*FileHeader), ImageSize))
	{
		return STATUS_INVALID_PARAMETER;
	}

	if (*(const UINT32*)(Bytes + NtHeadersOffset) != IMAGE_NT_SIGNATURE)
	{
		return STATUS_INVALID_PARAMETER;
	}

	FileHeaderOffset = NtHeadersOffset + sizeof(UINT32);
	FileHeader = (const IMAGE_FILE_HEADER_MIN*)(Bytes + FileHeaderOffset);
	OptionalHeaderOffset = FileHeaderOffset + sizeof(*FileHeader);
	if (!IsRangeValid(OptionalHeaderOffset, FileHeader->SizeOfOptionalHeader, ImageSize)
		|| (FileHeader->SizeOfOptionalHeader < sizeof(UINT16)))
	{
		return STATUS_INVALID_PARAMETER;
	}

	*OptionalHeader = Bytes + OptionalHeaderOffset;
	*OptionalHeaderSize = FileHeader->SizeOfOptionalHeader;
	*OptionalHeaderMagic = *(const UINT16*)*OptionalHeader;
	return STATUS_SUCCESS;
}

static NTSTATUS GetDataDirectory(
	const VOID* ImageBase,
	UINTN ImageSize,
	UINTN DirectoryIndex,
	IMAGE_DATA_DIRECTORY_MIN* DataDirectory)
{
	const UINT8* OptionalHeader;
	UINT16 OptionalHeaderSize;
	UINT16 OptionalHeaderMagic;
	UINTN NumberOfDirectoriesOffset;
	UINTN DataDirectoryOffset;
	UINTN RequestedDirectoryOffset;
	UINT32 NumberOfDirectories;
	NTSTATUS Status;

	if (DataDirectory == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}

	DataDirectory->VirtualAddress = 0;
	DataDirectory->Size = 0;
	Status = GetOptionalHeader(ImageBase, ImageSize, &OptionalHeader, &OptionalHeaderSize, &OptionalHeaderMagic);
	if (Status != STATUS_SUCCESS)
	{
		return Status;
	}

	if (OptionalHeaderMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
	{
		NumberOfDirectoriesOffset = PE32_NUMBER_OF_RVA_AND_SIZES_OFFSET;
		DataDirectoryOffset = PE32_DATA_DIRECTORY_OFFSET;
	}
	else if (OptionalHeaderMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{
		NumberOfDirectoriesOffset = PE32_PLUS_NUMBER_OF_RVA_AND_SIZES_OFFSET;
		DataDirectoryOffset = PE32_PLUS_DATA_DIRECTORY_OFFSET;
	}
	else
	{
		return STATUS_NOT_SUPPORTED;
	}

	if (!IsRangeValid(NumberOfDirectoriesOffset, sizeof(NumberOfDirectories), OptionalHeaderSize))
	{
		return STATUS_INVALID_PARAMETER;
	}

	NumberOfDirectories = *(const UINT32*)(OptionalHeader + NumberOfDirectoriesOffset);
	if (DirectoryIndex >= NumberOfDirectories)
	{
		return STATUS_NOT_FOUND;
	}

	RequestedDirectoryOffset = DataDirectoryOffset + (DirectoryIndex * sizeof(*DataDirectory));
	if (!IsRangeValid(RequestedDirectoryOffset, sizeof(*DataDirectory), OptionalHeaderSize))
	{
		return STATUS_INVALID_PARAMETER;
	}

	*DataDirectory = *(const IMAGE_DATA_DIRECTORY_MIN*)(OptionalHeader + RequestedDirectoryOffset);
	if ((DataDirectory->VirtualAddress == 0) || (DataDirectory->Size == 0))
	{
		return STATUS_NOT_FOUND;
	}

	return STATUS_SUCCESS;
}

static NTSTATUS FindResDirEntry(
	const UINT8* ResourceDirectoryBase,
	UINTN ResourceDirectorySize,
	UINTN DirectoryOffset,
	UINT16 Id,
	BOOLEAN ExpectDirectory,
	const IMAGE_RESOURCE_DIRECTORY_ENTRY_MIN** FoundEntry)
{
	const IMAGE_RESOURCE_DIRECTORY_MIN* Directory;
	const IMAGE_RESOURCE_DIRECTORY_ENTRY_MIN* Entries;
	const IMAGE_RESOURCE_DIRECTORY_ENTRY_MIN* Entry;
	UINT32 EntryCount;
	UINT32 Index;
	UINTN EntriesOffset;
	UINTN EntriesSize;
	BOOLEAN IsDirectory;

	if ((ResourceDirectoryBase == NULL) || (FoundEntry == NULL))
	{
		return STATUS_INVALID_PARAMETER;
	}

	*FoundEntry = NULL;
	if (!IsRangeValid(DirectoryOffset, sizeof(*Directory), ResourceDirectorySize))
	{
		return STATUS_INVALID_PARAMETER;
	}

	Directory = (const IMAGE_RESOURCE_DIRECTORY_MIN*)(ResourceDirectoryBase + DirectoryOffset);
	EntryCount = (UINT32)Directory->NumberOfNamedEntries + Directory->NumberOfIdEntries;
	EntriesOffset = DirectoryOffset + sizeof(*Directory);
	EntriesSize = (UINTN)EntryCount * sizeof(*Entries);
	if (!IsRangeValid(EntriesOffset, EntriesSize, ResourceDirectorySize))
	{
		return STATUS_INVALID_PARAMETER;
	}

	Entries = (const IMAGE_RESOURCE_DIRECTORY_ENTRY_MIN*)(ResourceDirectoryBase + EntriesOffset);
	for (Index = Directory->NumberOfNamedEntries; Index < EntryCount; Index++)
	{
		Entry = &Entries[Index];
		if ((Entry->Name & IMAGE_RESOURCE_NAME_IS_STRING) != 0)
		{
			continue;
		}

		if ((Entry->Name & 0xFFFF0000UL) != 0 || (UINT16)Entry->Name != Id)
		{
			continue;
		}

		IsDirectory = ((Entry->OffsetToData & IMAGE_RESOURCE_DATA_IS_DIRECTORY) != 0);
		if (IsDirectory != ExpectDirectory)
		{
			return STATUS_INVALID_PARAMETER;
		}

		*FoundEntry = Entry;
		return STATUS_SUCCESS;
	}

	return STATUS_NOT_FOUND;
}

static NTSTATUS FindFirstResDataEntry(
	const UINT8* ResourceDirectoryBase,
	UINTN ResourceDirectorySize,
	UINTN DirectoryOffset,
	const IMAGE_RESOURCE_DATA_ENTRY_MIN** FoundDataEntry)
{
	const IMAGE_RESOURCE_DIRECTORY_MIN* Directory;
	const IMAGE_RESOURCE_DIRECTORY_ENTRY_MIN* Entries;
	const IMAGE_RESOURCE_DIRECTORY_ENTRY_MIN* Entry;
	UINT32 EntryCount;
	UINT32 Index;
	UINTN EntriesOffset;
	UINTN EntriesSize;
	UINTN DataEntryOffset;

	if ((ResourceDirectoryBase == NULL) || (FoundDataEntry == NULL))
	{
		return STATUS_INVALID_PARAMETER;
	}

	*FoundDataEntry = NULL;
	if (!IsRangeValid(DirectoryOffset, sizeof(*Directory), ResourceDirectorySize))
	{
		return STATUS_INVALID_PARAMETER;
	}

	Directory = (const IMAGE_RESOURCE_DIRECTORY_MIN*)(ResourceDirectoryBase + DirectoryOffset);
	EntryCount = (UINT32)Directory->NumberOfNamedEntries + Directory->NumberOfIdEntries;
	EntriesOffset = DirectoryOffset + sizeof(*Directory);
	EntriesSize = (UINTN)EntryCount * sizeof(*Entries);
	if (!IsRangeValid(EntriesOffset, EntriesSize, ResourceDirectorySize))
	{
		return STATUS_INVALID_PARAMETER;
	}

	Entries = (const IMAGE_RESOURCE_DIRECTORY_ENTRY_MIN*)(ResourceDirectoryBase + EntriesOffset);
	for (Index = Directory->NumberOfNamedEntries; Index < EntryCount; Index++)
	{
		Entry = &Entries[Index];
		if ((Entry->Name & IMAGE_RESOURCE_NAME_IS_STRING) != 0)
		{
			continue;
		}

		if ((Entry->OffsetToData & IMAGE_RESOURCE_DATA_IS_DIRECTORY) != 0)
		{
			return STATUS_INVALID_PARAMETER;
		}

		DataEntryOffset = (UINTN)(Entry->OffsetToData & IMAGE_RESOURCE_OFFSET_MASK);
		if (!IsRangeValid(DataEntryOffset, sizeof(**FoundDataEntry), ResourceDirectorySize))
		{
			return STATUS_INVALID_PARAMETER;
		}

		*FoundDataEntry = (const IMAGE_RESOURCE_DATA_ENTRY_MIN*)(ResourceDirectoryBase + DataEntryOffset);
		return STATUS_SUCCESS;
	}

	return STATUS_NOT_FOUND;
}

NTSTATUS PeGetImageSubsystem(
	const VOID* ImageBase,
	UINTN ImageSize,
	UINT16* Subsystem)
{
	const UINT8* OptionalHeader;
	UINT16 OptionalHeaderSize;
	UINT16 OptionalHeaderMagic;
	UINTN SubsystemOffset;
	NTSTATUS Status;

	if (Subsystem == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}

	*Subsystem = 0;
	Status = GetOptionalHeader(ImageBase, ImageSize, &OptionalHeader, &OptionalHeaderSize, &OptionalHeaderMagic);
	if (Status != STATUS_SUCCESS)
	{
		return Status;
	}

	if (OptionalHeaderMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
	{
		SubsystemOffset = PE32_SUBSYSTEM_OFFSET;
	}
	else if (OptionalHeaderMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{
		SubsystemOffset = PE32_PLUS_SUBSYSTEM_OFFSET;
	}
	else
	{
		return STATUS_NOT_SUPPORTED;
	}

	if (!IsRangeValid(SubsystemOffset, sizeof(*Subsystem), OptionalHeaderSize))
	{
		return STATUS_INVALID_PARAMETER;
	}

	*Subsystem = *(const UINT16*)(OptionalHeader + SubsystemOffset);
	return STATUS_SUCCESS;
}

NTSTATUS PeFindResData(
	const VOID* ImageBase,
	UINTN ImageSize,
	UINT16 TypeId,
	UINT16 NameId,
	const VOID** ResourceData,
	UINT32* ResourceSize)
{
	IMAGE_DATA_DIRECTORY_MIN ResourceDirectory;
	const UINT8* ImageBytes;
	const UINT8* ResourceDirectoryBase;
	const IMAGE_RESOURCE_DIRECTORY_ENTRY_MIN* DirectoryEntry;
	const IMAGE_RESOURCE_DATA_ENTRY_MIN* DataEntry;
	UINTN DirectoryOffset;
	NTSTATUS Status;

	if ((ImageBase == NULL) || (ResourceData == NULL) || (ResourceSize == NULL))
	{
		return STATUS_INVALID_PARAMETER;
	}

	*ResourceData = NULL;
	*ResourceSize = 0;
	Status = GetDataDirectory(ImageBase, ImageSize, IMAGE_DIRECTORY_ENTRY_RESOURCE, &ResourceDirectory);
	if (Status != STATUS_SUCCESS)
	{
		return Status;
	}

	if (!IsRangeValid(ResourceDirectory.VirtualAddress, ResourceDirectory.Size, ImageSize))
	{
		return STATUS_INVALID_PARAMETER;
	}

	ImageBytes = (const UINT8*)ImageBase;
	ResourceDirectoryBase = ImageBytes + ResourceDirectory.VirtualAddress;
	Status = FindResDirEntry(ResourceDirectoryBase, ResourceDirectory.Size, 0, TypeId, TRUE, &DirectoryEntry);
	if (Status != STATUS_SUCCESS)
	{
		return Status;
	}

	DirectoryOffset = (UINTN)(DirectoryEntry->OffsetToData & IMAGE_RESOURCE_OFFSET_MASK);
	Status = FindResDirEntry(ResourceDirectoryBase, ResourceDirectory.Size, DirectoryOffset, NameId, TRUE, &DirectoryEntry);
	if (Status != STATUS_SUCCESS)
	{
		return Status;
	}

	DirectoryOffset = (UINTN)(DirectoryEntry->OffsetToData & IMAGE_RESOURCE_OFFSET_MASK);
	Status = FindFirstResDataEntry(ResourceDirectoryBase, ResourceDirectory.Size, DirectoryOffset, &DataEntry);
	if (Status != STATUS_SUCCESS)
	{
		return Status;
	}

	if (!IsRangeValid(DataEntry->OffsetToData, DataEntry->Size, ImageSize))
	{
		return STATUS_INVALID_PARAMETER;
	}

	*ResourceData = ImageBytes + DataEntry->OffsetToData;
	*ResourceSize = DataEntry->Size;
	return STATUS_SUCCESS;
}
