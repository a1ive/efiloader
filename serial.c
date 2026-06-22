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

#include "serial.h"

#if defined(ENABLE_SERIAL_DEBUG) && (defined(_M_IX86) || defined(_M_X64))

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#define COM1_PORT 0x3F8

#if defined(__GNUC__) && !defined(_MSC_VER)
static VOID SerialOutByte(UINT16 Port, UINT8 Value)
{
	__asm__ __volatile__("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

static UINT8 SerialInByte(UINT16 Port)
{
	UINT8 Value;
	__asm__ __volatile__("inb %1, %0" : "=a"(Value) : "Nd"(Port));
	return Value;
}
#else
#define SerialOutByte(Port, Value) __outbyte((Port), (Value))
#define SerialInByte(Port) __inbyte((Port))
#endif

VOID SerialInitialize(VOID)
{
	SerialOutByte(COM1_PORT + 1, 0x00);
	SerialOutByte(COM1_PORT + 3, 0x80);
	SerialOutByte(COM1_PORT + 0, 0x03);
	SerialOutByte(COM1_PORT + 1, 0x00);
	SerialOutByte(COM1_PORT + 3, 0x03);
	SerialOutByte(COM1_PORT + 2, 0xC7);
	SerialOutByte(COM1_PORT + 4, 0x0B);
}

VOID SerialWriteCharacter(CHAR16 Character)
{
	UINT32 Timeout;

	for (Timeout = 0; Timeout < 100000; Timeout++)
	{
		if ((SerialInByte(COM1_PORT + 5) & 0x20) != 0)
		{
			break;
		}
	}

	SerialOutByte(COM1_PORT, Character < 0x80 ? (UINT8)Character : '?');
}

#endif
