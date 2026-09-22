#include "identity.h"

void fadal_identity_user(struct fadal_identity *identity) {
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

void fadal_identity_root(struct fadal_identity *identity) {
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

fadal_u8 fadal_identity_has(const struct fadal_identity *identity,
                            fadal_u32 capability) {
    return identity != (const struct fadal_identity *)0 &&
        capability != 0 &&
        (identity->effective & capability) == capability;
}

fadal_u8 fadal_identity_is_root(const struct fadal_identity *identity) {
    return identity != (const struct fadal_identity *)0 &&
        identity->uid == FADAL_UID_ROOT &&
        identity->gid == FADAL_GID_ROOT &&
        identity->effective == FADAL_CAP_ALL;
}
