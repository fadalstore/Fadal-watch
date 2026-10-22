#ifndef FADAL_HTTP_H
#define FADAL_HTTP_H

typedef unsigned char http_u8;
typedef unsigned short http_u16;
typedef unsigned int http_u32;

enum {
    HTTP_RESPONSE_MORE = 1,
    HTTP_RESPONSE_DONE = 2,
    HTTP_RESPONSE_ERROR = 0
};
enum {
    HTTP_RESPONSE_PARSE_STATUS = 1,
    HTTP_RESPONSE_PARSE_HEADERS,
    HTTP_RESPONSE_PARSE_FIXED_BODY,
    HTTP_RESPONSE_PARSE_CHUNK_SIZE,
    HTTP_RESPONSE_PARSE_CHUNK_BODY,
    HTTP_RESPONSE_PARSE_CHUNK_CRLF,
    HTTP_RESPONSE_PARSE_TRAILERS,
    HTTP_RESPONSE_PARSE_CLOSE_BODY,
    HTTP_RESPONSE_PARSE_DONE,
    HTTP_RESPONSE_PARSE_ERROR
};

typedef struct {
    http_u32 header_bytes;
    http_u32 trailer_bytes;
    http_u32 content_length;
    http_u32 body_bytes;
    http_u32 chunk_remaining;
    http_u16 status_code;
    http_u16 line_length;
    http_u8 state;
    http_u8 line_pending_cr;
    http_u8 content_length_seen;
    http_u8 transfer_encoding_seen;
    http_u8 interim_count;
    http_u8 chunk_crlf_step;
    http_u8 line[256];
} http_response_parser;

typedef int (*http_body_sink)(void *context, const http_u8 *bytes, http_u32 length);

void http_response_init(http_response_parser *parser);
/* Feed arbitrary plaintext fragments; sink receives synchronously de-framed body bytes. */
int http_response_feed(http_response_parser *parser, const http_u8 *bytes,
                       http_u32 length, http_u32 *consumed,
                       http_body_sink sink, void *context);
/* EOF completes only a close-delimited response; incomplete framed bodies fail. */
int http_response_finish_eof(http_response_parser *parser);
int http_response_self_test(void);

#endif
