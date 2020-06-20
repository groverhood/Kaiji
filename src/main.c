#include <Base.h>
#include <Uefi.h>
#include <Guid/FileInfo.h>
#include <Uefi/UefiBaseType.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/SimpleFileSystem.h>

#include <bootstruct.h>
#include <ramdisk.h>

/**
   typedef
   EFI_STATUS
   (EFIAPI *EFI_ALLOCATE_PAGES) (
     IN EFI_ALLOCATE_TYPE Type,
     IN EFI_MEMORY_TYPE MemoryType,
     IN UINTN Pages,
     IN OUT EFI_PHYSICAL_ADDRESS *Memory
   );

   typedef
   EFI_STATUS
   (EFIAPI *EFI_GET_MEMORY_MAP) (   
     IN OUT UINTN *MemoryMapSize,   
     OUT EFI_MEMORY_DESCRIPTOR *MemoryMap,   
     OUT UINTN *MapKey,   
     OUT UINTN *DescriptorSize,   
     OUT UINT32 *DescriptorVersion   
   );
 **/


#define K 1096
#define DIVIDE_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

/* 16 kB region. */
#define BOOTINFO_NPAGES 4

static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *cout;

static EFI_STATUS get_ramdisk_snp(EFI_HANDLE image, EFI_BOOT_SERVICES *services, UINTN *mapkey, struct bootstruct **bootinfo);
static EFI_STATUS get_ramdisk_file(EFI_HANDLE image, EFI_BOOT_SERVICES *services, UINTN *mapkey, struct bootstruct **bootinfo);

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    UINTN key;
    EFI_STATUS status;
    cout = systab->ConOut;
    cout->ClearScreen(cout);
    
    EFI_BOOT_SERVICES *services = systab->BootServices;
    struct bootstruct *bootinfo;
    status = get_ramdisk_snp(image, services, &key, &bootinfo);
    if (status != EFI_SUCCESS) {
        cout->OutputString(cout, u"Couldn't find SNP driver, searching for ramdisk in boot partition\n\r");
        status = get_ramdisk_file(image, services, &key, &bootinfo);
        if (status != EFI_SUCCESS) {
            cout->OutputString(cout, u"Couldn't find SFS driver, returning with failing exit code\n\r");
            return status;
        }
    }
        
    UINT8 *cpu_driver = ramdisk_find(bootinfo->ramdisk, "sbin/cpu_driver");
    if (cpu_driver == NULL) {
        return EFI_LOAD_ERROR;
    }

    bootinfo->magic = KAIJIMAG;
    return ramdisk_exec(cpu_driver, services, key, bootinfo);
}

static EFI_STATUS get_memory_map(EFI_HANDLE image, EFI_BOOT_SERVICES *services, UINTN *mapkey, struct bootstruct **bootinfo)
{
    EFI_STATUS status;
    UINTN memmap_size, bootinfo_size;
    services->GetMemoryMap(&memmap_size, NULL, NULL, NULL, NULL);
    bootinfo_size = DIVIDE_ROUND_UP(memmap_size + sizeof **bootinfo, 4 * K) * (4 * K);

    status = services->AllocatePages(AllocateAnyPages, EfiLoaderData, bootinfo_size / (4 * K),
                                     (EFI_PHYSICAL_ADDRESS *)bootinfo);
    if (status != EFI_SUCCESS) {
        cout->OutputString(cout, u"Failed to allocate bootinfo struct\n\r");
        return status;
    }

    (*bootinfo)->memmap_size = memmap_size;
    status = services->GetMemoryMap(&(*bootinfo)->memmap_size, (*bootinfo)->memmap,
                                    mapkey, NULL, NULL);
    if (status) {
        cout->OutputString(cout, u"Failed to get EFI memory map\n\r");
        return status;
    }
    
    return status;
}

static EFI_STATUS get_ramdisk_file(EFI_HANDLE image, EFI_BOOT_SERVICES *services, UINTN *mapkey, struct bootstruct **bootinfo)
{
    EFI_STATUS status;
    EFI_GUID fileguid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID getinfoid = EFI_FILE_INFO_ID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs;
    union {
        EFI_FILE_INFO rdinfo;
        UINT8 raw[sizeof(EFI_FILE_INFO) + sizeof u"\\fakixrd"];
    } un;
    UINTN rdinfo_size = sizeof un.rdinfo + sizeof u"\\fakixrd";
    EFI_FILE_PROTOCOL *rootdir = NULL;
    EFI_FILE_PROTOCOL *ramdisk = NULL;

    status = services->LocateProtocol(&fileguid, NULL, (VOID **)&sfs);
    if (status != EFI_SUCCESS) {
        cout->OutputString(cout, u"LocateProtocol() failed\n\r");
        return status;
    }

    status = sfs->OpenVolume(sfs, &rootdir);
    if (status != EFI_SUCCESS) {
        cout->OutputString(cout, u"OpenVolume() failed\n\r");
    }

    status = rootdir->Open(rootdir, &ramdisk, u"\\fakixrd", EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS) {
        cout->OutputString(cout, u"Failed to open ramdisk\n\r");
        rootdir->Close(rootdir);
        return status;
    }

    status = ramdisk->GetInfo(ramdisk, &getinfoid, &rdinfo_size, &un.rdinfo);
    if (status != EFI_SUCCESS) {
        cout->OutputString(cout, u"Failed to stat ramdisk\n\r");
        if (status == EFI_BUFFER_TOO_SMALL) {
            cout->OutputString(cout, u"Buffer too small\n\r");
        }
        goto cleanup;
    }

    UINT8 *ramdisk_buffer;
    UINT64 filesize = un.rdinfo.FileSize;
    UINT64 ramdisk_size = DIVIDE_ROUND_UP(filesize, 4 * K) * (4 * K);
    status = services->AllocatePages(AllocateAnyPages, EfiLoaderData, ramdisk_size / (4 * K),
                                     (EFI_PHYSICAL_ADDRESS *)&ramdisk_buffer);
    if (status != EFI_SUCCESS) {
        cout->OutputString(cout, u"Failed to allocate ramdisk memory\n\r");
        goto cleanup;
    }

    status = ramdisk->Read(ramdisk, &ramdisk_size, (VOID *)ramdisk_buffer);
    if (status != EFI_SUCCESS) {
        cout->OutputString(cout, u"Failed to read ramdisk into memory\n\r");
        goto cleanup;
    }

    status = get_memory_map(image, services, mapkey, bootinfo);
    if (status != EFI_SUCCESS) {
        cout->OutputString(cout, u"Failed to get memory map\n\r");
        goto cleanup;
    }

    (*bootinfo)->ramdisk = ramdisk_buffer;
    (*bootinfo)->ramdisk_size = ramdisk_size;

cleanup:
    cout->OutputString(cout, u"\n\rCleaning up streams\n\r");
    if (ramdisk != NULL) {
        ramdisk->Close(ramdisk);
    }
    if (rootdir != NULL) {
        rootdir->Close(rootdir);
    }
    return status;
}


static EFI_STATUS get_ramdisk_snp(EFI_HANDLE image, EFI_BOOT_SERVICES *services, UINTN *mapkey, struct bootstruct **bootinfo)
{
    return !EFI_SUCCESS;
}
