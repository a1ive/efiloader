# efiloader

`efiloader` is a Windows Boot Application that is launched by Windows Boot
Manager and then restores the UEFI firmware execution context so it can call
normal UEFI services and start another EFI application.

## What It Does

Windows Boot Applications are entered after Boot Manager has switched into its
own application context. In that context, direct calls into firmware services
such as `ST`, `BS`, or console protocols may partially work or may hang.

`efiloader` reads the firmware descriptor from
`BOOT_APPLICATION_PARAMETER_BLOCK`, restores the saved x64 firmware processor
state, verifies screen output through `ConOut->OutputString`, and then launches
a target EFI application.

The restored firmware context includes:

- `CR3`
- `GDTR`
- `IDTR`
- `LDTR`
- `CS`
- `DS`, `ES`, `FS`, `GS`, `SS`

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
`ENABLE_SERIAL_DEBUG` when building to enable diagnostics.

## Limitations

After switching from Boot Manager's application context to the firmware context,
the current control flow treats the switch as one-way. Do not add normal returns
to Boot Manager after firmware calls unless the Boot Manager application context
is also captured, restored, and verified.

Only x64 firmware descriptors with version 2 or newer are supported by the
current implementation.

## Acknowledgements

This project references work from:

- https://github.com/imbushuo/boot-shim
- https://github.com/reactos/reactos
- https://github.com/Mattiwatti/EfiGuard
