
set bcd=/store efi\microsoft\boot\bcd

set guid={14530529-1111-2222-abcd-12345678abcd}

bcdedit %bcd% /set {bootmgr} timeout 10
bcdedit %bcd% /set {bootmgr} displaybootmenu true
bcdedit %bcd% /set {bootmgr} nointegritychecks true

bcdedit %bcd% /create %guid% /d "EFI LOADER" /application bootapp
bcdedit %bcd% /set %guid% device boot
bcdedit %bcd% /set %guid% path \efiloader.efi
bcdedit %bcd% /set %guid% nointegritychecks true
rem Optional: override the EFI application launched by efiloader.
rem bcdedit %bcd% /set %guid% loadoptions \EFI\okr.efi

bcdedit %bcd% /set {bootmgr} displayorder %guid% /addlast
