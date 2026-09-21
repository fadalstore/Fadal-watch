#ifndef FADAL_GITPACK_H
#define FADAL_GITPACK_H

typedef unsigned char git_u8;
typedef unsigned short git_u16;
typedef unsigned int git_u32;

/* The length is represented as two 32-bit words to keep the ABI freestanding. */
typedef struct {
    git_u32 hash[5];
    git_u32 length_high;
    git_u32 length_low;
    git_u8 block[64];
    git_u32 block_length;
} git_sha1_context;

void git_sha1_init(git_sha1_context *context);
void git_sha1_update(git_sha1_context *context, const git_u8 *data, git_u32 length);
void git_sha1_final(git_sha1_context *context, git_u8 digest[20]);
git_u8 git_object_sha1(const git_u8 *type, const git_u8 *data, git_u32 length, git_u8 digest[20]);

typedef struct {
    git_u8 active;
    git_u8 header_bytes;
    git_u8 checksum_tail_length;
    git_u8 header[12];
    git_u8 checksum_tail[20];
    git_u32 version;
    git_u32 object_count;
    git_u32 stream_bytes;
    git_sha1_context checksum;
} git_pack_stream;

git_u8 git_pack_begin(git_pack_stream *stream);
git_u8 git_pack_feed(git_pack_stream *stream, const git_u8 *data, git_u32 length);
git_u8 git_pack_finish(git_pack_stream *stream, git_u8 pack_checksum[20]);
git_u8 gitpack_self_test(void);

#endif
