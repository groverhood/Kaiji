#include <Uefi.h>

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
  systab->ConOut->ClearScreen(systab->ConOut);
  systab->ConOut->OutputString(systab->ConOut, u"Hello, world!\n");
  return RETURN_SUCCESS;
}
