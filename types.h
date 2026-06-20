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
#if defined(_M_IX86)
typedef unsigned long UINTN;
typedef long INTN;
#define EFI_STATUS_ERROR_BIT_VALUE 0x80000000UL
#elif defined(_M_X64)
typedef unsigned __int64 UINTN;
typedef __int64 INTN;
#define EFI_STATUS_ERROR_BIT_VALUE 0x8000000000000000ULL
#else
#error Unsupported architecture
#endif
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
#define EFI_STATUS_ERROR_BIT ((EFI_STATUS)EFI_STATUS_ERROR_BIT_VALUE)
#define EFI_ERROR(Status) (((EFI_STATUS)(Status) & EFI_STATUS_ERROR_BIT) != 0)
#define EFIERR(Code) ((EFI_STATUS)(EFI_STATUS_ERROR_BIT | (EFI_STATUS)(Code)))

#define EFI_STATUS_INVALID_PARAMETER EFIERR(2)
#define EFI_STATUS_UNSUPPORTED EFIERR(3)
#define EFI_STATUS_BUFFER_TOO_SMALL EFIERR(5)
#define EFI_STATUS_OUT_OF_RESOURCES EFIERR(9)
#define EFI_STATUS_NOT_FOUND EFIERR(14)

#define EfiLoaderData ((EFI_MEMORY_TYPE)2)

#define BL_RETURN_ARGUMENTS_VERSION 1

#if defined(_M_IX86)
#define BLAPI __stdcall
#else
#define BLAPI
#endif

#define IMAGE_DOS_SIGNATURE 0x5A4D
#define IMAGE_NT_SIGNATURE 0x00004550
#define IMAGE_FILE_MACHINE_I386 0x014C
#define IMAGE_FILE_MACHINE_AMD64 0x8664
#define IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10B
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

typedef struct _EFI_FILEPATH_DEVICE_PATH
{
	EFI_DEVICE_PATH_PROTOCOL Header;
	CHAR16 PathName[1];
} EFI_FILEPATH_DEVICE_PATH;

#define SIZE_OF_FILEPATH_DEVICE_PATH FIELD_OFFSET(EFI_FILEPATH_DEVICE_PATH, PathName)

#define EFI_DP_TYPE_MASK 0x7F

#define END_DEVICE_PATH_LENGTH ((UINTN)sizeof(EFI_DEVICE_PATH_PROTOCOL))

#define MEDIA_DEVICE_PATH 0x04
#define MEDIA_FILEPATH_DP 0x04
#define END_DEVICE_PATH_TYPE 0x7F
#define END_ENTIRE_DEVICE_PATH_SUBTYPE 0xFF

typedef struct _EFI_PXE_BASE_CODE_PROTOCOL EFI_PXE_BASE_CODE_PROTOCOL;

typedef struct _EFI_PXE_BASE_CODE_DHCPV4_PACKET
{
	UINT8 BootpOpcode;
	UINT8 BootpHwType;
	UINT8 BootpHwAddrLen;
	UINT8 BootpGateHops;
	UINT32 BootpIdent;
	UINT16 BootpSeconds;
	UINT16 BootpFlags;
	UINT8 BootpCiAddr[4];
	UINT8 BootpYiAddr[4];
	UINT8 BootpSiAddr[4];
	UINT8 BootpGiAddr[4];
	UINT8 BootpHwAddr[16];
	UINT8 BootpSrvName[64];
	UINT8 BootpBootFile[128];
	UINT32 DhcpMagik;
	UINT8 DhcpOptions[56];
} EFI_PXE_BASE_CODE_DHCPV4_PACKET;

typedef union _EFI_PXE_BASE_CODE_PACKET
{
	UINT8 Raw[1472];
	EFI_PXE_BASE_CODE_DHCPV4_PACKET Dhcpv4;
} EFI_PXE_BASE_CODE_PACKET;

typedef struct _EFI_PXE_BASE_CODE_MODE
{
	BOOLEAN Started;
	BOOLEAN Ipv6Available;
	BOOLEAN Ipv6Supported;
	BOOLEAN UsingIpv6;
	BOOLEAN BisSupported;
	BOOLEAN BisDetected;
	BOOLEAN AutoArp;
	BOOLEAN SendGUID;
	BOOLEAN DhcpDiscoverValid;
	BOOLEAN DhcpAckReceived;
	BOOLEAN ProxyOfferReceived;
	BOOLEAN PxeDiscoverValid;
	BOOLEAN PxeReplyReceived;
	BOOLEAN PxeBisReplyReceived;
	BOOLEAN IcmpErrorReceived;
	BOOLEAN TftpErrorReceived;
	BOOLEAN MakeCallbacks;
	UINT8 TTL;
	UINT8 ToS;
	EFI_IP_ADDRESS StationIp;
	EFI_IP_ADDRESS SubnetMask;
	EFI_PXE_BASE_CODE_PACKET DhcpDiscover;
	EFI_PXE_BASE_CODE_PACKET DhcpAck;
	EFI_PXE_BASE_CODE_PACKET ProxyOffer;
	EFI_PXE_BASE_CODE_PACKET PxeDiscover;
	EFI_PXE_BASE_CODE_PACKET PxeReply;
	EFI_PXE_BASE_CODE_PACKET PxeBisReply;
} EFI_PXE_BASE_CODE_MODE;

typedef enum _EFI_PXE_BASE_CODE_TFTP_OPCODE
{
	EFI_PXE_BASE_CODE_TFTP_FIRST,
	EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE,
	EFI_PXE_BASE_CODE_TFTP_READ_FILE,
	EFI_PXE_BASE_CODE_TFTP_WRITE_FILE,
	EFI_PXE_BASE_CODE_TFTP_READ_DIRECTORY,
	EFI_PXE_BASE_CODE_MTFTP_GET_FILE_SIZE,
	EFI_PXE_BASE_CODE_MTFTP_READ_FILE,
	EFI_PXE_BASE_CODE_MTFTP_READ_DIRECTORY,
	EFI_PXE_BASE_CODE_MTFTP_LAST
} EFI_PXE_BASE_CODE_TFTP_OPCODE;

typedef EFI_STATUS(*EFI_PXE_BASE_CODE_START)(
	EFI_PXE_BASE_CODE_PROTOCOL* This,
	BOOLEAN UseIpv6);

typedef EFI_STATUS(*EFI_PXE_BASE_CODE_DHCP)(
	EFI_PXE_BASE_CODE_PROTOCOL* This,
	BOOLEAN SortOffers);

typedef EFI_STATUS(*EFI_PXE_BASE_CODE_MTFTP)(
	EFI_PXE_BASE_CODE_PROTOCOL* This,
	EFI_PXE_BASE_CODE_TFTP_OPCODE Operation,
	VOID* BufferPtr,
	BOOLEAN Overwrite,
	UINT64* BufferSize,
	UINTN* BlockSize,
	EFI_IP_ADDRESS* ServerIp,
	UINT8* Filename,
	VOID* Info,
	BOOLEAN DontUseBuffer);

struct _EFI_PXE_BASE_CODE_PROTOCOL
{
	UINT64 Revision;
	EFI_PXE_BASE_CODE_START Start;
	VOID* Stop;
	EFI_PXE_BASE_CODE_DHCP Dhcp;
	VOID* Discover;
	EFI_PXE_BASE_CODE_MTFTP Mtftp;
	VOID* UdpWrite;
	VOID* UdpRead;
	VOID* SetIpFilter;
	VOID* Arp;
	VOID* SetParameters;
	VOID* SetStationIp;
	VOID* SetPackets;
	EFI_PXE_BASE_CODE_MODE* Mode;
};

C_ASSERT(sizeof(EFI_PXE_BASE_CODE_DHCPV4_PACKET) == 296);
C_ASSERT(sizeof(EFI_PXE_BASE_CODE_PACKET) == 1472);
C_ASSERT(FIELD_OFFSET(EFI_PXE_BASE_CODE_MODE, DhcpAck) == 1524);

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

typedef struct _IA32_DESCRIPTOR_TABLE_REGISTER
{
	UINT16 Limit;
	UINT32 Base;
} IA32_DESCRIPTOR_TABLE_REGISTER;

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

typedef struct _BL_FIRMWARE_PROCESSOR_STATE_IA32
{
	UINT32 Mode;
	UINT32 TranslationType;
	UINT32 Flags;
	UINT32 Reserved;
} BL_FIRMWARE_PROCESSOR_STATE_IA32;

typedef struct _BL_FIRMWARE_DESCRIPTOR_X64
{
	UINT32 Version;
	UINT32 Unknown;
	EFI_HANDLE ImageHandle;
	EFI_SYSTEM_TABLE* SystemTable;
	UINT32 Reserved18;
	BL_FIRMWARE_PROCESSOR_STATE_X64 ProcessorState;
} BL_FIRMWARE_DESCRIPTOR_X64;

typedef struct _BL_FIRMWARE_DESCRIPTOR_IA32
{
	UINT32 Version;
	UINT32 Unknown;
	EFI_HANDLE ImageHandle;
	EFI_SYSTEM_TABLE* SystemTable;
} BL_FIRMWARE_DESCRIPTOR_IA32;

#pragma pack(pop)

#if defined(_M_IX86)
C_ASSERT(sizeof(IA32_DESCRIPTOR_TABLE_REGISTER) == 6);
C_ASSERT(sizeof(BL_FIRMWARE_PROCESSOR_STATE_IA32) == 0x10);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_IA32, ImageHandle) == 0x08);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_IA32, SystemTable) == 0x0C);
C_ASSERT(sizeof(BL_FIRMWARE_DESCRIPTOR_IA32) == 0x10);
typedef BL_FIRMWARE_PROCESSOR_STATE_IA32 BL_FIRMWARE_PROCESSOR_STATE;
typedef BL_FIRMWARE_DESCRIPTOR_IA32 BL_FIRMWARE_DESCRIPTOR;
#elif defined(_M_X64)
C_ASSERT(sizeof(X64_DESCRIPTOR_TABLE_REGISTER) == 10);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_X64, ImageHandle) == 0x08);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_X64, SystemTable) == 0x10);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_X64, ProcessorState) == 0x1C);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_X64, ProcessorState.Gdtr) == 0x24);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_X64, ProcessorState.Ss) == 0x44);
C_ASSERT(FIELD_OFFSET(BL_FIRMWARE_DESCRIPTOR_X64, ProcessorState.TranslationEnabled) == 0x46);
C_ASSERT(sizeof(BL_FIRMWARE_DESCRIPTOR_X64) == 0x48);
typedef BL_FIRMWARE_PROCESSOR_STATE_X64 BL_FIRMWARE_PROCESSOR_STATE;
typedef BL_FIRMWARE_DESCRIPTOR_X64 BL_FIRMWARE_DESCRIPTOR;
#endif

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
