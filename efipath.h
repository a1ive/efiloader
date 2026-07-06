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

BOOLEAN EfiIsDevicePathPrefix(
	EFI_DEVICE_PATH_PROTOCOL* Candidate,
	EFI_DEVICE_PATH_PROTOCOL* Child,
	UINTN* PrefixSize,
	BOOLEAN* ExactMatch);
EFI_STATUS EfiGetParentLoadedImage(
	EFI_LOADED_IMAGE_PROTOCOL** LoadedImage);
EFI_STATUS EfiGetParentDevicePath(
	EFI_DEVICE_PATH_PROTOCOL** DevicePath);
EFI_STATUS EfiCreateFileDevicePath(
	CHAR16* ImagePath,
	EFI_DEVICE_PATH_PROTOCOL** DevicePath,
	UINTN* DevicePathSize);
EFI_STATUS EfiBuildFullImageDevicePath(
	EFI_DEVICE_PATH_PROTOCOL* RelativeDevicePath,
	EFI_DEVICE_PATH_PROTOCOL** FullDevicePath);
