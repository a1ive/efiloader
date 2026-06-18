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

#include "bootconfig.h"

#define BCD_OPTION_SEARCH_MAX_DEPTH 8
#define BCD_OPTION_MAX_NEXT_OFFSET 0x100000

static BL_BCD_OPTION* BlpGetBootOption(BL_BCD_OPTION* List, UINT32 Type, UINT32 Depth)
{
	UINT32 NextOption;

	if ((List == NULL) || (Depth >= BCD_OPTION_SEARCH_MAX_DEPTH))
	{
		return NULL;
	}

	NextOption = 0;
	do
	{
		BL_BCD_OPTION* Option;
		BL_BCD_OPTION* FoundOption;

		if (NextOption >= BCD_OPTION_MAX_NEXT_OFFSET)
		{
			return NULL;
		}

		Option = (BL_BCD_OPTION*)((UINT8*)List + NextOption);
		if ((Option->Type == Type) && (Option->Empty == 0))
		{
			return Option;
		}

		if (Option->ListOffset != 0)
		{
			FoundOption = BlpGetBootOption(
				(BL_BCD_OPTION*)((UINT8*)Option + Option->ListOffset),
				Type,
				Depth + 1);
			if (FoundOption != NULL)
			{
				return FoundOption;
			}
		}

		NextOption = Option->NextEntryOffset;
	} while (NextOption != 0);

	return NULL;
}

static inline BOOLEAN IsSpace16(CHAR16 Character)
{
	return (Character == ' ') || (Character == '\t') || (Character == '\r') || (Character == '\n');
}

NTSTATUS BlGetLoadedApplicationEntry(BOOT_APPLICATION_PARAMETER_BLOCK* BootParameters, BL_LOADED_APPLICATION_ENTRY* LoadedApplicationEntry)
{
	BL_APPLICATION_ENTRY* ApplicationEntry;

	if (BootParameters->AppEntryOffset == 0)
	{
		return STATUS_INVALID_PARAMETER;
	}

	ApplicationEntry = (BL_APPLICATION_ENTRY*)((UINT8*)BootParameters + BootParameters->AppEntryOffset);

	if ((ApplicationEntry->Flags & BL_APPLICATION_ENTRY_BCD_OPTIONS_INTERNAL) != 0)
	{
		ApplicationEntry->Flags &= ~BL_APPLICATION_ENTRY_BCD_OPTIONS_INTERNAL;
		ApplicationEntry->Flags |= BL_APPLICATION_ENTRY_BCD_OPTIONS_EXTERNAL;
	}

	LoadedApplicationEntry->Flags = ApplicationEntry->Flags;
	LoadedApplicationEntry->EfiGuid = ApplicationEntry->EfiGuid;
	LoadedApplicationEntry->BcdData = &ApplicationEntry->BcdData;

	return STATUS_SUCCESS;
}

NTSTATUS BlGetBootOptionString(BL_BCD_OPTION* List, UINT32 Type, CHAR16* Buffer, UINTN BufferCharacters)
{
	BL_BCD_OPTION* Option;
	CHAR16* Source;
	UINTN SourceCharacters;
	UINTN Start;
	UINTN End;
	UINTN Index;

	if ((Buffer == NULL) || (BufferCharacters == 0))
	{
		return STATUS_INVALID_PARAMETER;
	}

	Buffer[0] = 0;
	if (((Type >> 24) & 0x0F) != BCD_ELEMENT_FORMAT_STRING)
	{
		return STATUS_INVALID_PARAMETER;
	}

	Option = BlpGetBootOption(List, Type, 0);
	if (Option == NULL)
	{
		return STATUS_NOT_FOUND;
	}

	if ((Option->DataOffset == 0) || ((Option->DataSize & 1) != 0))
	{
		return STATUS_INVALID_PARAMETER;
	}

	Source = (CHAR16*)((UINT8*)Option + Option->DataOffset);
	SourceCharacters = Option->DataSize / sizeof(CHAR16);
	Start = 0;
	while ((Start < SourceCharacters) &&
		((Source[Start] == 0) || IsSpace16(Source[Start])))
	{
		Start++;
	}

	End = SourceCharacters;
	while ((End > Start) &&
		((Source[End - 1] == 0) || IsSpace16(Source[End - 1])))
	{
		End--;
	}

	if ((End > Start + 1) &&
		(Source[Start] == '"') &&
		(Source[End - 1] == '"'))
	{
		Start++;
		End--;
	}

	if ((End - Start + 1) > BufferCharacters)
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	for (Index = 0; Index < (End - Start); Index++)
	{
		Buffer[Index] = Source[Start + Index];
	}

	Buffer[Index] = 0;
	return (Index == 0) ? STATUS_NOT_FOUND : STATUS_SUCCESS;
}
