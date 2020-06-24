#include <elf.h>
#include <Base.h>
#include <Uefi.h>
#include <ramdisk.h>

struct posix_header {           /* byte offset */
    char name[100];               /*   0 */
    char mode[8];                 /* 100 */
    char uid[8];                  /* 108 */
    char gid[8];                  /* 116 */
    char size[12];                /* 124 */
    char mtime[12];               /* 136 */
    char chksum[8];               /* 148 */
    char typeflag;                /* 156 */
    char linkname[100];           /* 157 */
    char magic[6];                /* 257 */
    char version[2];              /* 263 */
    char uname[32];               /* 265 */
    char gname[32];               /* 297 */
    char devmajor[8];             /* 329 */
    char devminor[8];             /* 337 */
    char prefix[167];             /* 345 */
                                /* 512 */
};

static int strcmp(const char *s1, const char *s2)
{
    int cval;
    while ((cval = *s1 - *s2) == 0 && *s1 != 0 && *s2 != 0) {
        s1++;
        s2++;
    }
    return cval;
}

extern void boot_upper_half(EFI_BOOT_SERVICES *services, EFI_RUNTIME_SERVICES *vmapper, Elf64_Ehdr *kernhdr, UINTN mapkey, struct bootstruct *bootinfo);
extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *cout;

static void pmagic(Elf64_Ehdr *kernhdr)
{
    CHAR16 mag[19];
    UINTN i;
    for (i = 0; i < 16; ++i) {
        mag[i] = kernhdr->e_ident[i];
    }
    mag[16] = '\n';
    mag[17] = '\r';
    mag[18] = 0;
    cout->OutputString(cout, mag);
}

EFI_STATUS ramdisk_exec(UINT8* rdent, EFI_SYSTEM_TABLE *systab, UINTN mapkey, struct bootstruct *bootinfo)
{
    struct posix_header *hdr = (struct posix_header *)rdent;
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(hdr + 1);

    cout->OutputString(cout, u"Kernel ELF header ident: ");
    pmagic(ehdr);

    boot_upper_half(systab->BootServices, systab->RuntimeServices, ehdr, mapkey, bootinfo);
    
    return !EFI_SUCCESS;
}

UINT8 *ramdisk_find(UINT8 *rd, CHAR8 *ent)
{
    while (strcmp((char *)rd, ent) != 0) {
      rd++;
    }
   
    return rd;
}
