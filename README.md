# efiloader

`efiloader` is a Windows Boot Application that is launched by Windows Boot
Manager and then restores the UEFI firmware execution context so it can call
normal UEFI services and start another EFI application.

## What It Does

Windows Boot Applications are entered after Boot Manager has switched into its
own application context. In that context, direct calls into firmware services
such as `ST`, `BS`, or console protocols may partially work or may hang.

`efiloader` reads the firmware descriptor from
`BOOT_APPLICATION_PARAMETER_BLOCK`, prepares a firmware-callable context,
verifies screen output through `ConOut->OutputString`, and then launches a
target EFI application.

On x64, the restored firmware context includes:

- `CR3`
- `GDTR`
- `IDTR`
- `LDTR`
- `CS`
- `DS`, `ES`, `FS`, `GS`, `SS`

On ARM64, the restored firmware context includes:

- `SCTLR_EL1`
- `VBAR_EL1`
- `TPIDR_EL1`
- `TTBR0_EL1`, `TTBR1_EL1`
- `TCR_EL1`
- `MAIR_EL1`
- DAIF IRQ state

## Building on Linux with GCC/MinGW

A GNU Make build is available for MinGW cross toolchains. Install `nasm` and
the matching `*-w64-mingw32-gcc` package for the target architecture, then run:

```sh
make ARCH=x64
make ARCH=ia32
make ARCH=arm64
```

The default target is `ARCH=x64`, and outputs are written under `build/<arch>/`.
You can override the cross-toolchain prefix when needed:

```sh
make ARCH=x64 CROSS_COMPILE=x86_64-w64-mingw32-
```

The Visual Studio project remains supported for MSVC builds.

## BCD Options

The Boot Application entry should point to:

```text
\efiloader.efi
```

The BCD entry GUID and the path to `efiloader.efi` can be customized. Integrity
checks must be disabled for the Boot Application entry:

```batch
set guid={14530529-1111-2222-abcd-12345678abcd}
bcdedit /create %guid% /d "EFI LOADER" /application bootapp
bcdedit /set %guid% device boot
bcdedit /set %guid% path \efiloader.efi
bcdedit /set %guid% nointegritychecks true
bcdedit /set {bootmgr} displayorder %guid% /addlast
```

If you are editing an offline BCD store, add the same `/store` argument to the
`bcdedit` commands as appropriate for your environment.

`efiloader` also supports overriding the launched EFI application with the BCD
`loadoptions` string element (`0x12000030`):

```batch
bcdedit %bcd% /set %guid% loadoptions \EFI\okr.efi
```

The option is copied before switching to firmware context. Leading/trailing
spaces, NUL characters, and one pair of surrounding quotes are ignored.

If `loadoptions` is absent or invalid, the fallback target is:

```text
\shell.efi
```

## ISO Boot

When booting from optical media, `efiloader.efi` must be placed in the ISO
filesystem. The EFI application launched by `efiloader`, such as `shell.efi`,
must be placed inside the El Torito UEFI image.

Example layout:

```text
[ISO]\bootmgr.efi
[ISO]\efiloader.efi
[ISO]\boot\boot.sdi
[ISO]\EFI\microsoft\boot\bcd
[IMG]\shell.efi
[IMG]\EFI\BOOT\BOOTX64.EFI
```

## Serial Debug

Serial debug output goes to COM1 and is disabled by default. Define
`ENABLE_SERIAL_DEBUG` when building x86/x64 images to enable diagnostics.

## Acknowledgements

This project references work from:

- https://github.com/imbushuo/boot-shim
- https://github.com/reactos/reactos
- https://github.com/Mattiwatti/EfiGuard
