#ifndef FADAL_IDENTITY_H
#define FADAL_IDENTITY_H

typedef unsigned int fadal_u32;
typedef unsigned char fadal_u8;

#define FADAL_UID_ROOT 0u
#define FADAL_UID_USER 1000u
#define FADAL_GID_ROOT 0u
#define FADAL_GID_USER 1000u

#define FADAL_CAP_TTY_READ          (1u << 0)
#define FADAL_CAP_TTY_WRITE         (1u << 1)
#define FADAL_CAP_FS_READ           (1u << 2)
#define FADAL_CAP_FS_WRITE          (1u << 3)
#define FADAL_CAP_FS_ADMIN          (1u << 4)
#define FADAL_CAP_PROC_READ         (1u << 5)
#define FADAL_CAP_PROC_ADMIN        (1u << 6)
#define FADAL_CAP_NET_OBSERVE       (1u << 7)
#define FADAL_CAP_NET_ADMIN         (1u << 8)
#define FADAL_CAP_DEVICE_ADMIN      (1u << 9)
#define FADAL_CAP_CREDENTIAL_ADMIN  (1u << 10)
#define FADAL_CAP_BOOT_RECOVERY     (1u << 11)
#define FADAL_CAP_ALL               ((1u << 12) - 1u)

struct fadal_identity {
    fadal_u32 uid;
    fadal_u32 gid;
    fadal_u32 saved_uid;
    fadal_u32 saved_gid;
    fadal_u32 permitted;
    fadal_u32 effective;
};

static inline void fadal_identity_user(struct fadal_identity *identity) {
    if (identity == (struct fadal_identity *)0) {
        return;
    }
    identity->uid = FADAL_UID_USER;
    identity->gid = FADAL_GID_USER;
    identity->saved_uid = FADAL_UID_USER;
    identity->saved_gid = FADAL_GID_USER;
    identity->permitted = FADAL_CAP_TTY_READ |
        FADAL_CAP_TTY_WRITE |
        FADAL_CAP_FS_READ |
        FADAL_CAP_NET_OBSERVE;
    identity->effective = identity->permitted;
}

static inline void fadal_identity_root(struct fadal_identity *identity) {
    if (identity == (struct fadal_identity *)0) {
        return;
    }
    identity->uid = FADAL_UID_ROOT;
    identity->gid = FADAL_GID_ROOT;
    identity->saved_uid = FADAL_UID_ROOT;
    identity->saved_gid = FADAL_GID_ROOT;
    identity->permitted = FADAL_CAP_ALL;
    identity->effective = FADAL_CAP_ALL;
}

static inline fadal_u8 fadal_identity_has(
    const struct fadal_identity *identity, fadal_u32 capability) {
    return identity != (const struct fadal_identity *)0 &&
        capability != 0 &&
        (identity->effective & capability) == capability;
}

static inline fadal_u8 fadal_identity_is_root(
    const struct fadal_identity *identity) {
    return identity != (const struct fadal_identity *)0 &&
        identity->uid == FADAL_UID_ROOT &&
        identity->gid == FADAL_GID_ROOT &&
        identity->effective == FADAL_CAP_ALL;
}

#endif
