#include "gitpack.h"

static git_u32 rol(git_u32 value, git_u32 count) {
    return (value << count) | (value >> (32 - count));
}
static git_u32 be32(const git_u8 *data) {
    return ((git_u32)data[0] << 24) | ((git_u32)data[1] << 16) |
        ((git_u32)data[2] << 8) | data[3];
}
static void put_be32(git_u8 *data, git_u32 value) {
    data[0] = (git_u8)(value >> 24); data[1] = (git_u8)(value >> 16);
    data[2] = (git_u8)(value >> 8); data[3] = (git_u8)value;
}
static void sha1_block(git_sha1_context *context, const git_u8 *block) {
    git_u32 words[80];
    for (git_u32 index = 0; index < 16; index++) words[index] = be32(block + index * 4);
    for (git_u32 index = 16; index < 80; index++)
        words[index] = rol(words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16], 1);
    git_u32 a = context->hash[0], b = context->hash[1], c = context->hash[2];
    git_u32 d = context->hash[3], e = context->hash[4];
    for (git_u32 index = 0; index < 80; index++) {
        git_u32 function, constant;
        if (index < 20) { function = (b & c) | ((~b) & d); constant = 0x5a827999; }
        else if (index < 40) { function = b ^ c ^ d; constant = 0x6ed9eba1; }
        else if (index < 60) { function = (b & c) | (b & d) | (c & d); constant = 0x8f1bbcdc; }
        else { function = b ^ c ^ d; constant = 0xca62c1d6; }
        git_u32 temporary = rol(a, 5) + function + e + constant + words[index];
        e = d; d = c; c = rol(b, 30); b = a; a = temporary;
    }
    context->hash[0] += a; context->hash[1] += b; context->hash[2] += c;
    context->hash[3] += d; context->hash[4] += e;
}
void git_sha1_init(git_sha1_context *context) {
    context->hash[0] = 0x67452301; context->hash[1] = 0xefcdab89;
    context->hash[2] = 0x98badcfe; context->hash[3] = 0x10325476;
    context->hash[4] = 0xc3d2e1f0;
    context->length_high = 0; context->length_low = 0; context->block_length = 0;
}
void git_sha1_update(git_sha1_context *context, const git_u8 *data, git_u32 length) {
    for (git_u32 index = 0; index < length; index++) {
        context->block[context->block_length++] = data[index];
        if (context->block_length == 64) {
            sha1_block(context, context->block);
            context->block_length = 0;
        }
        context->length_low += 8;
        if (context->length_low < 8) context->length_high++;
    }
}
void git_sha1_final(git_sha1_context *context, git_u8 digest[20]) {
    context->block[context->block_length++] = 0x80;
    if (context->block_length > 56) {
        while (context->block_length < 64) context->block[context->block_length++] = 0;
        sha1_block(context, context->block); context->block_length = 0;
    }
    while (context->block_length < 56) context->block[context->block_length++] = 0;
    put_be32(context->block + 56, context->length_high);
    put_be32(context->block + 60, context->length_low);
    sha1_block(context, context->block);
    for (git_u32 index = 0; index < 5; index++) put_be32(digest + index * 4, context->hash[index]);
}
static git_u32 decimal_length(git_u32 value, git_u8 *output) {
    git_u8 reverse[12]; git_u32 length = 0;
    do { reverse[length++] = (git_u8)('0' + value % 10); value /= 10; } while (value != 0);
    for (git_u32 index = 0; index < length; index++) output[index] = reverse[length - index - 1];
    return length;
}
git_u8 git_object_sha1(const git_u8 *type, const git_u8 *data, git_u32 length, git_u8 digest[20]) {
    if (!type || !data || !digest) return 0;
    git_sha1_context context; git_u8 size[12];
    git_u32 type_length = 0;
    while (type[type_length] && type_length < 16) type_length++;
    if (type_length == 0 || type_length == 16) return 0;
    git_u32 size_length = decimal_length(length, size);
    git_sha1_init(&context); git_sha1_update(&context, type, type_length);
    git_sha1_update(&context, (const git_u8 *)" ", 1);
    git_sha1_update(&context, size, size_length);
    git_sha1_update(&context, (const git_u8 *)"\0", 1);
    git_sha1_update(&context, data, length); git_sha1_final(&context, digest);
    return 1;
}
static void pack_header_byte(git_pack_stream *stream, git_u8 value) {
    if (stream->header_bytes < 12) stream->header[stream->header_bytes++] = value;
}
static void pack_flush_tail(git_pack_stream *stream, git_u8 value) {
    if (stream->checksum_tail_length < 20) {
        stream->checksum_tail[stream->checksum_tail_length++] = value;
        return;
    }
    git_sha1_update(&stream->checksum, stream->checksum_tail, 1);
    for (git_u32 index = 1; index < 20; index++) stream->checksum_tail[index - 1] = stream->checksum_tail[index];
    stream->checksum_tail[19] = value;
    stream->stream_bytes++;
}
git_u8 git_pack_begin(git_pack_stream *stream) {
    if (!stream) return 0;
    stream->active = 1; stream->header_bytes = 0; stream->checksum_tail_length = 0;
    stream->version = 0; stream->object_count = 0; stream->stream_bytes = 0;
    git_sha1_init(&stream->checksum);
    return 1;
}
git_u8 git_pack_feed(git_pack_stream *stream, const git_u8 *data, git_u32 length) {
    if (!stream || !stream->active || !data) return 0;
    for (git_u32 index = 0; index < length; index++) {
        pack_header_byte(stream, data[index]);
        pack_flush_tail(stream, data[index]);
        if (stream->header_bytes == 12 && stream->version == 0) {
            if (stream->header[0] != 'P' || stream->header[1] != 'A' || stream->header[2] != 'C' || stream->header[3] != 'K') return 0;
            stream->version = be32(stream->header + 4);
            stream->object_count = be32(stream->header + 8);
            if ((stream->version != 2 && stream->version != 3) || stream->object_count == 0) return 0;
        }
    }
    return 1;
}
git_u8 git_pack_finish(git_pack_stream *stream, git_u8 pack_checksum[20]) {
    git_u8 digest[20];
    if (!stream || !stream->active || stream->header_bytes != 12 || stream->checksum_tail_length != 20) return 0;
    git_sha1_final(&stream->checksum, digest);
    for (git_u32 index = 0; index < 20; index++) {
        if (digest[index] != stream->checksum_tail[index]) return 0;
        if (pack_checksum) pack_checksum[index] = digest[index];
    }
    stream->active = 0;
    return 1;
}
git_u8 gitpack_self_test(void) {
    static const git_u8 abc[] = "abc";
    static const git_u8 blob_expected[20] = {
        0xf2, 0xba, 0x8f, 0x84, 0xab, 0x5c, 0x1b, 0xce, 0x84, 0xa7,
        0xb4, 0x41, 0xcb, 0x19, 0x59, 0xcf, 0xc7, 0x09, 0x3b, 0x7f
    };
    static const git_u8 expected[20] = {
        0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a, 0xba, 0x3e,
        0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d
    };
    git_u8 digest[20], pack[33], verified[20];
    git_sha1_context context;
    git_sha1_init(&context); git_sha1_update(&context, abc, 3); git_sha1_final(&context, digest);
    for (git_u32 index = 0; index < 20; index++) if (digest[index] != expected[index]) return 0;
    if (!git_object_sha1((const git_u8 *)"blob", abc, 3, digest)) return 0;
    for (git_u32 index = 0; index < 20; index++) if (digest[index] != blob_expected[index]) return 0;
    pack[0] = 'P'; pack[1] = 'A'; pack[2] = 'C'; pack[3] = 'K';
    pack[4] = 0; pack[5] = 0; pack[6] = 0; pack[7] = 2;
    pack[8] = 0; pack[9] = 0; pack[10] = 0; pack[11] = 1; pack[12] = 0;
    git_sha1_init(&context); git_sha1_update(&context, pack, 13); git_sha1_final(&context, pack + 13);
    git_pack_stream stream;
    if (!git_pack_begin(&stream) || !git_pack_feed(&stream, pack, 7) || !git_pack_feed(&stream, pack + 7, 26) ||
        !git_pack_finish(&stream, verified)) return 0;
    return verified[0] == pack[13] && verified[19] == pack[32];
}
