#ifndef KAIJI_RAMDISK_H
#define KAIJI_RAMDISK_H 1

#include <Uefi.h>
#include <bootstruct.h>

/**
 * The Fakix ramdisk comes in the format of a tarball that assumes
 * the role of the root directory.
 * 
 * As such, iterating through its entries is best accomplished
 * through the usage of the following functions exposed in this
 * header file.
 **/

/* Find and return the pointer to the desired ramdisk entry, if
   such an entry exists. Otherwise return NULL. */
UINT8 *ramdisk_find(UINT8 *rd, CHAR8 *ent);

/* Execute a ramdisk entry. Should not return if successful. */
EFI_STATUS ramdisk_exec(UINT8 *rdent, struct bootstruct *bootinfo);

#endif
