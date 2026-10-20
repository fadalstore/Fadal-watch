#include <efi.h>
#include <efilib.h>
#include "net.h"
#include "page_tables.h"

#define FADAL_UEFI_MAGIC 0x46444c3634ULL /* "FDL64" */
#define FADAL_UEFI_VERSION_LEGACY 2U
#define FADAL_UEFI_VERSION_CURRENT 3U
#define FADAL_UEFI_MAX_IMAGE (64U * 1024U * 1024U)
#define FADAL_LAYOUT_MAGIC 0x314d5046U /* "FPM1" */
#define FADAL_LAYOUT_VERSION 1U
#define FADAL_PAGE_SIZE 4096U

typedef struct {
    UINT64 magic;
    UINT32 version;
    UINT32 header_size;
    UINT64 memory_size;
    UINT64 entry_offset;
} FADAL_UEFI_HEADER;

typedef struct {
    UINT32 magic;
    UINT32 version;
    UINT32 text_start;
    UINT32 text_end;
    UINT32 rodata_start;
    UINT32 rodata_end;
    UINT32 data_start;
    UINT32 data_end;
    UINT32 bss_start;
    UINT32 memory_end;
} FADAL_MEMORY_LAYOUT;

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
static EFI_GUID gFadalFileInfoGuid = EFI_FILE_INFO_ID;
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
static EFI_FILE_HANDLE g_fadal_home;
static EFI_HTTP_PROTOCOL *g_fadal_http;
static EFI_HANDLE g_fadal_http_child;
static fadal_net_u32 fadal_home_path(const char *path, CHAR16 wide_path[128],
                                     int allow_empty) {
    UINTN input = 0;
    UINTN output = 0;
    UINTN component_start = 0;
    UINTN component_length = 0;
    if (path == NULL || wide_path == NULL) return FADAL_HOME_INVALID_NAME;
    if (path[0] == '\0') {
        if (!allow_empty) return FADAL_HOME_INVALID_NAME;
        wide_path[0] = 0;
        return FADAL_HOME_OK;
    }
    for (;;) {
        UINT8 value = (UINT8)path[input];
        if (value == '\0' || value == '/') {
            if (component_length == 0 || component_length > 47 ||
                (component_length == 1 && path[component_start] == '.') ||
                (component_length == 2 && path[component_start] == '.' &&
                 path[component_start + 1] == '.')) {
                return FADAL_HOME_INVALID_NAME;
            }
            if (value == '\0') break;
            if (output >= 127) return FADAL_HOME_INVALID_NAME;
            wide_path[output++] = L'\\';
            input++;
            component_start = input;
            component_length = 0;
            continue;
        }
        if (output >= 127 || component_length >= 47 ||
            !((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9') || value == '_' || value == '-' ||
              value == '.')) {
            return FADAL_HOME_INVALID_NAME;
        }
        wide_path[output++] = (CHAR16)value;
        component_length++;
        input++;
    }
    wide_path[output] = 0;
    return FADAL_HOME_OK;
}

static fadal_net_u32 fadal_home_status(EFI_STATUS status) {
    if (status == EFI_SUCCESS) return FADAL_HOME_OK;
    if (status == EFI_NOT_FOUND) return FADAL_HOME_NOT_FOUND;
    if (status == EFI_BUFFER_TOO_SMALL) return FADAL_HOME_TOO_LARGE;
    return FADAL_HOME_FAILED;
}

static fadal_net_u32 fadal_home_init(void) {
    static CHAR16 directory_name[] = L"\\FADALHOME";
    EFI_STATUS status;
    if (g_fadal_root == NULL) return FADAL_HOME_UNAVAILABLE;
    status = uefi_call_wrapper(g_fadal_root->Open, 5, g_fadal_root,
                               &g_fadal_home, directory_name,
                               EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE |
                                   EFI_FILE_MODE_CREATE,
                               EFI_FILE_DIRECTORY);
    if (EFI_ERROR(status) || g_fadal_home == NULL) {
        g_fadal_home = NULL;
        return FADAL_HOME_UNAVAILABLE;
    }
    status = uefi_call_wrapper(g_fadal_home->Flush, 1, g_fadal_home);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(g_fadal_home->Close, 1, g_fadal_home);
        g_fadal_home = NULL;
        return FADAL_HOME_UNAVAILABLE;
    }
    return FADAL_HOME_OK;
}

static fadal_net_u32 fadal_home_entry_is_directory(EFI_FILE_HANDLE file);

static fadal_net_u32 fadal_home_list(const char *path, char *buffer,
                                     fadal_net_u32 capacity,
                                     fadal_net_u32 *written) {
    UINT8 info_storage[512] __attribute__((aligned(8)));
    UINTN output_size = 0;
    EFI_FILE_HANDLE directory;
    CHAR16 wide_path[128];
    EFI_STATUS status;
    fadal_net_u32 result;
    int owns_directory = 0;
    if (written != NULL) *written = 0;
    if (g_fadal_home == NULL) return FADAL_HOME_UNAVAILABLE;
    if (buffer == NULL || written == NULL || capacity == 0) return FADAL_HOME_TOO_LARGE;
    buffer[0] = '\0';
    result = fadal_home_path(path, wide_path, 1);
    if (result != FADAL_HOME_OK) return result;
    directory = g_fadal_home;
    if (wide_path[0] != 0) {
        status = uefi_call_wrapper(g_fadal_home->Open, 5, g_fadal_home,
                                   &directory, wide_path, EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(status)) return fadal_home_status(status);
        owns_directory = 1;
        result = fadal_home_entry_is_directory(directory);
        if (result != FADAL_HOME_IS_DIRECTORY) {
            if (result == FADAL_HOME_OK) result = FADAL_HOME_NOT_DIRECTORY;
            uefi_call_wrapper(directory->Close, 1, directory);
            return result;
        }
    }
    status = uefi_call_wrapper(directory->SetPosition, 2, directory, 0);
    if (EFI_ERROR(status)) {
        result = fadal_home_status(status);
        goto cleanup;
    }
    for (;;) {
        UINTN info_size = sizeof(info_storage);
        EFI_FILE_INFO *info = (EFI_FILE_INFO *)info_storage;
        UINTN index;
        UINTN name_length = 0;
        UINTN suffix_size;
        int is_directory;
        char ascii_name[48];
        status = uefi_call_wrapper(directory->Read, 3, directory,
                                   &info_size, info_storage);
        if (EFI_ERROR(status)) {
            result = fadal_home_status(status);
            goto cleanup;
        }
        if (info_size == 0) break;
        if (info_size < SIZE_OF_EFI_FILE_INFO + sizeof(CHAR16)) {
            result = FADAL_HOME_FAILED;
            goto cleanup;
        }
        is_directory = (info->Attribute & EFI_FILE_DIRECTORY) != 0;
        while (name_length < 47 && info->FileName[name_length] != 0) {
            if (info->FileName[name_length] > 0x7f) break;
            ascii_name[name_length] = (char)info->FileName[name_length];
            name_length++;
        }
        if (name_length == 0 || info->FileName[name_length] != 0) continue;
        ascii_name[name_length] = '\0';
        if ((name_length == 1 && ascii_name[0] == '.') ||
            (name_length == 2 && ascii_name[0] == '.' && ascii_name[1] == '.')) continue;
        for (index = 0; index < name_length; index++) {
            char value = ascii_name[index];
            if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                  (value >= '0' && value <= '9') || value == '_' || value == '-' || value == '.')) {
                break;
            }
        }
        if (index != name_length) continue;
        suffix_size = is_directory ? 2 : 1;
        if (output_size + name_length + suffix_size >= capacity) {
            result = FADAL_HOME_TOO_LARGE;
            goto cleanup;
        }
        for (index = 0; index < name_length; index++) buffer[output_size++] = ascii_name[index];
        if (is_directory) buffer[output_size++] = '/';
        buffer[output_size++] = '\n';
    }
    buffer[output_size] = '\0';
    *written = (fadal_net_u32)output_size;
    result = FADAL_HOME_OK;
cleanup:
    if (owns_directory) uefi_call_wrapper(directory->Close, 1, directory);
    if (result != FADAL_HOME_OK) {
        buffer[0] = '\0';
        *written = 0;
    }
    return result;
}

static fadal_net_u32 fadal_home_read(const char *name, char *buffer,
                                     fadal_net_u32 capacity,
                                     fadal_net_u32 *read_size) {
    CHAR16 wide_name[128];
    EFI_FILE_HANDLE file = NULL;
    UINTN size = capacity;
    EFI_STATUS status;
    fadal_net_u32 result = fadal_home_path(name, wide_name, 0);
    if (read_size != NULL) *read_size = 0;
    if (result != FADAL_HOME_OK) return result;
    if (g_fadal_home == NULL) return FADAL_HOME_UNAVAILABLE;
    if (buffer == NULL || read_size == NULL || capacity == 0) return FADAL_HOME_TOO_LARGE;
    status = uefi_call_wrapper(g_fadal_home->Open, 5, g_fadal_home, &file,
                               wide_name, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) return fadal_home_status(status);
    status = uefi_call_wrapper(file->Read, 3, file, &size, buffer);
    uefi_call_wrapper(file->Close, 1, file);
    if (EFI_ERROR(status)) return fadal_home_status(status);
    *read_size = (fadal_net_u32)size;
    return FADAL_HOME_OK;
}

static fadal_net_u32 fadal_home_entry_is_directory(EFI_FILE_HANDLE file) {
    UINT8 info_storage[512] __attribute__((aligned(8)));
    UINTN info_size = sizeof(info_storage);
    EFI_FILE_INFO *info = (EFI_FILE_INFO *)info_storage;
    EFI_STATUS status;
    status = uefi_call_wrapper(file->GetInfo, 4, file, &gFadalFileInfoGuid,
                               &info_size, info_storage);
    if (EFI_ERROR(status)) return fadal_home_status(status);
    if (info_size < SIZE_OF_EFI_FILE_INFO) return FADAL_HOME_FAILED;
    if ((info->Attribute & EFI_FILE_DIRECTORY) != 0) return FADAL_HOME_IS_DIRECTORY;
    return FADAL_HOME_OK;
}

static fadal_net_u32 fadal_home_write(const char *name, const char *buffer,
                                      fadal_net_u32 size) {
    CHAR16 wide_name[128];
    EFI_FILE_HANDLE file = NULL;
    EFI_STATUS status;
    UINTN write_size;
    fadal_net_u32 result = fadal_home_path(name, wide_name, 0);
    if (result != FADAL_HOME_OK) return result;
    if (g_fadal_home == NULL) return FADAL_HOME_UNAVAILABLE;
    if ((buffer == NULL && size != 0) || size > 4096) return FADAL_HOME_TOO_LARGE;
    status = uefi_call_wrapper(g_fadal_home->Open, 5, g_fadal_home, &file,
                               wide_name, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (status == EFI_SUCCESS) {
        result = fadal_home_entry_is_directory(file);
        if (result != FADAL_HOME_OK) {
            uefi_call_wrapper(file->Close, 1, file);
            return result;
        }
        status = uefi_call_wrapper(file->Delete, 1, file);
        file = NULL;
        if (status != EFI_SUCCESS) return fadal_home_status(status);
    } else if (status != EFI_NOT_FOUND) {
        return fadal_home_status(status);
    }
    status = uefi_call_wrapper(g_fadal_home->Open, 5, g_fadal_home, &file,
                               wide_name, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE |
                                   EFI_FILE_MODE_CREATE,
                               0);
    if (EFI_ERROR(status)) return fadal_home_status(status);
    write_size = size;
    if (size != 0) {
        status = uefi_call_wrapper(file->Write, 3, file, &write_size, (VOID *)buffer);
        if (!EFI_ERROR(status) && write_size != size) status = EFI_DEVICE_ERROR;
    }
    if (!EFI_ERROR(status)) status = uefi_call_wrapper(file->Flush, 1, file);
    uefi_call_wrapper(file->Close, 1, file);
    return fadal_home_status(status);
}

static fadal_net_u32 fadal_home_delete(const char *name) {
    CHAR16 wide_name[128];
    EFI_FILE_HANDLE file = NULL;
    EFI_STATUS status;
    fadal_net_u32 result = fadal_home_path(name, wide_name, 0);
    if (result != FADAL_HOME_OK) return result;
    if (g_fadal_home == NULL) return FADAL_HOME_UNAVAILABLE;
    status = uefi_call_wrapper(g_fadal_home->Open, 5, g_fadal_home, &file,
                               wide_name, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (EFI_ERROR(status)) return fadal_home_status(status);
    result = fadal_home_entry_is_directory(file);
    if (result != FADAL_HOME_OK) {
        uefi_call_wrapper(file->Close, 1, file);
        return result;
    }
    status = uefi_call_wrapper(file->Delete, 1, file);
    file = NULL;
    if (status != EFI_SUCCESS) return fadal_home_status(status);
    status = uefi_call_wrapper(g_fadal_home->Flush, 1, g_fadal_home);
    return fadal_home_status(status);
}

static fadal_net_u32 fadal_home_is_directory(const char *path) {
    CHAR16 wide_path[128];
    EFI_FILE_HANDLE directory = NULL;
    EFI_STATUS status;
    fadal_net_u32 result = fadal_home_path(path, wide_path, 1);
    if (result != FADAL_HOME_OK) return result;
    if (g_fadal_home == NULL) return FADAL_HOME_UNAVAILABLE;
    if (wide_path[0] == 0) return FADAL_HOME_OK;
    status = uefi_call_wrapper(g_fadal_home->Open, 5, g_fadal_home,
                               &directory, wide_path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) return fadal_home_status(status);
    result = fadal_home_entry_is_directory(directory);
    uefi_call_wrapper(directory->Close, 1, directory);
    if (result == FADAL_HOME_IS_DIRECTORY) return FADAL_HOME_OK;
    if (result == FADAL_HOME_OK) return FADAL_HOME_NOT_DIRECTORY;
    return result;
}

static fadal_net_u32 fadal_home_mkdir(const char *path) {
    CHAR16 wide_path[128];
    EFI_FILE_HANDLE directory = NULL;
    EFI_STATUS status;
    fadal_net_u32 result = fadal_home_path(path, wide_path, 0);
    if (result != FADAL_HOME_OK) return result;
    if (g_fadal_home == NULL) return FADAL_HOME_UNAVAILABLE;
    status = uefi_call_wrapper(g_fadal_home->Open, 5, g_fadal_home,
                               &directory, wide_path,
                               EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (status == EFI_SUCCESS) {
        uefi_call_wrapper(directory->Close, 1, directory);
        return FADAL_HOME_ALREADY_EXISTS;
    }
    if (status != EFI_NOT_FOUND) return fadal_home_status(status);
    status = uefi_call_wrapper(g_fadal_home->Open, 5, g_fadal_home,
                               &directory, wide_path,
                               EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE |
                                   EFI_FILE_MODE_CREATE,
                               EFI_FILE_DIRECTORY);
    if (EFI_ERROR(status)) return fadal_home_status(status);
    result = fadal_home_entry_is_directory(directory);
    if (result != FADAL_HOME_IS_DIRECTORY) {
        uefi_call_wrapper(directory->Close, 1, directory);
        return result == FADAL_HOME_OK ? FADAL_HOME_FAILED : result;
    }
    status = uefi_call_wrapper(directory->Flush, 1, directory);
    uefi_call_wrapper(directory->Close, 1, directory);
    if (!EFI_ERROR(status)) status = uefi_call_wrapper(g_fadal_home->Flush, 1, g_fadal_home);
    return fadal_home_status(status);
}

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

static BOOLEAN valid_memory_layout(const FADAL_MEMORY_LAYOUT *layout,
                                   UINTN file_size, UINTN memory_size,
                                   UINT64 entry_offset) {
    const UINT32 page = FADAL_PAGE_SIZE;
    if (layout->magic != FADAL_LAYOUT_MAGIC ||
        layout->version != FADAL_LAYOUT_VERSION ||
        layout->memory_end != memory_size ||
        layout->text_start < page || layout->text_start % page != 0 ||
        layout->text_end % page != 0 || layout->rodata_start % page != 0 ||
        layout->rodata_end % page != 0 || layout->data_start % page != 0 ||
        layout->data_end % page != 0 || layout->bss_start % page != 0 ||
        layout->memory_end % page != 0 ||
        layout->text_start >= layout->text_end ||
        layout->text_end != layout->rodata_start ||
        layout->rodata_start >= layout->rodata_end ||
        layout->rodata_end != layout->data_start ||
        layout->data_start >= layout->data_end ||
        layout->data_end != layout->bss_start ||
        layout->bss_start >= layout->memory_end ||
        entry_offset < layout->text_start || entry_offset >= layout->text_end ||
        layout->data_end != file_size) {
        return FALSE;
    }
    return TRUE;
}

static VOID payload_page_permissions(const FADAL_MEMORY_LAYOUT *layout,
                                     UINT64 offset,
                                     UINT64 *set_flags,
                                     UINT64 *clear_flags) {
    if (offset < layout->text_start) {
        *set_flags = FADAL_X86_PTE_NX;
        *clear_flags = FADAL_X86_PTE_WRITE;
    } else if (offset < layout->text_end) {
        *set_flags = 0;
        *clear_flags = FADAL_X86_PTE_WRITE | FADAL_X86_PTE_NX;
    } else if (offset < layout->data_start) {
        *set_flags = FADAL_X86_PTE_NX;
        *clear_flags = FADAL_X86_PTE_WRITE;
    } else {
        *set_flags = FADAL_X86_PTE_WRITE | FADAL_X86_PTE_NX;
        *clear_flags = 0;
    }
}

static EFI_STATUS read_payload(EFI_FILE_HANDLE root, VOID **payload,
                               UINTN *file_size_out, UINTN *memory_size_out,
                               UINT64 *entry_offset,
                               FADAL_MEMORY_LAYOUT *layout_out,
                               BOOLEAN *has_layout_out) {
    EFI_FILE_HANDLE file = NULL;
    EFI_FILE_INFO *info = NULL;
    UINTN info_size = 0;
    UINTN read_size;
    UINTN file_size;
    UINTN memory_size;
    UINTN allocation_pages;
    UINTN allocation_bytes;
    EFI_PHYSICAL_ADDRESS allocation_base = 0;
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

    file_size = (UINTN)info->FileSize;
    read_size = sizeof(header);
    status = uefi_call_wrapper(file->Read, 3, file, &read_size, &header);
    if (EFI_ERROR(status) || read_size != sizeof(header)) {
        uefi_call_wrapper(BS->FreePool, 1, info);
        uefi_call_wrapper(file->Close, 1, file);
        return EFI_LOAD_ERROR;
    }
    if (header.magic != FADAL_UEFI_MAGIC ||
        (header.version != FADAL_UEFI_VERSION_LEGACY &&
         header.version != FADAL_UEFI_VERSION_CURRENT) ||
        header.header_size != sizeof(header) ||
        header.memory_size < file_size ||
        header.memory_size > FADAL_UEFI_MAX_IMAGE ||
        header.entry_offset < header.header_size ||
        header.entry_offset >= file_size) {
        uefi_call_wrapper(BS->FreePool, 1, info);
        uefi_call_wrapper(file->Close, 1, file);
        return EFI_UNSUPPORTED;
    }
    memory_size = (UINTN)header.memory_size;
    status = uefi_call_wrapper(file->SetPosition, 2, file, 0);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(BS->FreePool, 1, info);
        uefi_call_wrapper(file->Close, 1, file);
        return EFI_LOAD_ERROR;
    }
    allocation_pages = (memory_size + FADAL_PAGE_SIZE - 1U) / FADAL_PAGE_SIZE;
    allocation_bytes = allocation_pages * FADAL_PAGE_SIZE;
    status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
                               EfiLoaderData, allocation_pages, &allocation_base);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(BS->FreePool, 1, info);
        uefi_call_wrapper(file->Close, 1, file);
        return status;
    }
    *payload = (VOID *)(UINTN)allocation_base;
    SetMem(*payload, allocation_bytes, 0);
    read_size = file_size;
    status = uefi_call_wrapper(file->Read, 3, file, &read_size, *payload);
    uefi_call_wrapper(file->Close, 1, file);
    uefi_call_wrapper(BS->FreePool, 1, info);
    if (EFI_ERROR(status) || read_size != file_size) {
        uefi_call_wrapper(BS->FreePages, 2, allocation_base, allocation_pages);
        *payload = NULL;
        return EFI_LOAD_ERROR;
    }

    *has_layout_out = FALSE;
    SetMem(layout_out, sizeof(*layout_out), 0);
    if (header.version == FADAL_UEFI_VERSION_CURRENT) {
        if (file_size < sizeof(header) + sizeof(*layout_out)) {
            uefi_call_wrapper(BS->FreePages, 2, allocation_base, allocation_pages);
            *payload = NULL;
            return EFI_UNSUPPORTED;
        }
        CopyMem(layout_out, (UINT8 *)*payload + sizeof(header), sizeof(*layout_out));
        if (!valid_memory_layout(layout_out, file_size, memory_size, header.entry_offset)) {
            uefi_call_wrapper(BS->FreePages, 2, allocation_base, allocation_pages);
            *payload = NULL;
            return EFI_UNSUPPORTED;
        }
        *has_layout_out = TRUE;
    }

    *file_size_out = file_size;
    *memory_size_out = memory_size;
    *entry_offset = header.entry_offset;
    return EFI_SUCCESS;
}

static EFI_STATUS protect_payload_pages(VOID *payload, UINTN memory_size,
                                        const FADAL_MEMORY_LAYOUT *layout) {
    UINTN page_count;
    UINTN page_index;
    UINTN changed_pages = 0;
    UINTN map_bytes;
    UINT64 base = (UINT64)(UINTN)payload;
    FADAL_X86_PAGE_MAPPING *mappings = NULL;
    FADAL_X86_PAGE_TABLE_CONTEXT context;
    EFI_STATUS status;

    if ((memory_size % FADAL_PAGE_SIZE) != 0 ||
        layout->memory_end != memory_size) {
        return EFI_UNSUPPORTED;
    }
    status = fadal_x86_enable_nx();
    if (EFI_ERROR(status)) return status;

    page_count = memory_size / FADAL_PAGE_SIZE;
    map_bytes = page_count * sizeof(*mappings);
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
                               map_bytes, (VOID **)&mappings);
    if (EFI_ERROR(status)) return status;
    SetMem(mappings, map_bytes, 0);
    SetMem(&context, sizeof(context), 0);

    /* Resolve every leaf first. If a mapping is unsupported, restore any
       large-page splits before changing a single payload permission. */
    for (page_index = 0; page_index < page_count; page_index++) {
        UINT64 page_address = base + (UINT64)page_index * FADAL_PAGE_SIZE;
        UINT64 set_flags;
        UINT64 clear_flags;
        status = fadal_x86_walk_page(BS, &context, page_address,
                                     &mappings[page_index]);
        if (!EFI_ERROR(status)) {
            payload_page_permissions(layout,
                                     (UINT64)page_index * FADAL_PAGE_SIZE,
                                     &set_flags, &clear_flags);
            status = fadal_x86_check_page_permissions(&mappings[page_index],
                                                     set_flags, clear_flags);
        }
        if (EFI_ERROR(status)) {
            fadal_x86_restore_splits(BS, &context);
            uefi_call_wrapper(BS->FreePool, 1, mappings);
            return status;
        }
    }

    fadal_x86_enable_write_protect();
    for (page_index = 0; page_index < page_count; page_index++) {
        UINT64 offset = (UINT64)page_index * FADAL_PAGE_SIZE;
        UINT64 set_flags;
        UINT64 clear_flags;
        payload_page_permissions(layout, offset, &set_flags, &clear_flags);
        status = fadal_x86_set_page_permissions(&mappings[page_index],
                                                set_flags, clear_flags);
        if (EFI_ERROR(status)) {
            fadal_x86_restore_leaf_permissions(mappings, changed_pages);
            fadal_x86_restore_splits(BS, &context);
            uefi_call_wrapper(BS->FreePool, 1, mappings);
            return status;
        }
        changed_pages++;
    }

    Print(L"W^X page policy installed: %lu pages, %lu table splits\r\n",
          page_count, context.SplitCount);
    uefi_call_wrapper(BS->FreePool, 1, mappings);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system_table) {
    EFI_LOADED_IMAGE *loaded_image = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *filesystem = NULL;
    EFI_FILE_HANDLE root = NULL;
    EFI_STATUS status;
    VOID *payload = NULL;
    UINTN payload_size = 0;
    UINTN payload_memory_size = 0;
    UINT64 entry_offset = 0;
    FADAL_MEMORY_LAYOUT memory_layout;
    BOOLEAN has_memory_layout = FALSE;
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
    g_fadal_home = NULL;
    if (fadal_home_init() == FADAL_HOME_OK) {
        Print(L"persistent home ready: \\FADALHOME\r\n");
    } else {
        Print(L"persistent home unavailable; file commands disabled\r\n");
    }
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

    status = read_payload(root, &payload, &payload_size, &payload_memory_size,
                          &entry_offset, &memory_layout, &has_memory_layout);
    if (EFI_ERROR(status)) {
        Print(L"error: FADAL.KRN is not a supported Fadal64 payload: %r\r\n", status);
        return status;
    }

    Print(L"payload loaded (%lu file / %lu memory bytes)\r\n",
          payload_size, payload_memory_size);
    if (has_memory_layout) {
        status = protect_payload_pages(payload, payload_memory_size,
                                       &memory_layout);
        if (EFI_ERROR(status)) {
            UINTN pages = (payload_memory_size + FADAL_PAGE_SIZE - 1U) /
                          FADAL_PAGE_SIZE;
            Print(L"error: refusing unprotected FDL64 v3 payload: %r\r\n", status);
            uefi_call_wrapper(BS->FreePages, 2,
                              (EFI_PHYSICAL_ADDRESS)(UINTN)payload, pages);
            return status;
        }
    } else {
        Print(L"warning: legacy FDL64 v2 has no W^X segment manifest\r\n");
    }
    services.http_download = fadal_http_download;
    services.http_available = g_fadal_http != NULL;
    services.home_list = fadal_home_list;
    services.home_read = fadal_home_read;
    services.home_write = fadal_home_write;
    services.home_available = g_fadal_home != NULL;
    services.home_delete = fadal_home_delete;
    services.home_is_directory = fadal_home_is_directory;
    services.home_mkdir = fadal_home_mkdir;
    entry = (FADAL_UEFI_ENTRY)((UINT8 *)payload + entry_offset);
    entry(&framebuffer, &services);
    return EFI_SUCCESS;
}
