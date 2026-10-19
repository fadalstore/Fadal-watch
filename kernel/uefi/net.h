#ifndef FADAL_UEFI_NET_H
#define FADAL_UEFI_NET_H

typedef unsigned int fadal_net_u32;

typedef fadal_net_u32 (*fadal_http_download_fn)(const char *url, const char *name);
typedef fadal_net_u32 (*fadal_home_list_fn)(char *buffer, fadal_net_u32 capacity,
                                             fadal_net_u32 *written);
typedef fadal_net_u32 (*fadal_home_read_fn)(const char *name, char *buffer,
                                             fadal_net_u32 capacity,
                                             fadal_net_u32 *read_size);
typedef fadal_net_u32 (*fadal_home_write_fn)(const char *name, const char *buffer,
                                              fadal_net_u32 size);

typedef struct {
    fadal_http_download_fn http_download;
    fadal_net_u32 http_available;
    fadal_home_list_fn home_list;
    fadal_home_read_fn home_read;
    fadal_home_write_fn home_write;
    fadal_net_u32 home_available;
} fadal_uefi_services;

#define FADAL_HTTP_OK 0
#define FADAL_HTTP_UNAVAILABLE 1
#define FADAL_HTTP_BAD_REQUEST 2
#define FADAL_HTTP_FAILED 3
#define FADAL_HTTP_TOO_LARGE 4

#define FADAL_HOME_OK 0
#define FADAL_HOME_UNAVAILABLE 1
#define FADAL_HOME_INVALID_NAME 2
#define FADAL_HOME_NOT_FOUND 3
#define FADAL_HOME_FAILED 4
#define FADAL_HOME_TOO_LARGE 5

#endif
