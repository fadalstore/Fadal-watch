#include <efi.h>
#include <efilib.h>

#define FADAL_UEFI_MAGIC 0x46444c3634ULL /* "FDL64" */
#define FADAL_UEFI_VERSION 1U
#define FADAL_UEFI_MAX_IMAGE (64U * 1024U * 1024U)

typedef struct {
    UINT64 magic;
    UINT32 version;
    UINT32 header_size;
    UINT64 image_size;
    UINT64 entry_offset;
} FADAL_UEFI_HEADER;

typedef struct {
    UINT64 base;
    UINT32 width;
    UINT32 height;
    UINT32 pixels_per_scanline;
    UINT32 pixel_format;
} FADAL_FRAMEBUFFER;

typedef void (*FADAL_UEFI_ENTRY)(FADAL_FRAMEBUFFER *);

static void debug_marker(const char *text) {
    while (*text != '\0') {
        __asm__ volatile ("outb %0, %1" : : "a"((UINT8)*text++), "Nd"((UINT16)0x402));
    }
}

static EFI_STATUS read_payload(EFI_FILE_HANDLE root, VOID **payload,
                               UINTN *payload_size, UINT64 *entry_offset) {
    EFI_FILE_HANDLE file = NULL;
    EFI_FILE_INFO *info = NULL;
    UINTN info_size = 0;
    UINTN read_size;
    UINTN image_size;
    EFI_STATUS status;
    FADAL_UEFI_HEADER header;

    status = uefi_call_wrapper(root->Open, 5, root, &file,
                               L"\\FADAL.KRN", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        return status;
    }

    status = uefi_call_wrapper(file->GetInfo, 4, file, &gEfiFileInfoGuid,
                               &info_size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL || info_size == 0) {
        uefi_call_wrapper(file->Close, 1, file);
        return EFI_LOAD_ERROR;
    }
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, info_size,
                               (VOID **)&info);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(file->Close, 1, file);
        return status;
    }
    status = uefi_call_wrapper(file->GetInfo, 4, file, &gEfiFileInfoGuid,
                               &info_size, info);
    if (EFI_ERROR(status) || info->FileSize < sizeof(header) ||
        info->FileSize > FADAL_UEFI_MAX_IMAGE) {
        uefi_call_wrapper(BS->FreePool, 1, info);
        uefi_call_wrapper(file->Close, 1, file);
        return EFI_LOAD_ERROR;
    }

    image_size = (UINTN)info->FileSize;
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
                               image_size, payload);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(BS->FreePool, 1, info);
        uefi_call_wrapper(file->Close, 1, file);
        return status;
    }
    read_size = image_size;
    status = uefi_call_wrapper(file->Read, 3, file, &read_size, *payload);
    uefi_call_wrapper(file->Close, 1, file);
    uefi_call_wrapper(BS->FreePool, 1, info);
    if (EFI_ERROR(status) || read_size != image_size) {
        uefi_call_wrapper(BS->FreePool, 1, *payload);
        *payload = NULL;
        return EFI_LOAD_ERROR;
    }

    CopyMem(&header, *payload, sizeof(header));
    if (header.magic != FADAL_UEFI_MAGIC ||
        header.version != FADAL_UEFI_VERSION ||
        header.header_size < sizeof(header) ||
        header.image_size != image_size ||
        header.entry_offset < header.header_size ||
        header.entry_offset >= header.image_size) {
        uefi_call_wrapper(BS->FreePool, 1, *payload);
        *payload = NULL;
        return EFI_UNSUPPORTED;
    }

    *payload_size = image_size;
    *entry_offset = header.entry_offset;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system_table) {
    EFI_LOADED_IMAGE *loaded_image = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *filesystem = NULL;
    EFI_FILE_HANDLE root = NULL;
    EFI_STATUS status;
    VOID *payload = NULL;
    UINTN payload_size = 0;
    UINT64 entry_offset = 0;
    UINTN memory_map_size = 0;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;
    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
    FADAL_UEFI_ENTRY entry;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    FADAL_FRAMEBUFFER framebuffer;

    InitializeLib(image, system_table);
    Print(L"Fadal UEFI loader\r\n");

    framebuffer.base = 0;
    framebuffer.width = 0;
    framebuffer.height = 0;
    framebuffer.pixels_per_scanline = 0;
    framebuffer.pixel_format = 0xffffffffU;
    status = uefi_call_wrapper(BS->LocateProtocol, 3,
                               &gEfiGraphicsOutputProtocolGuid, NULL,
                               (VOID **)&gop);
    if (!EFI_ERROR(status) && gop != NULL && gop->Mode != NULL &&
        gop->Mode->Info != NULL) {
        framebuffer.base = gop->Mode->FrameBufferBase;
        framebuffer.width = gop->Mode->Info->HorizontalResolution;
        framebuffer.height = gop->Mode->Info->VerticalResolution;
        framebuffer.pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;
        framebuffer.pixel_format = gop->Mode->Info->PixelFormat;
        Print(L"GOP framebuffer %ux%u\r\n", framebuffer.width, framebuffer.height);
    } else {
        Print(L"GOP framebuffer unavailable; text fallback\r\n");
    }

    status = uefi_call_wrapper(BS->HandleProtocol, 3, image,
                               &gEfiLoadedImageProtocolGuid,
                               (VOID **)&loaded_image);
    if (EFI_ERROR(status)) {
        Print(L"error: loaded-image protocol: %r\r\n", status);
        return status;
    }
    status = uefi_call_wrapper(BS->HandleProtocol, 3, loaded_image->DeviceHandle,
                               &gEfiSimpleFileSystemProtocolGuid,
                               (VOID **)&filesystem);
    if (EFI_ERROR(status)) {
        Print(L"error: filesystem protocol: %r\r\n", status);
        return status;
    }
    status = uefi_call_wrapper(filesystem->OpenVolume, 2, filesystem, &root);
    if (EFI_ERROR(status)) {
        Print(L"error: open ESP: %r\r\n", status);
        return status;
    }

    status = read_payload(root, &payload, &payload_size, &entry_offset);
    uefi_call_wrapper(root->Close, 1, root);
    if (EFI_ERROR(status)) {
        Print(L"error: FADAL.KRN is not a supported Fadal64 payload: %r\r\n", status);
        return status;
    }

    Print(L"payload loaded (%lu bytes)\r\n", payload_size);
    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &memory_map_size,
                               memory_map, &map_key, &descriptor_size,
                               &descriptor_version);
    if (status != EFI_BUFFER_TOO_SMALL || descriptor_size == 0) {
        uefi_call_wrapper(BS->FreePool, 1, payload);
        return EFI_LOAD_ERROR;
    }
    memory_map_size += descriptor_size * 2;
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
                               memory_map_size, (VOID **)&memory_map);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(BS->FreePool, 1, payload);
        return status;
    }
    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &memory_map_size,
                               memory_map, &map_key, &descriptor_size,
                               &descriptor_version);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(BS->FreePool, 1, memory_map);
        uefi_call_wrapper(BS->FreePool, 1, payload);
        return status;
    }
    debug_marker("BEFORE_EBS\n");
    status = uefi_call_wrapper(BS->ExitBootServices, 2, image, map_key);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(BS->FreePool, 1, memory_map);
        uefi_call_wrapper(BS->FreePool, 1, payload);
        return status;
    }

    debug_marker("AFTER_EBS\n");
    entry = (FADAL_UEFI_ENTRY)((UINT8 *)payload + entry_offset);
    entry(&framebuffer);
    return EFI_SUCCESS;
}
