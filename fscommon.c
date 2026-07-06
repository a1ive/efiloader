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

#include "fscommon.h"

typedef union _UINT64_PARTS
{
	UINT64 Value;
	struct
	{
		UINT32 Low;
		UINT32 High;
	} Parts;
} UINT64_PARTS;

UINT16 FsReadLe16(const UINT8* Buffer)
{
	return (UINT16)(((UINT16)Buffer[0]) | ((UINT16)Buffer[1] << 8));
}

UINT32 FsReadLe32(const UINT8* Buffer)
{
	return
		((UINT32)Buffer[0]) |
		((UINT32)Buffer[1] << 8) |
		((UINT32)Buffer[2] << 16) |
		((UINT32)Buffer[3] << 24);
}

UINT64 FsReadLe64(const UINT8* Buffer)
{
	return
		((UINT64)FsReadLe32(Buffer)) |
		((UINT64)FsReadLe32(Buffer + 4) << 32);
}

UINT64 FsShiftRight64(UINT64 Value, UINTN Shift)
{
	UINT64_PARTS Result;

	Result.Value = Value;
	if (Shift == 0)
	{
		return Result.Value;
	}

	if (Shift < 32)
	{
		Result.Parts.Low =
			(Result.Parts.Low >> Shift) |
			(Result.Parts.High << (32 - Shift));
		Result.Parts.High >>= Shift;
	}
	else
	{
		Result.Parts.Low = Result.Parts.High >> (Shift - 32);
		Result.Parts.High = 0;
	}

	return Result.Value;
}

UINT64 FsShiftLeft32To64(UINT32 Value, UINTN Shift)
{
	UINT64_PARTS Result;

	Result.Value = 0;
	if (Shift == 0)
	{
		Result.Parts.Low = Value;
	}
	else if (Shift < 32)
	{
		Result.Parts.Low = Value << Shift;
		Result.Parts.High = Value >> (32 - Shift);
	}
	else
	{
		Result.Parts.High = Value << (Shift - 32);
	}

	return Result.Value;
}

BOOLEAN FsGetSectorShift(UINTN Value, UINTN* Shift)
{
	UINTN CurrentShift;
	UINTN Remaining;

	if ((Value == 0) || ((Value & (Value - 1)) != 0))
	{
		return FALSE;
	}

	CurrentShift = 0;
	Remaining = Value;
	while (Remaining > 1)
	{
		Remaining >>= 1;
		CurrentShift++;
	}

	*Shift = CurrentShift;
	return TRUE;
}

CHAR16 FsFoldAsciiCharacter(CHAR16 Character)
{
	if ((Character >= 'A') && (Character <= 'Z'))
	{
		return (CHAR16)(Character + ('a' - 'A'));
	}

	return Character;
}

BOOLEAN FsUnicodeNameEquals(
	const CHAR16* First,
	UINTN FirstLength,
	const CHAR16* Second,
	UINTN SecondLength)
{
	UINTN Index;

	if (FirstLength != SecondLength)
	{
		return FALSE;
	}

	for (Index = 0; Index < FirstLength; Index++)
	{
		if (FsFoldAsciiCharacter(First[Index]) !=
			FsFoldAsciiCharacter(Second[Index]))
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOLEAN FsGetNextPathComponent(
	CHAR16** Path,
	CHAR16** Component,
	UINTN* ComponentLength,
	BOOLEAN* LastComponent)
{
	CHAR16* Current;
	CHAR16* Next;

	Current = *Path;
	while ((*Current == '\\') || (*Current == '/'))
	{
		Current++;
	}

	if (*Current == 0)
	{
		return FALSE;
	}

	*Component = Current;
	while ((*Current != 0) && (*Current != '\\') && (*Current != '/'))
	{
		Current++;
	}

	*ComponentLength = (UINTN)(Current - *Component);
	Next = Current;
	while ((*Next == '\\') || (*Next == '/'))
	{
		Next++;
	}

	*LastComponent = (*Next == 0) ? TRUE : FALSE;
	*Path = Current;
	return TRUE;
}

BOOLEAN FsIsDotComponent(const CHAR16* Component, UINTN Length)
{
	return ((Length == 1) && (Component[0] == '.'));
}

BOOLEAN FsIsDotDotComponent(const CHAR16* Component, UINTN Length)
{
	return ((Length == 2) && (Component[0] == '.') &&
		(Component[1] == '.'));
}
