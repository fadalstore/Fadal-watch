#ifndef FADAL_UEFI_NET_H
#define FADAL_UEFI_NET_H

typedef unsigned int fadal_net_u32;

typedef fadal_net_u32 (*fadal_http_download_fn)(const char *url, const char *name);

typedef struct {
    fadal_http_download_fn http_download;
    fadal_net_u32 http_available;
} fadal_uefi_services;

#define FADAL_HTTP_OK 0
#define FADAL_HTTP_UNAVAILABLE 1
#define FADAL_HTTP_BAD_REQUEST 2
#define FADAL_HTTP_FAILED 3
#define FADAL_HTTP_TOO_LARGE 4

#endif
