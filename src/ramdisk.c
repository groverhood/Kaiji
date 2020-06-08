#include <elf.h>
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
  char prefix[155];             /* 345 */
                                /* 500 */
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

typedef void start_func(struct bootstruct *bootinfo);

EFI_STATUS ramdisk_exec(UINT8* rdent, struct bootstruct *bootinfo)
{
    struct posix_header *hdr = (struct posix_header *)rdent;
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(hdr + 1);

    start_func *start = (start_func *)ehdr->e_entry;
    start(bootinfo);
    
    return !EFI_SUCCESS;
}

UINT8 *ramdisk_find(UINT8 *rd, CHAR8 *ent)
{
    while (strcmp((char *)rd, ent) != 0) {
	rd++;
    }
    return rd;
}
