#include "auth.h"
#include "gitpack.h"

#define AUTH_PASSWORD_MAX 31
static const auth_u8 root_salt[] = "FADAL-ROOT-2026";
static const auth_u8 user_salt[] = "FADAL-USER-2026";
static auth_u8 root_hash[20];
static auth_u8 user_hash[20];
static auth_u8 current_role;
static auth_u8 initialized;

static auth_u8 auth_equal(const auth_u8 *left, const auth_u8 *right, auth_u32 length) {
    auth_u8 difference = 0;
    for (auth_u32 index = 0; index < length; index++) difference |= left[index] ^ right[index];
    return difference == 0;
}
static auth_u8 auth_hash(const auth_u8 *salt, const auth_u8 *password,
                         auth_u32 length, auth_u8 output[20]) {
    if (!password || length == 0 || length > AUTH_PASSWORD_MAX) return 0;
    git_sha1_context context;
    git_sha1_init(&context);
    git_sha1_update(&context, salt, 15);
    git_sha1_update(&context, password, length);
    git_sha1_final(&context, output);
    return 1;
}
static void auth_default_hash(const auth_u8 *salt, auth_u8 output[20]) {
    static const auth_u8 default_password[] = "fadal";
    git_sha1_context context;
    git_sha1_init(&context);
    git_sha1_update(&context, salt, 15);
    git_sha1_update(&context, default_password, sizeof(default_password) - 1);
    git_sha1_final(&context, output);
}
auth_u8 auth_init(void) {
    auth_default_hash(root_salt, root_hash);
    auth_default_hash(user_salt, user_hash);
    current_role = AUTH_NONE;
    initialized = 1;
    return 1;
}
auth_u8 auth_login(const auth_u8 *username, auth_u32 username_length,
                  const auth_u8 *password, auth_u32 password_length) {
    auth_u8 digest[20];
    if (!initialized || !username || !password || username_length == 0 || username_length > 16 ||
        !auth_hash(username_length == 4 && username[0] == 'r' ? root_salt : user_salt,
                   password, password_length, digest)) return AUTH_NONE;
    if (username_length == 4 && username[0] == 'r' && username[1] == 'o' &&
        username[2] == 'o' && username[3] == 't' && auth_equal(digest, root_hash, 20)) {
        current_role = AUTH_ROOT;
        return current_role;
    }
    if (username_length == 4 && username[0] == 'u' && username[1] == 's' &&
        username[2] == 'e' && username[3] == 'r' && auth_equal(digest, user_hash, 20)) {
        current_role = AUTH_USER;
        return current_role;
    }
    current_role = AUTH_NONE;
    return AUTH_NONE;
}
auth_u8 auth_change_password(auth_u8 role, const auth_u8 *old_password,
                             auth_u32 old_length, const auth_u8 *new_password,
                             auth_u32 new_length) {
    auth_u8 digest[20];
    auth_u8 *stored_hash;
    const auth_u8 *salt;
    if (!initialized || role != current_role || (role != AUTH_ROOT && role != AUTH_USER) ||
        !auth_hash(role == AUTH_ROOT ? root_salt : user_salt, old_password, old_length, digest) ||
        new_length == 0 || new_length > AUTH_PASSWORD_MAX || !auth_hash(role == AUTH_ROOT ? root_salt : user_salt,
        new_password, new_length, digest)) return 0;
    stored_hash = role == AUTH_ROOT ? root_hash : user_hash;
    salt = role == AUTH_ROOT ? root_salt : user_salt;
    if (!auth_hash(salt, old_password, old_length, digest) || !auth_equal(digest, stored_hash, 20)) return 0;
    auth_hash(salt, new_password, new_length, stored_hash);
    return 1;
}
auth_u8 auth_role(void) { return current_role; }
