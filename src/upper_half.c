
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

typedef void (*start_fn)(struct bootstruct *bootinfo);

static void init_upper_half_mapping(EFI_MEMORY_DESCRIPTOR *mmap, UINTN mmap_size);

void boot_upper_half(EFI_BOOT_SERVICES *services, EFI_RUNTIME_SERVICES *vmapper, Elf64_Ehdr *kernhdr,
                     UINTN mapkey, struct bootstruct *bootinfo)
{
    EFI_PHYSICAL_ADDRESS pkernhdr = (EFI_PHYSICAL_ADDRESS)kernhdr + UPPER_HALF_BASE;
    kernhdr = (Elf64_Ehdr *)pkernhdr;

    EFI_PHYSICAL_ADDRESS pbootinfo = (EFI_PHYSICAL_ADDRESS)bootinfo + UPPER_HALF_BASE;
    bootinfo = (struct bootstruct *)pbootinfo;

    services->ExitBootServices(services, mapkey);
    init_upper_half_mapping(bootinfo->memmap, bootinfo->memmap_size);
    vmapper->SetVirtualAddressMap(bootinfo->memmap_size, sizeof *bootinfo->memmap, );

    start_fn local_start = (start_fn)(pkernhdr + kernhdr->e_entry); 
    funcall(local_start, bootinfo);
}

static void init_upper_half_mapping(EFI_MEMORY_DESCRIPTOR *mmap, UINTN mmap_size)
{

}
