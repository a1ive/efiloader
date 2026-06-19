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

#include "loader.h"
#include "serial.h"

typedef struct _PRINT_CONTEXT
{
	BOOLEAN SerialOnly;
	EFI_STATUS Status;
} PRINT_CONTEXT;

static VOID PrintPutCharacter(PRINT_CONTEXT* PrintContext, CHAR16 Character)
{
	EFI_STATUS Status;
	CHAR16 Buffer[2];

	if (Character == '\n')
	{
		PrintPutCharacter(PrintContext, '\r');
	}

#ifdef ENABLE_SERIAL_DEBUG
	if (PrintContext->SerialOnly)
	{
		SerialWriteCharacter(Character);
		return;
	}
#endif

	if (EFI_ERROR(PrintContext->Status))
	{
		return;
	}

	Buffer[0] = Character;
	Buffer[1] = 0;
	Status = gContext.ConOut->OutputString(gContext.ConOut, Buffer);
	if (EFI_ERROR(Status))
	{
		PrintContext->Status = Status;
	}
}

static VOID PrintHex(
	PRINT_CONTEXT* PrintContext,
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
		PrintPutCharacter(PrintContext, '0');
		Width--;
	}

	while (Index != 0)
	{
		Index--;
		PrintPutCharacter(PrintContext, Buffer[Index]);
	}
}

static VOID PrintString(
	PRINT_CONTEXT* PrintContext,
	const CHAR16* String)
{
	if (String == NULL)
	{
		String = L"(null)";
	}

	while (*String != 0)
	{
		PrintPutCharacter(PrintContext, *String);
		String++;
	}
}

static VOID PrintFormat(
	PRINT_CONTEXT* PrintContext,
	const CHAR16* Format,
	va_list Arguments)
{
	while ((Format != NULL) && (*Format != 0))
	{
		UINTN Width;
		CHAR16 Specifier;

		if (*Format != '%')
		{
			PrintPutCharacter(PrintContext, *Format);
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
			PrintString(PrintContext, va_arg(Arguments, CHAR16*));
			break;

		case 'p':
			PrintString(PrintContext, L"0x");
			PrintHex(PrintContext, (UINT64)(UINTN)va_arg(Arguments, VOID*), 16);
			break;

		case 'X':
		case 'x':
			PrintHex(PrintContext, va_arg(Arguments, UINT64), Width);
			break;

		case '%':
			PrintPutCharacter(PrintContext, '%');
			break;

		default:
			PrintPutCharacter(PrintContext, '%');
			PrintPutCharacter(PrintContext, Specifier);
			break;
		}
	}
}

#ifdef ENABLE_SERIAL_DEBUG

VOID SerialPrint(const CHAR16* Format, ...)
{
	PRINT_CONTEXT PrintContext;
	va_list Arguments;

	PrintContext.SerialOnly = TRUE;
	PrintContext.Status = EFI_SUCCESS;

	va_start(Arguments, Format);
	PrintFormat(&PrintContext, Format, Arguments);
	va_end(Arguments);
}

#endif

EFI_STATUS EfiPrint(const CHAR16* Format, ...)
{
	PRINT_CONTEXT PrintContext;
	va_list Arguments;

	PrintContext.SerialOnly = FALSE;
	PrintContext.Status = EFI_SUCCESS;

	va_start(Arguments, Format);
	PrintFormat(&PrintContext, Format, Arguments);
	va_end(Arguments);

	return PrintContext.Status;
}
