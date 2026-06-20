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

#pragma once

#include "types.h"

#define BL_APPLICATION_ENTRY_BCD_OPTIONS_INTERNAL 0x02
#define BL_APPLICATION_ENTRY_BCD_OPTIONS_EXTERNAL 0x80

#define BCD_ELEMENT_FORMAT_STRING 0x02
#define BCD_BOOTAPP_LOAD_OPTIONS 0x12000030

typedef struct _BL_BCD_OPTION
{
	UINT32 Type;
	UINT32 DataOffset;
	UINT32 DataSize;
	UINT32 ListOffset;
	UINT32 NextEntryOffset;
	UINT32 Empty;
} BL_BCD_OPTION;

typedef struct _BL_APPLICATION_ENTRY
{
	char Signature[8];
	UINT32 Flags;
	EFI_GUID EfiGuid;
	UINT32 Unknown[4];
	BL_BCD_OPTION BcdData;
} BL_APPLICATION_ENTRY;

typedef struct _BL_LOADED_APPLICATION_ENTRY
{
	UINT32 Flags;
	EFI_GUID EfiGuid;
	BL_BCD_OPTION* BcdData;
} BL_LOADED_APPLICATION_ENTRY;

NTSTATUS BlGetLoadedApplicationEntry(
	BOOT_APPLICATION_PARAMETER_BLOCK* BootParameters,
	BL_LOADED_APPLICATION_ENTRY* LoadedApplicationEntry);

NTSTATUS BlGetBootOptionString(
	BL_BCD_OPTION* List,
	UINT32 Type,
	CHAR16* Buffer,
	UINTN BufferCharacters);
