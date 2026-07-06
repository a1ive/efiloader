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

#include "efidisk.h"
#include "iso9660.h"
#include "optical.h"
#include "print.h"
#include "udf.h"

EFI_STATUS OpticalReadFile(
	CHAR16* FilePath,
	VOID** FileBuffer,
	UINTN* FileSize)
{
	EFI_DISK* Disk;
	EFI_STATUS UdfStatus;
	EFI_STATUS IsoStatus;
	EFI_STATUS Status;

	if ((FilePath == NULL) || (FilePath[0] == 0) ||
		(FileBuffer == NULL) || (FileSize == NULL))
	{
		return EFI_STATUS_INVALID_PARAMETER;
	}

	*FileBuffer = NULL;
	*FileSize = 0;
	Status = EfiDiskOpenBootOptical(&Disk);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	UdfStatus = UdfReadFile(Disk, FilePath, FileBuffer, FileSize);
	SerialPrint(L"UdfReadFile status=0x%016llX\n", UdfStatus);
	if (!EFI_ERROR(UdfStatus))
	{
		EfiDiskClose(Disk);
		return EFI_SUCCESS;
	}

	IsoStatus = Iso9660ReadFile(Disk, FilePath, FileBuffer, FileSize);
	SerialPrint(L"IsoReadFile status=0x%016llX\n", IsoStatus);
	EfiDiskClose(Disk);
	if (!EFI_ERROR(IsoStatus))
	{
		return EFI_SUCCESS;
	}

	if ((UdfStatus == EFI_STATUS_NOT_FOUND) ||
		(IsoStatus == EFI_STATUS_NOT_FOUND))
	{
		return EFI_STATUS_NOT_FOUND;
	}

	return IsoStatus;
}
