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

#pragma function(memcpy)

VOID* memcpy(VOID* Destination, const VOID* Source, UINTN Size)
{
	UINT8* DestinationBytes;
	const UINT8* SourceBytes;

	DestinationBytes = (UINT8*)Destination;
	SourceBytes = (const UINT8*)Source;
	while (Size != 0)
	{
		*DestinationBytes = *SourceBytes;
		DestinationBytes++;
		SourceBytes++;
		Size--;
	}

	return Destination;
}

#pragma function(memcmp)

int memcmp(const VOID* First, const VOID* Second, UINTN Count)
{
	const UINT8* FirstBytes;
	const UINT8* SecondBytes;

	FirstBytes = (const UINT8*)First;
	SecondBytes = (const UINT8*)Second;
	while (Count != 0)
	{
		if (*FirstBytes != *SecondBytes)
		{
			return (int)*FirstBytes - (int)*SecondBytes;
		}

		FirstBytes++;
		SecondBytes++;
		Count--;
	}

	return 0;
}

#pragma function(memset)

VOID* memset(VOID* Destination, int Value, UINTN Count)
{
	UINT8* DestinationBytes;
	UINT8 ValueByte;
	DestinationBytes = (UINT8*)Destination;
	ValueByte = (UINT8)Value;
	while (Count != 0)
	{
		*DestinationBytes = ValueByte;
		DestinationBytes++;
		Count--;
	}
	return Destination;
}

#pragma function(wcslen)

UINTN wcslen(CHAR16* String)
{
	UINTN Length;

	Length = 0;
	if (String == NULL)
	{
		return 0;
	}

	while (String[Length] != 0)
	{
		Length++;
	}

	return Length;
}
