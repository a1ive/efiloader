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

#include "mm.h"
#include "compiler.h"
#include "loader.h"

EFI_STATUS EfiAllocatePool(UINTN Size, VOID** Buffer)
{
	EFI_STATUS Status;

	if (Buffer == NULL)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*Buffer = NULL;
	if (Size == 0)
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	Status = gContext.BootServices->AllocatePool(EfiLoaderData, Size, Buffer);
	if (!EFI_ERROR(Status) && (*Buffer == NULL))
	{
		return EFI_STATUS_OUT_OF_RESOURCES;
	}

	return Status;
}

EFI_STATUS EfiAllocateZeroPool(UINTN Size, VOID** Buffer)
{
	EFI_STATUS Status;

	Status = EfiAllocatePool(Size, Buffer);
	if (!EFI_ERROR(Status))
	{
		memset(*Buffer, 0, Size);
	}

	return Status;
}

VOID EfiFreePool(VOID* Buffer)
{
	if (Buffer != NULL)
	{
		gContext.BootServices->FreePool(Buffer);
	}
}

