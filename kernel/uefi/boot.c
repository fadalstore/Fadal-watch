#include <efi.h>
#include <efilib.h>
#include "net.h"

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

typedef struct _EFI_HTTP_PROTOCOL EFI_HTTP_PROTOCOL;
typedef struct { UINT32 Method; CHAR16 *Url; } FADAL_HTTP_REQUEST_DATA;
typedef struct { UINT32 StatusCode; } FADAL_HTTP_RESPONSE_DATA;
typedef struct { CHAR8 *FieldName; CHAR8 *FieldValue; } FADAL_HTTP_HEADER;
typedef struct {
    union { FADAL_HTTP_REQUEST_DATA *Request; FADAL_HTTP_RESPONSE_DATA *Response; } Data;
    UINTN HeaderCount;
    FADAL_HTTP_HEADER *Headers;
    UINTN BodyLength;
    VOID *Body;
} FADAL_HTTP_MESSAGE;
typedef struct { EFI_EVENT Event; EFI_STATUS Status; FADAL_HTTP_MESSAGE *Message; } FADAL_HTTP_TOKEN;
typedef struct { UINT32 HttpVersion; UINT32 TimeOutMillisec; BOOLEAN LocalAddressIsIPv6; VOID *AccessPoint; } FADAL_HTTP_CONFIG_DATA;
typedef struct { BOOLEAN UseDefaultAddress; UINT8 LocalAddress[4]; UINT8 LocalSubnet[4]; UINT16 LocalPort; } FADAL_HTTPV4_ACCESS_POINT;
typedef EFI_STATUS (EFIAPI *FADAL_HTTP_CONFIGURE)(EFI_HTTP_PROTOCOL *, FADAL_HTTP_CONFIG_DATA *);
typedef EFI_STATUS (EFIAPI *FADAL_HTTP_REQUEST)(EFI_HTTP_PROTOCOL *, FADAL_HTTP_TOKEN *);
typedef EFI_STATUS (EFIAPI *FADAL_HTTP_RESPONSE)(EFI_HTTP_PROTOCOL *, FADAL_HTTP_TOKEN *);
typedef EFI_STATUS (EFIAPI *FADAL_HTTP_POLL)(EFI_HTTP_PROTOCOL *);
struct _EFI_HTTP_PROTOCOL {
    VOID *GetModeData;
    FADAL_HTTP_CONFIGURE Configure;
    FADAL_HTTP_REQUEST Request;
    VOID *Cancel;
    FADAL_HTTP_RESPONSE Response;
    FADAL_HTTP_POLL Poll;
};
static EFI_GUID gFadalHttpProtocolGuid = {
    0x7a59b29b, 0x910b, 0x4171, {0x82, 0x42, 0xa8, 0x5a, 0x0d, 0xf2, 0x5b, 0x5b}
};
static EFI_GUID gFadalHttpBindingGuid = {
    0xbdc8e6af, 0xd9bc, 0x4379, {0xa7, 0x2a, 0xe0, 0xc4, 0xe7, 0x5d, 0xae, 0x1c}
};
typedef struct _FADAL_SERVICE_BINDING FADAL_SERVICE_BINDING;
typedef EFI_STATUS (EFIAPI *FADAL_CREATE_CHILD)(FADAL_SERVICE_BINDING *, EFI_HANDLE *);
typedef EFI_STATUS (EFIAPI *FADAL_DESTROY_CHILD)(FADAL_SERVICE_BINDING *, EFI_HANDLE);
struct _FADAL_SERVICE_BINDING {
    FADAL_CREATE_CHILD CreateChild;
    FADAL_DESTROY_CHILD DestroyChild;
};
typedef void (*FADAL_UEFI_ENTRY)(FADAL_FRAMEBUFFER *, fadal_uefi_services *);

static void __attribute__((unused)) debug_marker(const char *text) {
    while (*text != '\0') {
        __asm__ volatile ("outb %0, %1" : : "a"((UINT8)*text++), "Nd"((UINT16)0x402));
    }
}
static EFI_FILE_HANDLE g_fadal_root;
static EFI_HTTP_PROTOCOL *g_fadal_http;
static EFI_HANDLE g_fadal_http_child;
static fadal_net_u32 fadal_http_download(const char *url, const char *name) {
    CHAR16 wide_url[256];
    CHAR16 wide_name[64];
    UINTN url_length = 0;
    UINTN name_length = 0;
    UINTN index;
    EFI_STATUS status;
    EFI_FILE_HANDLE file = NULL;
    VOID *buffer = NULL;
    UINTN buffer_size = 65536;
    UINTN write_size;
    FADAL_HTTP_REQUEST_DATA request_data;
    FADAL_HTTP_RESPONSE_DATA response_data;
    FADAL_HTTP_HEADER request_header;
    FADAL_HTTP_MESSAGE request_message;
    FADAL_HTTP_MESSAGE response_message;
    FADAL_HTTP_TOKEN request_token;
    FADAL_HTTP_TOKEN response_token;
    FADAL_HTTP_CONFIG_DATA config_data;
    FADAL_HTTPV4_ACCESS_POINT access_point;
    if (g_fadal_http == NULL || g_fadal_root == NULL || url == NULL || name == NULL) return 0;
    while (url[url_length] != '\0' && url_length + 1 < sizeof(wide_url) / sizeof(wide_url[0])) url_length++;
    while (name[name_length] != '\0' && name_length + 1 < sizeof(wide_name) / sizeof(wide_name[0])) name_length++;
    if (url[url_length] != '\0' || name[name_length] != '\0' || url_length < 7 ||
        url[0] != 'h' || url[1] != 't' || url[2] != 't' || url[3] != 'p' || url[4] != ':' ||
        url[5] != '/' || url[6] != '/') return 0;
    for (index = 0; index < url_length; index++) wide_url[index] = (CHAR16)(UINT8)url[index];
    for (index = 0; index < name_length; index++) wide_name[index] = (CHAR16)(UINT8)name[index];
    wide_url[url_length] = 0;
    wide_name[name_length] = 0;
    config_data.HttpVersion = 1;
    config_data.TimeOutMillisec = 5000;
    config_data.LocalAddressIsIPv6 = FALSE;
    access_point.UseDefaultAddress = TRUE;
    access_point.LocalPort = 0;
    config_data.AccessPoint = &access_point;
    request_data.Method = 0;
    request_data.Url = wide_url;
    request_header.FieldName = (CHAR8 *)"Accept";
    request_header.FieldValue = (CHAR8 *)"*/*";
    request_message.Data.Request = &request_data;
    request_message.HeaderCount = 1;
    request_message.Headers = &request_header;
    request_message.BodyLength = 0;
    request_message.Body = NULL;
    request_token.Event = NULL;
    status = uefi_call_wrapper(BS->CreateEvent, 6, EVT_NOTIFY_SIGNAL, TPL_CALLBACK,
                               NULL, NULL, NULL, &request_token.Event);
    if (EFI_ERROR(status)) goto cleanup;
    request_token.Status = EFI_NOT_READY;
    request_token.Message = &request_message;
    response_data.StatusCode = 0;
    response_message.Data.Response = &response_data;
    response_message.HeaderCount = 0;
    response_message.Headers = NULL;
    response_message.BodyLength = buffer_size;
    response_message.Body = NULL;
    response_token.Event = NULL;
    status = uefi_call_wrapper(BS->CreateEvent, 6, EVT_NOTIFY_SIGNAL, TPL_CALLBACK,
                               NULL, NULL, NULL, &response_token.Event);
    if (EFI_ERROR(status)) goto cleanup;
    response_token.Status = EFI_NOT_READY;
    response_token.Message = &response_message;
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiBootServicesData, buffer_size, &buffer);
    if (EFI_ERROR(status)) return 0;
    response_message.Body = buffer;
    status = g_fadal_http->Configure(g_fadal_http, &config_data);
    if (EFI_ERROR(status)) goto cleanup;
    status = g_fadal_http->Request(g_fadal_http, &request_token);
    if (EFI_ERROR(status)) goto cleanup;
    for (index = 0; index < 10000 && request_token.Status == EFI_NOT_READY; index++) {
        g_fadal_http->Poll(g_fadal_http);
        uefi_call_wrapper(BS->Stall, 1, 1000);
    }
    if (EFI_ERROR(request_token.Status)) goto cleanup;
    status = g_fadal_http->Response(g_fadal_http, &response_token);
    if (EFI_ERROR(status)) goto cleanup;
    for (index = 0; index < 10000 && response_token.Status == EFI_NOT_READY; index++) {
        g_fadal_http->Poll(g_fadal_http);
        uefi_call_wrapper(BS->Stall, 1, 1000);
    }
    if (EFI_ERROR(response_token.Status) || response_data.StatusCode != 200) goto cleanup;
    status = uefi_call_wrapper(g_fadal_root->Open, 5, g_fadal_root, &file, wide_name,
                               EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(status)) goto cleanup;
    write_size = response_message.BodyLength;
    status = uefi_call_wrapper(file->Write, 3, file, &write_size, buffer);
    uefi_call_wrapper(file->Close, 1, file);
    if (EFI_ERROR(status) || write_size == 0) goto cleanup;
    uefi_call_wrapper(BS->FreePool, 1, buffer);
    return (fadal_net_u32)write_size;
cleanup:
    if (request_token.Event != NULL) uefi_call_wrapper(BS->CloseEvent, 1, request_token.Event);
    if (response_token.Event != NULL) uefi_call_wrapper(BS->CloseEvent, 1, response_token.Event);
    if (file != NULL) uefi_call_wrapper(file->Close, 1, file);
    if (buffer != NULL) uefi_call_wrapper(BS->FreePool, 1, buffer);
    return 0;
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
    FADAL_UEFI_ENTRY entry;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    FADAL_FRAMEBUFFER framebuffer;
    fadal_uefi_services services;

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
    g_fadal_root = root;
    g_fadal_http = NULL;
    g_fadal_http_child = NULL;
    status = uefi_call_wrapper(BS->LocateProtocol, 3, &gFadalHttpProtocolGuid, NULL,
                               (VOID **)&g_fadal_http);
    if (EFI_ERROR(status) || g_fadal_http == NULL) {
        FADAL_SERVICE_BINDING *binding = NULL;
        status = uefi_call_wrapper(BS->LocateProtocol, 3, &gFadalHttpBindingGuid, NULL,
                                   (VOID **)&binding);
        if (!EFI_ERROR(status) && binding != NULL &&
            !EFI_ERROR(binding->CreateChild(binding, &g_fadal_http_child))) {
            status = uefi_call_wrapper(BS->OpenProtocol, 6, g_fadal_http_child,
                                       &gFadalHttpProtocolGuid, (VOID **)&g_fadal_http,
                                       image, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        }
    }
    if (EFI_ERROR(status) || g_fadal_http == NULL) {
        g_fadal_http = NULL;
        Print(L"HTTP service unavailable; desktop downloads disabled\r\n");
    } else {
        Print(L"UEFI HTTP service available; desktop downloads enabled\r\n");
    }

    status = read_payload(root, &payload, &payload_size, &entry_offset);
    if (EFI_ERROR(status)) {
        Print(L"error: FADAL.KRN is not a supported Fadal64 payload: %r\r\n", status);
        return status;
    }

    Print(L"payload loaded (%lu bytes)\r\n", payload_size);
    services.http_download = fadal_http_download;
    services.http_available = g_fadal_http != NULL;
    entry = (FADAL_UEFI_ENTRY)((UINT8 *)payload + entry_offset);
    entry(&framebuffer, &services);
    return EFI_SUCCESS;
}
