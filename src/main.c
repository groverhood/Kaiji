#include <Base.h>
#include <Uefi.h>
#include <Uefi/UefiBaseType.h>
#include <Protocol/SimpleNetwork.h>

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


#define DIVIDE_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

/* 16 kB region. */
#define BOOTINFO_NPAGES 4

struct bootinfo {
    UINT64 ramdisk_size;
    UINT8 *ramdisk;
    UINT64 memmap_size;
    EFI_MEMORY_DESCRIPTOR memmap[0];
};

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    EFI_STATUS status;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *cout = systab->ConOut;
    cout->ClearScreen(cout);
    
    EFI_BOOT_SERVICES *services = systab->BootServices;
    EFI_GUID netguid = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;
    EFI_SIMPLE_NETWORK_PROTOCOL *netp;
    EFI_HANDLE nethandle;
    UINTN handle_size = 0;
    
    status = services->LocateHandle(ByProtocol, &netguid, NULL, &handle_size, NULL);
    if (status != EFI_SUCCESS) {
	cout->OutputString(cout, u"Couldn't locate simple network protocol handle");
	return status;
    }
 
    status = services->AllocatePages(AllocateAddress, EfiLoaderData,
				     DIVIDE_ROUND_UP(handle_size, 0x1000),
				     (EFI_PHYSICAL_ADDRESS *)&nethandle);
    if (status != EFI_SUCCESS) {
	return status;
    }
    
    status = services->OpenProtocol(nethandle, &netguid, (void **)&netp,
				    image, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (status != EFI_SUCCESS) {
	return status;
    }

    netp->Initialize(netp, 0, 0);
    netp->Start(netp);
    
    struct bootinfo *bootinfo;
    status = services->AllocatePages(AllocateAddress, EfiLoaderData,
				     BOOTINFO_NPAGES, (EFI_PHYSICAL_ADDRESS *)&bootinfo);
    if (status != EFI_SUCCESS) {
	return status;
    }

    struct bootstruct *bs;
    status = services->AllocatePages(AllocateAddress, EfiLoaderData,
				     BOOTINFO_NPAGES, (EFI_PHYSICAL_ADDRESS *)&bs);
    if (status != EFI_SUCCESS) {
	return status;
    }

    bs->magic = KAIJIMAG;
    /* Transmit magic */
    status = netp->Transmit(netp, 0, sizeof *bs, bs, NULL, NULL, NULL);
    if (status != EFI_SUCCESS) {
	return status;
    }
    
    UINTN bs_size = BOOTINFO_NPAGES;
    /* Receive ramdisk */
    status = netp->Receive(netp, NULL, &bs_size, bs, NULL, NULL, NULL);
    if (status != EFI_SUCCESS) {
	return status;
    }
    
    status = netp->Stop(netp);
    if (status != EFI_SUCCESS) {
	return status;
    }

    bootinfo->ramdisk = bs->ramdisk;
    bootinfo->ramdisk_size = bs->ramdisk_size;
    
    status = services->CloseProtocol(nethandle, &netguid, image, NULL);
    if (status != EFI_SUCCESS) {
	return status;
    }

    bootinfo->memmap_size = BOOTINFO_NPAGES - sizeof *bootinfo;
    status = services->GetMemoryMap(&bootinfo->memmap_size, bootinfo->memmap,
				    NULL, NULL, NULL);
    if (status != EFI_SUCCESS) {
	return status;
    }
    
    cout->OutputString(cout, u"Got to end!\n\r");
    while (1);
    
    UINT8 *cpu_driver = ramdisk_find(bootinfo->ramdisk, "/sbin/cpu_driver");
    if (cpu_driver == NULL) {
	return EFI_LOAD_ERROR;
    }

    return ramdisk_exec(cpu_driver);
}
