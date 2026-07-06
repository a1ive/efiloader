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

#include "efiguid.h"

EFI_GUID gEfiBlockIoProtocolGuid =
{
	0x964E5B21,
	0x6459,
	0x11D2,
	{ 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

EFI_GUID gEfiDevicePathProtocolGuid =
{
	0x09576E91,
	0x6D3F,
	0x11D2,
	{ 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

EFI_GUID gEfiLoadedImageProtocolGuid =
{
	0x5B1B31A1,
	0x9562,
	0x11D2,
	{ 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

EFI_GUID gEfiPxeBaseCodeProtocolGuid =
{
	0x03C4E603,
	0xAC28,
	0x11D3,
	{ 0x9A, 0x2D, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D }
};
