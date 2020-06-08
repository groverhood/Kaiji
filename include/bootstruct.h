#ifndef KAIJI_BOOTSTRUCT_H
#define KAIJI_BOOTSTRUCT_H 1

#include <Uefi.h>

#define KAIJIMAG *(UINT64 *)((CHAR8[8]){ 'K', 'a', 'i', 'j', 'i', 'E', 'F', 'I' })

/* Small architecture-agnostic structure used to transfer the Fakix ramdisk
   from the host to the Fakix machine. */
struct bootstruct {
    UINT64 magic;
    UINT64 ramdisk_size;
    UINT8 *ramdisk;
    UINT64 memmap_size;
    EFI_MEMORY_DESCRIPTOR memmap[0];
};

#endif
