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

#pragma once

#ifndef NULL
#define NULL ((void*)0)
#endif

#define TRUE 1
#define FALSE 0

typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned long UINT32;
typedef unsigned __int64 UINT64;
typedef __int32 INT32;
typedef unsigned __int64 UINTN;
typedef __int64 INTN;
typedef long NTSTATUS;
typedef unsigned char BOOLEAN;
typedef unsigned short CHAR16;
typedef void VOID;
typedef VOID* EFI_HANDLE;
typedef UINTN EFI_STATUS;
typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT32 EFI_MEMORY_TYPE;

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000DL)
#define STATUS_NOT_FOUND ((NTSTATUS)0xC0000225L)
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xC00000BBL)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)

#define EFI_SUCCESS ((EFI_STATUS)0)
#define EFI_ERROR(Status) (((EFI_STATUS)(Status) & 0x8000000000000000ULL) != 0)

#define EFI_STATUS_INVALID_PARAMETER ((EFI_STATUS)0x8000000000000002ULL)
#define EFI_STATUS_UNSUPPORTED ((EFI_STATUS)0x8000000000000003ULL)
#define EFI_STATUS_BUFFER_TOO_SMALL ((EFI_STATUS)0x8000000000000005ULL)
#define EFI_STATUS_NOT_FOUND ((EFI_STATUS)0x800000000000000EULL)

#define EfiLoaderData ((EFI_MEMORY_TYPE)2)

#define BL_RETURN_ARGUMENTS_VERSION 1

#define IMAGE_DOS_SIGNATURE 0x5A4D
#define IMAGE_NT_SIGNATURE 0x00004550
#define IMAGE_FILE_MACHINE_AMD64 0x8664
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20B
#define IMAGE_DIRECTORY_ENTRY_EXPORT 0

#define FIELD_OFFSET(Type, Field) ((UINTN)&(((Type*)0)->Field))
#define C_ASSERT_JOIN2(a, b) a##b
#define C_ASSERT_JOIN(a, b) C_ASSERT_JOIN2(a, b)
#define C_ASSERT(e) typedef char C_ASSERT_JOIN(__c_assert_, __LINE__)[(e) ? 1 : -1]

typedef struct _EFI_GUID
{
	UINT32 Data1;
	UINT16 Data2;
	UINT16 Data3;
	UINT8 Data4[8];
} EFI_GUID;

typedef struct _EFI_TABLE_HEADER
{
	UINT64 Signature;
	UINT32 Revision;
	UINT32 HeaderSize;
	UINT32 CRC32;
	UINT32 Reserved;
} EFI_TABLE_HEADER;

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct _EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH_PROTOCOL;
typedef struct _EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;

typedef struct _EFI_IPv4_ADDRESS
{
	UINT8 Addr[4];
} EFI_IPv4_ADDRESS;

typedef struct _EFI_IPv6_ADDRESS
{
	UINT8 Addr[16];
} EFI_IPv6_ADDRESS;

typedef union _EFI_IP_ADDRESS
{
	UINT32 Addr[4];
	EFI_IPv4_ADDRESS v4;
	EFI_IPv6_ADDRESS v6;
} EFI_IP_ADDRESS;

typedef EFI_STATUS (*EFI_TEXT_STRING)(
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
	CHAR16* String);

typedef EFI_STATUS (*EFI_HANDLE_PROTOCOL)(
	EFI_HANDLE Handle,
	EFI_GUID* Protocol,
	VOID** Interface);

typedef EFI_STATUS (*EFI_ALLOCATE_POOL)(
	EFI_MEMORY_TYPE PoolType,
	UINTN Size,
	VOID** Buffer);

typedef EFI_STATUS (*EFI_FREE_POOL)(
	VOID* Buffer);

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
{
	VOID* Reset;
	EFI_TEXT_STRING OutputString;
	VOID* TestString;
	VOID* QueryMode;
	VOID* SetMode;
	VOID* SetAttribute;
	VOID* ClearScreen;
	VOID* SetCursorPosition;
	VOID* EnableCursor;
	VOID* Mode;
};

struct _EFI_DEVICE_PATH_PROTOCOL
{
	UINT8 Type;
	UINT8 SubType;
	UINT8 Length[2];
};

#define MEDIA_DEVICE_PATH 0x04
#define MEDIA_FILEPATH_DP 0x04
#define END_DEVICE_PATH_TYPE 0x7F
#define END_ENTIRE_DEVICE_PATH_SUBTYPE 0xFF

typedef EFI_STATUS (*EFI_IMAGE_LOAD)(
	BOOLEAN BootPolicy,
	EFI_HANDLE ParentImageHandle,
	EFI_DEVICE_PATH_PROTOCOL* DevicePath,
	VOID* SourceBuffer,
	UINTN SourceSize,
	EFI_HANDLE* ImageHandle);

typedef EFI_STATUS (*EFI_IMAGE_START)(
	EFI_HANDLE ImageHandle,
	UINTN* ExitDataSize,
	CHAR16** ExitData);

typedef EFI_STATUS (*EFI_SET_WATCHDOG_TIMER)(
	UINTN Timeout,
	UINT64 WatchdogCode,
	UINTN DataSize,
	CHAR16* WatchdogData);

typedef EFI_STATUS (*EFI_IMAGE_UNLOAD)(
	EFI_HANDLE ImageHandle);

typedef struct _EFI_LOADED_IMAGE_PROTOCOL
{
	UINT32 Revision;
	EFI_HANDLE ParentHandle;
	EFI_SYSTEM_TABLE* SystemTable;
	EFI_HANDLE DeviceHandle;
	EFI_DEVICE_PATH_PROTOCOL* FilePath;
	VOID* Reserved;
	UINT32 LoadOptionsSize;
	VOID* LoadOptions;
	VOID* ImageBase;
	UINT64 ImageSize;
	UINTN ImageCodeType;
	UINTN ImageDataType;
	EFI_IMAGE_UNLOAD Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

typedef struct _EFI_BOOT_SERVICES
{
	EFI_TABLE_HEADER Hdr;
	VOID* RaiseTPL;
	VOID* RestoreTPL;
	VOID* AllocatePages;
	VOID* FreePages;
	VOID* GetMemoryMap;
	EFI_ALLOCATE_POOL AllocatePool;
	EFI_FREE_POOL FreePool;
	VOID* CreateEvent;
	VOID* SetTimer;
	VOID* WaitForEvent;
	VOID* SignalEvent;
	VOID* CloseEvent;
	VOID* CheckEvent;
	VOID* InstallProtocolInterface;
	VOID* ReinstallProtocolInterface;
	VOID* UninstallProtocolInterface;
	EFI_HANDLE_PROTOCOL HandleProtocol;
	VOID* Reserved;
	VOID* RegisterProtocolNotify;
	VOID* LocateHandle;
	VOID* LocateDevicePath;
	VOID* InstallConfigurationTable;
	EFI_IMAGE_LOAD LoadImage;
	EFI_IMAGE_START StartImage;
	VOID* Exit;
	VOID* UnloadImage;
	VOID* ExitBootServices;
	VOID* GetNextMonotonicCount;
	VOID* Stall;
	EFI_SET_WATCHDOG_TIMER SetWatchdogTimer;
} EFI_BOOT_SERVICES;

struct _EFI_SYSTEM_TABLE
{
	EFI_TABLE_HEADER Hdr;
	CHAR16* FirmwareVendor;
	UINT32 FirmwareRevision;
	EFI_HANDLE ConsoleInHandle;
	VOID* ConIn;
	EFI_HANDLE ConsoleOutHandle;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* ConOut;
	EFI_HANDLE StandardErrorHandle;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* StdErr;
	VOID* RuntimeServices;
	EFI_BOOT_SERVICES* BootServices;
	UINTN NumberOfTableEntries;
	VOID* ConfigurationTable;
};

typedef struct _BOOT_APPLICATION_PARAMETER_BLOCK
{
	UINT32 Signature[2];
	UINT32 Version;
	UINT32 Size;
	UINT32 ImageType;
	UINT32 MemoryTranslationType;
	UINT64 ImageBase;
	UINT32 ImageSize;
	UINT32 MemoryDataOffset;
	UINT32 AppEntryOffset;
	UINT32 BootDeviceOffset;
	UINT32 FirmwareParametersOffset;
	UINT32 ReturnArgumentsOffset;
} BOOT_APPLICATION_PARAMETER_BLOCK;

#pragma pack(push, 1)

typedef struct _X64_DESCRIPTOR_TABLE_REGISTER
{
	UINT16 Limit;
	UINT64 Base;
} X64_DESCRIPTOR_TABLE_REGISTER;

typedef struct _BL_FIRMWARE_PROCESSOR_STATE_X64
{
	UINT64 Cr3;
	X64_DESCRIPTOR_TABLE_REGISTER Gdtr;
	X64_DESCRIPTOR_TABLE_REGISTER Idtr;
	UINT16 Ldtr;
	UINT16 Cs;
	UINT16 Ds;
	UINT16 Es;
	UINT16 Fs;
	UINT16 Gs;
	UINT16 Ss;
	UINT8 TranslationEnabled;
	UINT8 Reserved;
} BL_FIRMWARE_PROCESSOR_STATE_X64;

typedef struct _BL_FIRMWARE_DESCRIPTOR_X64
{
	UINT32 Version;
	UINT32 Unknown;
	EFI_HANDLE ImageHandle;
	EFI_SYSTEM_TABLE* SystemTable;
	UINT32 Reserved18;
	BL_FIRMWARE_PROCESSOR_STATE_X64 ProcessorState;
} BL_FIRMWARE_DESCRIPTOR_X64;

#pragma pack(pop)

C_ASSERT(sizeof(X64_DESCRIPTOR_TABLE_REGISTER) == 10);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_X64, ImageHandle) == 0x08);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_X64, SystemTable) == 0x10);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_X64, ProcessorState) == 0x1C);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_X64, ProcessorState.Gdtr) == 0x24);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_X64, ProcessorState.Ss) == 0x44);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_X64, ProcessorState.TranslationEnabled) == 0x46);
C_ASSERT(sizeof(BL_FIRMWARE_DESCRIPTOR_X64) == 0x48);

typedef struct _BL_RETURN_ARGUMENTS
{
	UINT32 Version;
	NTSTATUS Status;
	UINT32 Flags;
	UINT64 DataSize;
	UINT64 DataPage;
} BL_RETURN_ARGUMENTS;

typedef struct _IMAGE_DOS_HEADER_MIN
{
	UINT16 e_magic;
	UINT16 e_cblp;
	UINT16 e_cp;
	UINT16 e_crlc;
	UINT16 e_cparhdr;
	UINT16 e_minalloc;
	UINT16 e_maxalloc;
	UINT16 e_ss;
	UINT16 e_sp;
	UINT16 e_csum;
	UINT16 e_ip;
	UINT16 e_cs;
	UINT16 e_lfarlc;
	UINT16 e_ovno;
	UINT16 e_res[4];
	UINT16 e_oemid;
	UINT16 e_oeminfo;
	UINT16 e_res2[10];
	INT32 e_lfanew;
} IMAGE_DOS_HEADER_MIN;

typedef struct _IMAGE_FILE_HEADER_MIN
{
	UINT16 Machine;
	UINT16 NumberOfSections;
	UINT32 TimeDateStamp;
	UINT32 PointerToSymbolTable;
	UINT32 NumberOfSymbols;
	UINT16 SizeOfOptionalHeader;
	UINT16 Characteristics;
} IMAGE_FILE_HEADER_MIN;

typedef struct _IMAGE_DATA_DIRECTORY_MIN
{
	UINT32 VirtualAddress;
	UINT32 Size;
} IMAGE_DATA_DIRECTORY_MIN;

typedef struct _IMAGE_OPTIONAL_HEADER64_MIN
{
	UINT16 Magic;
	UINT8 MajorLinkerVersion;
	UINT8 MinorLinkerVersion;
	UINT32 SizeOfCode;
	UINT32 SizeOfInitializedData;
	UINT32 SizeOfUninitializedData;
	UINT32 AddressOfEntryPoint;
	UINT32 BaseOfCode;
	UINT64 ImageBase;
	UINT32 SectionAlignment;
	UINT32 FileAlignment;
	UINT16 MajorOperatingSystemVersion;
	UINT16 MinorOperatingSystemVersion;
	UINT16 MajorImageVersion;
	UINT16 MinorImageVersion;
	UINT16 MajorSubsystemVersion;
	UINT16 MinorSubsystemVersion;
	UINT32 Win32VersionValue;
	UINT32 SizeOfImage;
	UINT32 SizeOfHeaders;
	UINT32 CheckSum;
	UINT16 Subsystem;
	UINT16 DllCharacteristics;
	UINT64 SizeOfStackReserve;
	UINT64 SizeOfStackCommit;
	UINT64 SizeOfHeapReserve;
	UINT64 SizeOfHeapCommit;
	UINT32 LoaderFlags;
	UINT32 NumberOfRvaAndSizes;
	IMAGE_DATA_DIRECTORY_MIN DataDirectory[16];
} IMAGE_OPTIONAL_HEADER64_MIN;

typedef struct _IMAGE_NT_HEADERS64_MIN
{
	UINT32 Signature;
	IMAGE_FILE_HEADER_MIN FileHeader;
	IMAGE_OPTIONAL_HEADER64_MIN OptionalHeader;
} IMAGE_NT_HEADERS64_MIN;

typedef struct _IMAGE_EXPORT_DIRECTORY_MIN
{
	UINT32 Characteristics;
	UINT32 TimeDateStamp;
	UINT16 MajorVersion;
	UINT16 MinorVersion;
	UINT32 Name;
	UINT32 Base;
	UINT32 NumberOfFunctions;
	UINT32 NumberOfNames;
	UINT32 AddressOfFunctions;
	UINT32 AddressOfNames;
	UINT32 AddressOfNameOrdinals;
} IMAGE_EXPORT_DIRECTORY_MIN;
