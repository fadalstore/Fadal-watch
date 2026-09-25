#ifndef FADAL_AUTH_H
#define FADAL_AUTH_H

typedef unsigned char auth_u8;
typedef unsigned int auth_u32;

#define AUTH_NONE 0
#define AUTH_USER 1
#define AUTH_ROOT 2

auth_u8 auth_init(void);
auth_u8 auth_login(const auth_u8 *username, auth_u32 username_length,
                  const auth_u8 *password, auth_u32 password_length);
auth_u8 auth_change_password(auth_u8 role, const auth_u8 *old_password,
                             auth_u32 old_length, const auth_u8 *new_password,
                             auth_u32 new_length);
auth_u8 auth_role(void);

#endif
