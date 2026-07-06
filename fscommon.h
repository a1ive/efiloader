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

#define FS_MAX_FILE_SIZE (256ULL * 1024ULL * 1024ULL)
#define FS_MAX_DIRECTORY_SIZE (32ULL * 1024ULL * 1024ULL)

UINT16 FsReadLe16(const UINT8* Buffer);
UINT32 FsReadLe32(const UINT8* Buffer);
UINT64 FsReadLe64(const UINT8* Buffer);
UINT64 FsShiftRight64(UINT64 Value, UINTN Shift);
UINT64 FsShiftLeft32To64(UINT32 Value, UINTN Shift);

BOOLEAN FsGetSectorShift(UINTN Value, UINTN* Shift);
CHAR16 FsFoldAsciiCharacter(CHAR16 Character);
BOOLEAN FsUnicodeNameEquals(
	const CHAR16* First,
	UINTN FirstLength,
	const CHAR16* Second,
	UINTN SecondLength);
BOOLEAN FsGetNextPathComponent(
	CHAR16** Path,
	CHAR16** Component,
	UINTN* ComponentLength,
	BOOLEAN* LastComponent);
BOOLEAN FsIsDotComponent(const CHAR16* Component, UINTN Length);
BOOLEAN FsIsDotDotComponent(const CHAR16* Component, UINTN Length);
