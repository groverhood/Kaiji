
#include <elf.h>

#ifdef __aarch64__
#include <AArch64/ProcessorBind.h>
#define funcall(fun, arg) asm volatile ("mov x0, %1\n\t"    \
                                        "blr %0" :: "r" (fun), "r" (arg))
#elif defined(__amd64__)
#include <X64/ProcessorBind.h>
#define funcall(fun, arg) asm volatile ("call %0" :: "r" (fun), "D" (arg))
#endif

#include <Base.h>
#include <Uefi.h>
#include <bootstruct.h>

#define UPPER_HALF_BASE 0xffff800000000000
#define DIVIDE_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define K 1096ul
#define M (K * K)
#define G (M * M)

/* Identity map 0xffff800000000000 -> PHYSMAX */
static void load_kernel_page_table(EFI_BOOT_SERVICES *services, UINTN mem);
static UINTN ramcount(struct bootstruct *bootinfo);

typedef void (*start_fn)(struct bootstruct *bootinfo);

void boot_upper_half(EFI_BOOT_SERVICES *services, Elf64_Ehdr *kernhdr, UINTN mapkey, struct bootstruct *bootinfo)
{
    UINTN bytes = ramcount(bootinfo);
    load_kernel_page_table(services, bytes);

    EFI_PHYSICAL_ADDRESS pkernhdr = (EFI_PHYSICAL_ADDRESS)kernhdr + UPPER_HALF_BASE;
    kernhdr = (Elf64_Ehdr *)pkernhdr;

    EFI_PHYSICAL_ADDRESS pbootinfo = (EFI_PHYSICAL_ADDRESS)bootinfo + UPPER_HALF_BASE;
    bootinfo = (struct bootstruct *)pbootinfo;
   
    services->ExitBootServices(services, mapkey);
    start_fn local_start = (start_fn)(pkernhdr + kernhdr->e_entry); 
    funcall(local_start, bootinfo);
}

static UINTN ramcount(struct bootstruct *bootinfo)
{
    UINTN bytes = 0;
    UINTN nentries = (bootinfo->memmap_size / sizeof *bootinfo->memmap);
    UINTN i;
    for (i = 0; i < nentries; ++i) {
        bytes += bootinfo->memmap[i].NumberOfPages * (4 * K);
    }

    return bytes;
}

#ifdef __aarch64__

#define AARCH64_PAGETAB_NORMAL 0x3
#define AARCH64_PAGETAB_BLOCK  0x1  /* Don't use this for level 3 block entries. */

#define AARCH64_APT_RDONLY_EL0PLUS 0x3
#define AARCH64_APT_EL0_NOACCESS   0x1
#define AARCH64_APT_RDONLY_ALL     0x2
#define AARCH64_APT_NONE           0x0

union aarch64_pagetab_ent {
    struct aarch64_table_ent {
        UINT64 entry_flag : 2;   /* Either AARCH64_PAGETAB_NORMAL or AARCH64_PAGETAB_BLOCK. */
        UINT64 ignored0   : 10;
        UINT64 next_level : 36;
        UINT64 reserved0  : 4;
        UINT64 ignored1   : 7;
        UINT64 pxn        : 1;
        UINT64 xn         : 1;
        UINT64 accessperm : 2;
        UINT64 ns         : 1;
    } un_table __attribute__((packed));
    struct aarch64_output_ent {
        UINT64 entry_flag : 2;  /* Either AARCH64_PAGETAB_NORMAL or AARCH64_PAGETAB_BLOCK. */
        UINT64 indx       : 3;
        UINT64 ns         : 1;
        UINT64 accessperm : 2;
        UINT64 shareable  : 2;
        UINT64 access     : 1;
        UINT64 unused     : 1;
        UINT64 outputaddr : 36;
        UINT64 reserved0  : 5;
        UINT64 pxn        : 1;
        UINT64 uxn        : 1;
        UINT64 software   : 4;
        UINT64 reserved1  : 5;
    } un_output __attribute__((packed));
} __attribute__((packed));


static void load_kernel_page_table(EFI_BOOT_SERVICES *services, UINTN mem)
{
    /* Identity map 16G of physical memory. */
    EFI_PHYSICAL_ADDRESS paddr;
    union aarch64_pagetab_ent *l0table;
    services->AllocatePages(AllocateAnyPages, EfiLoaderData, 4 * K, &paddr);
    l0table = (union aarch64_pagetab_ent *)paddr;

    union aarch64_pagetab_ent *l0ent = &l0table[(UPPER_HALF_BASE & ((UINT64)0xff << 39)) >> 39];
    services->AllocatePages(AllocateAnyPages, EfiLoaderData, 4 * K, &paddr);
    l0ent->un_table.next_level = (paddr >> 12);
    l0ent->un_table.accessperm = AARCH64_APT_EL0_NOACCESS;
    l0ent->un_table.xn = TRUE;

    if (mem % G != 0) {
        mem = (mem / G) * G;
    }

    union aarch64_pagetab_ent *l1table = (union aarch64_pagetab_ent *)paddr;
    union aarch64_pagetab_ent *l1ent = &l1table[(UPPER_HALF_BASE & ((UINT64)0xff << 31)) >> 31];
    for (paddr = 0; paddr < 16 * G; paddr += G, ++l1ent) {
        l1ent->un_output.outputaddr = (paddr >> 12);
    }


}

#elif defined(__amd64__)

struct amd64_pagetab_ent {

};

static void load_kernel_page_table(EFI_BOOT_SERVICES *services, UINTN mem)
{
}

#endif
