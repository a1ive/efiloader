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

#include "print.h"

#include <stdarg.h>

#include "serial.h"

typedef struct _PRINT_CONTEXT
{
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* ConOut;
	EFI_STATUS Status;
} PRINT_CONTEXT;

static VOID PrintPutCharacter(PRINT_CONTEXT* Context, CHAR16 Character)
{
	EFI_STATUS Status;
	CHAR16 Buffer[2];

	if (Character == '\n')
	{
		PrintPutCharacter(Context, '\r');
	}

#ifdef ENABLE_SERIAL_DEBUG
	if (Context->ConOut == NULL)
	{
		SerialWriteCharacter(Character);
		return;
	}
#endif

	if (EFI_ERROR(Context->Status))
	{
		return;
	}

	Buffer[0] = Character;
	Buffer[1] = 0;
	Status = Context->ConOut->OutputString(Context->ConOut, Buffer);
	if (EFI_ERROR(Status))
	{
		Context->Status = Status;
	}
}

static VOID PrintHex(
	PRINT_CONTEXT* Context,
	UINT64 Value,
	UINTN Width)
{
	CHAR16 Buffer[16];
	UINTN Index;

	Index = 0;
	do
	{
		UINT8 Digit;

		Digit = (UINT8)(Value & 0xF);
		Buffer[Index] = (CHAR16)(Digit < 10 ? ('0' + Digit) : ('A' + Digit - 10));
		Value >>= 4;
		Index++;
	} while (Value != 0);

	while (Width > Index)
	{
		PrintPutCharacter(Context, '0');
		Width--;
	}

	while (Index != 0)
	{
		Index--;
		PrintPutCharacter(Context, Buffer[Index]);
	}
}

static VOID PrintString(
	PRINT_CONTEXT* Context,
	const CHAR16* String)
{
	if (String == NULL)
	{
		String = L"(null)";
	}

	while (*String != 0)
	{
		PrintPutCharacter(Context, *String);
		String++;
	}
}

static VOID PrintFormat(
	PRINT_CONTEXT* Context,
	const CHAR16* Format,
	va_list Arguments)
{
	while ((Format != NULL) && (*Format != 0))
	{
		UINTN Width;
		CHAR16 Specifier;

		if (*Format != '%')
		{
			PrintPutCharacter(Context, *Format);
			Format++;
			continue;
		}

		Format++;
		Width = 0;
		if (*Format == '0')
		{
			Format++;
		}

		while ((*Format >= '0') && (*Format <= '9'))
		{
			Width = (Width * 10) + (*Format - '0');
			Format++;
		}

		while (*Format == 'l')
		{
			Format++;
		}

		Specifier = *Format;
		if (Specifier == 0)
		{
			break;
		}

		Format++;
		switch (Specifier)
		{
		case 's':
			PrintString(Context, va_arg(Arguments, CHAR16*));
			break;

		case 'p':
			PrintString(Context, L"0x");
			PrintHex(Context, (UINT64)(UINTN)va_arg(Arguments, VOID*), 16);
			break;

		case 'X':
		case 'x':
			PrintHex(Context, va_arg(Arguments, UINT64), Width);
			break;

		case '%':
			PrintPutCharacter(Context, '%');
			break;

		default:
			PrintPutCharacter(Context, '%');
			PrintPutCharacter(Context, Specifier);
			break;
		}
	}
}

#ifdef ENABLE_SERIAL_DEBUG

VOID SerialPrint(const CHAR16* Format, ...)
{
	PRINT_CONTEXT Context;
	va_list Arguments;

	Context.ConOut = NULL;
	Context.Status = EFI_SUCCESS;

	va_start(Arguments, Format);
	PrintFormat(&Context, Format, Arguments);
	va_end(Arguments);
}

#endif

EFI_STATUS EfiPrint(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* ConOut, const CHAR16* Format, ...)
{
	PRINT_CONTEXT Context;
	va_list Arguments;

	if ((ConOut == NULL) || (Format == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	Context.ConOut = ConOut;
	Context.Status = EFI_SUCCESS;

	va_start(Arguments, Format);
	PrintFormat(&Context, Format, Arguments);
	va_end(Arguments);

	return Context.Status;
}
