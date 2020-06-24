
#include <elf.h>

#ifdef __aarch64__
#define funcall(fun, arg) asm volatile ("mov x0, %1\n\t"    \
                                        "blr %0" :: "r" (fun), "r" (arg))
#elif defined(__amd64__)
#include <X64/ProcessorBind.h>
#define funcall(fun, arg) asm volatile ("call %0" :: "r" (fun), "D" (arg))
#endif

#include <Base.h>
#include <Uefi.h>
#include <bootstruct.h>

#define UPPER_HALF_BASE 0xffff800000000000ul

typedef void (*start_fn)(struct bootstruct *bootinfo);

extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *cout;

static void init_upper_half_mapping(struct bootstruct *bootinfo);

static void printn(UINTN n)
{
    if (n > 16) {
        printn(n / 16);
    }
    UINTN mod = (n % 16);
    UINT16 str[2] = { mod >= 10 ? ('a' + (mod - 10)) : '0' + mod, 0 };
    cout->OutputString(cout, (UINT16 *)str);
}


void boot_upper_half(EFI_BOOT_SERVICES *services, EFI_RUNTIME_SERVICES *vmapper, Elf64_Ehdr *kernhdr,
                     UINTN mapkey, struct bootstruct *bootinfo)
{
    cout->OutputString(cout, u"\n\rInitializing upper half identity map... ");
    init_upper_half_mapping(bootinfo);
    cout->OutputString(cout, u"Upper half initialized!\n\rExiting boot services and setting virtual address map...\n\r");
    services->ExitBootServices(services, mapkey);
    vmapper->SetVirtualAddressMap(bootinfo->memmap_size, sizeof *bootinfo->memmap, EFI_MEMORY_DESCRIPTOR_VERSION,
                                  bootinfo->memmap);

    vmapper->ConvertPointer(0, (void **)&kernhdr);
    vmapper->ConvertPointer(0, (void **)&bootinfo);
    vmapper->ConvertPointer(0, (void **)&bootinfo->ramdisk);

    cout->OutputString(cout, u"Kernel entry point at ");
    printn((UINTN)((EFI_VIRTUAL_ADDRESS)kernhdr + kernhdr->e_entry));
    cout->OutputString(cout, u"\n\r");

    start_fn local_start = (start_fn)((EFI_VIRTUAL_ADDRESS)kernhdr + kernhdr->e_entry);
    funcall(local_start, bootinfo);
}

static void init_upper_half_mapping(struct bootstruct *bootinfo)
{
    EFI_PHYSICAL_ADDRESS pmemmap = (EFI_PHYSICAL_ADDRESS)bootinfo->memmap;
    UINTN i;
    for (i = 0; i < bootinfo->memmap_size; i += bootinfo->memmap_entsz) {
        EFI_MEMORY_DESCRIPTOR *memdesc = (EFI_MEMORY_DESCRIPTOR *)(pmemmap + i);
        memdesc->VirtualStart = memdesc->PhysicalStart + UPPER_HALF_BASE;
        cout->OutputString(cout, u"Mapping 0x");
        printn(memdesc->PhysicalStart);
        cout->OutputString(cout, u" -> 0x");
        printn(memdesc->VirtualStart);
        cout->OutputString(cout, u"\n\r");
    }
}
