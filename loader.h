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

typedef struct _EFI_CONTEXT
{
	BL_FIRMWARE_DESCRIPTOR* Firmware;
	EFI_SYSTEM_TABLE* SystemTable;
	EFI_BOOT_SERVICES* BootServices;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* ConOut;
	EFI_HANDLE ParentHandle;
} EFI_CONTEXT;

extern EFI_CONTEXT gContext;

NTSTATUS EfiInitializeContext(BL_FIRMWARE_DESCRIPTOR* Firmware);
NTSTATUS EfiStartApplication(CHAR16* ImagePath);
