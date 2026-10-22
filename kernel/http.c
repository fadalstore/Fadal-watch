#include "http.h"

#define HTTP_HEADER_LIMIT 2048U
#define HTTP_TRAILER_LIMIT 2048U
#define HTTP_LINE_LIMIT 255U
#define HTTP_INTERIM_LIMIT 4U

struct http_test_capture {
    http_u8 bytes[32];
    http_u32 length;
};

static http_u8 http_ascii_lower(http_u8 value) {
    if (value >= 'A' && value <= 'Z') return (http_u8)(value + ('a' - 'A'));
    return value;
}
static int http_equal_ci(const http_u8 *left, http_u32 left_length, const char *right) {
    http_u32 index = 0;
    while (right[index] != 0) {
        if (index >= left_length || http_ascii_lower(left[index]) !=
            http_ascii_lower((http_u8)right[index])) return 0;
        index++;
    }
    return index == left_length;
}
static int http_token_char(http_u8 value) {
    if ((value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') ||
        (value >= 'a' && value <= 'z')) return 1;
    switch (value) {
    case '!': case '#': case '$': case '%': case '&': case '\'': case '*':
    case '+': case '-': case '.': case '^': case '_': case '`': case '|': case '~':
        return 1;
    default:
        return 0;
    }
}
static void http_clear_message_fields(http_response_parser *parser) {
    parser->header_bytes = 0;
    parser->trailer_bytes = 0;
    parser->content_length = 0;
    parser->body_bytes = 0;
    parser->chunk_remaining = 0;
    parser->status_code = 0;
    parser->line_length = 0;
    parser->line_pending_cr = 0;
    parser->content_length_seen = 0;
    parser->transfer_encoding_seen = 0;
    parser->chunk_crlf_step = 0;
}
static void http_fail(http_response_parser *parser) {
    parser->state = HTTP_RESPONSE_PARSE_ERROR;
}
void http_response_init(http_response_parser *parser) {
    if (!parser) return;
    http_clear_message_fields(parser);
    parser->interim_count = 0;
    parser->state = HTTP_RESPONSE_PARSE_STATUS;
    for (http_u32 index = 0; index < sizeof(parser->line); index++) parser->line[index] = 0;
}
static int http_read_line_byte(http_response_parser *parser, http_u8 value) {
    if (parser->state == HTTP_RESPONSE_PARSE_STATUS || parser->state == HTTP_RESPONSE_PARSE_HEADERS) {
        if (parser->header_bytes >= HTTP_HEADER_LIMIT) return -1;
        parser->header_bytes++;
    } else if (parser->state == HTTP_RESPONSE_PARSE_TRAILERS) {
        if (parser->trailer_bytes >= HTTP_TRAILER_LIMIT) return -1;
        parser->trailer_bytes++;
    }
    if (parser->line_pending_cr) {
        if (value != '\n') return -1;
        parser->line_pending_cr = 0;
        parser->line[parser->line_length] = 0;
        return 1;
    }
    if (value == '\r') {
        parser->line_pending_cr = 1;
        return 0;
    }
    if (value == '\n' || value == 0 || value == 0x7f ||
        (value < 0x20 && value != '\t')) return -1;
    if (parser->line_length >= HTTP_LINE_LIMIT) return -1;
    parser->line[parser->line_length++] = value;
    return 0;
}
static int http_parse_status_line(http_response_parser *parser) {
    const http_u8 *line = parser->line;
    http_u32 length = parser->line_length;
    http_u32 offset;
    if (length < 12 || line[8] != ' ' ||
        !((line[0] == 'H' && line[1] == 'T' && line[2] == 'T' && line[3] == 'P' &&
           line[4] == '/' && line[5] == '1' && line[6] == '.' &&
           (line[7] == '0' || line[7] == '1')))) return 0;
    if (line[9] < '0' || line[9] > '9' || line[10] < '0' || line[10] > '9' ||
        line[11] < '0' || line[11] > '9' || (length > 12 && line[12] != ' ')) return 0;
    parser->status_code = (http_u16)((line[9] - '0') * 100 + (line[10] - '0') * 10 + line[11] - '0');
    if (parser->status_code < 100 || parser->status_code > 599) return 0;
    for (offset = 12; offset < length; offset++) {
        if (line[offset] < 0x20 && line[offset] != '\t') return 0;
    }
    parser->state = HTTP_RESPONSE_PARSE_HEADERS;
    return 1;
}
static int http_parse_header_line(http_response_parser *parser, int trailer) {
    http_u32 colon = 0;
    http_u32 value_start, value_end;
    while (colon < parser->line_length && parser->line[colon] != ':') {
        if (!http_token_char(parser->line[colon])) return 0;
        colon++;
    }
    if (colon == 0 || colon >= parser->line_length) return 0;
    value_start = colon + 1;
    value_end = parser->line_length;
    while (value_start < value_end && (parser->line[value_start] == ' ' || parser->line[value_start] == '\t')) value_start++;
    while (value_end > value_start && (parser->line[value_end - 1] == ' ' || parser->line[value_end - 1] == '\t')) value_end--;
    if (trailer) {
        if (http_equal_ci(parser->line, colon, "content-length") ||
            http_equal_ci(parser->line, colon, "transfer-encoding")) return 0;
        return 1;
    }
    if (http_equal_ci(parser->line, colon, "content-length")) {
        http_u32 value = 0;
        if (parser->content_length_seen || value_start == value_end) return 0;
        for (http_u32 index = value_start; index < value_end; index++) {
            http_u8 digit = parser->line[index];
            if (digit < '0' || digit > '9') return 0;
            digit = (http_u8)(digit - '0');
            if (value > (0xffffffffU - digit) / 10U) return 0;
            value = value * 10U + digit;
        }
        parser->content_length = value;
        parser->content_length_seen = 1;
    } else if (http_equal_ci(parser->line, colon, "transfer-encoding")) {
        if (parser->transfer_encoding_seen ||
            !http_equal_ci(parser->line + value_start, value_end - value_start, "chunked")) return 0;
        parser->transfer_encoding_seen = 1;
    }
    return 1;
}
static int http_headers_complete(http_response_parser *parser) {
    if (parser->content_length_seen && parser->transfer_encoding_seen) return 0;
    if (parser->status_code >= 100 && parser->status_code < 200) {
        if (parser->status_code == 101 || parser->content_length_seen ||
            parser->transfer_encoding_seen || ++parser->interim_count > HTTP_INTERIM_LIMIT) return 0;
        http_clear_message_fields(parser);
        parser->state = HTTP_RESPONSE_PARSE_STATUS;
        return 1;
    }
    if (parser->status_code == 204) {
        if (parser->content_length_seen || parser->transfer_encoding_seen) return 0;
        parser->state = HTTP_RESPONSE_PARSE_DONE;
        return 1;
    }
    if (parser->status_code == 304) {
        parser->state = HTTP_RESPONSE_PARSE_DONE;
        return 1;
    }
    if (parser->transfer_encoding_seen) {
        parser->state = HTTP_RESPONSE_PARSE_CHUNK_SIZE;
        return 1;
    }
    if (parser->content_length_seen) {
        parser->state = parser->content_length == 0 ? HTTP_RESPONSE_PARSE_DONE : HTTP_RESPONSE_PARSE_FIXED_BODY;
        return 1;
    }
    parser->state = HTTP_RESPONSE_PARSE_CLOSE_BODY;
    return 1;
}
static int http_parse_chunk_size(http_response_parser *parser) {
    http_u32 value = 0;
    http_u32 index = 0;
    http_u32 digits = 0;
    while (index < parser->line_length && parser->line[index] != ';') {
        http_u8 c = parser->line[index++];
        http_u8 digit;
        if (c >= '0' && c <= '9') digit = (http_u8)(c - '0');
        else if (c >= 'a' && c <= 'f') digit = (http_u8)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = (http_u8)(c - 'A' + 10);
        else return 0;
        if (value > (0xffffffffU - digit) / 16U) return 0;
        value = value * 16U + digit;
        digits++;
    }
    if (digits == 0 || value > 0xffffffffU - parser->body_bytes) return 0;
    if (index < parser->line_length) {
        for (index++; index < parser->line_length; index++)
            if (parser->line[index] < 0x20 || parser->line[index] == 0x7f) return 0;
    }
    parser->chunk_remaining = value;
    if (value == 0) parser->state = HTTP_RESPONSE_PARSE_TRAILERS;
    else parser->state = HTTP_RESPONSE_PARSE_CHUNK_BODY;
    return 1;
}
static int http_deliver_body(http_response_parser *parser, const http_u8 *bytes,
                             http_u32 length, http_body_sink sink, void *context) {
    if (length > 0xffffffffU - parser->body_bytes) return 0;
    if (sink && !sink(context, bytes, length)) return 0;
    parser->body_bytes += length;
    return 1;
}
int http_response_feed(http_response_parser *parser, const http_u8 *bytes,
                       http_u32 length, http_u32 *consumed,
                       http_body_sink sink, void *context) {
    http_u32 offset = 0;
    if (consumed) *consumed = 0;
    if (!parser || !consumed || (!bytes && length != 0) ||
        parser->state == HTTP_RESPONSE_PARSE_ERROR) return HTTP_RESPONSE_ERROR;
    if (parser->state == HTTP_RESPONSE_PARSE_DONE) return HTTP_RESPONSE_DONE;
    while (offset < length && parser->state != HTTP_RESPONSE_PARSE_DONE &&
           parser->state != HTTP_RESPONSE_PARSE_ERROR) {
        if (parser->state == HTTP_RESPONSE_PARSE_FIXED_BODY ||
            parser->state == HTTP_RESPONSE_PARSE_CHUNK_BODY ||
            parser->state == HTTP_RESPONSE_PARSE_CLOSE_BODY) {
            http_u32 take = length - offset;
            if (parser->state == HTTP_RESPONSE_PARSE_FIXED_BODY) {
                http_u32 remaining = parser->content_length - parser->body_bytes;
                if (take > remaining) take = remaining;
            } else if (parser->state == HTTP_RESPONSE_PARSE_CHUNK_BODY && take > parser->chunk_remaining) {
                take = parser->chunk_remaining;
            }
            if (take == 0 || !http_deliver_body(parser, bytes + offset, take, sink, context)) {
                http_fail(parser);
                break;
            }
            offset += take;
            if (parser->state == HTTP_RESPONSE_PARSE_FIXED_BODY && parser->body_bytes == parser->content_length)
                parser->state = HTTP_RESPONSE_PARSE_DONE;
            else if (parser->state == HTTP_RESPONSE_PARSE_CHUNK_BODY) {
                parser->chunk_remaining -= take;
                if (parser->chunk_remaining == 0) {
                    parser->state = HTTP_RESPONSE_PARSE_CHUNK_CRLF;
                    parser->chunk_crlf_step = 0;
                }
            }
            continue;
        }
        if (parser->state == HTTP_RESPONSE_PARSE_CHUNK_CRLF) {
            http_u8 expected = parser->chunk_crlf_step == 0 ? '\r' : '\n';
            if (bytes[offset++] != expected) {
                http_fail(parser);
                break;
            }
            if (++parser->chunk_crlf_step == 2) {
                parser->chunk_crlf_step = 0;
                parser->state = HTTP_RESPONSE_PARSE_CHUNK_SIZE;
            }
            continue;
        }
        if (parser->state == HTTP_RESPONSE_PARSE_STATUS || parser->state == HTTP_RESPONSE_PARSE_HEADERS ||
            parser->state == HTTP_RESPONSE_PARSE_CHUNK_SIZE || parser->state == HTTP_RESPONSE_PARSE_TRAILERS) {
            int line_result = http_read_line_byte(parser, bytes[offset++]);
            if (line_result < 0) {
                http_fail(parser);
                break;
            }
            if (line_result == 0) continue;
            if (parser->state == HTTP_RESPONSE_PARSE_STATUS) {
                if (!http_parse_status_line(parser)) http_fail(parser);
            } else if (parser->state == HTTP_RESPONSE_PARSE_HEADERS) {
                if (parser->line_length == 0) {
                    if (!http_headers_complete(parser)) http_fail(parser);
                } else if (!http_parse_header_line(parser, 0)) http_fail(parser);
            } else if (parser->state == HTTP_RESPONSE_PARSE_CHUNK_SIZE) {
                if (!http_parse_chunk_size(parser)) http_fail(parser);
            } else if (parser->state == HTTP_RESPONSE_PARSE_TRAILERS) {
                if (parser->line_length == 0) parser->state = HTTP_RESPONSE_PARSE_DONE;
                else if (!http_parse_header_line(parser, 1)) http_fail(parser);
            }
            parser->line_length = 0;
            continue;
        }
        http_fail(parser);
    }
    *consumed = offset;
    if (parser->state == HTTP_RESPONSE_PARSE_ERROR) return HTTP_RESPONSE_ERROR;
    return parser->state == HTTP_RESPONSE_PARSE_DONE ? HTTP_RESPONSE_DONE : HTTP_RESPONSE_MORE;
}
int http_response_finish_eof(http_response_parser *parser) {
    if (!parser || parser->state == HTTP_RESPONSE_PARSE_ERROR) return 0;
    if (parser->state == HTTP_RESPONSE_PARSE_DONE) return 1;
    if (parser->state == HTTP_RESPONSE_PARSE_CLOSE_BODY) {
        parser->state = HTTP_RESPONSE_PARSE_DONE;
        return 1;
    }
    http_fail(parser);
    return 0;
}
static int http_test_sink(void *context, const http_u8 *bytes, http_u32 length) {
    struct http_test_capture *capture = (struct http_test_capture *)context;
    if (!capture || length > sizeof(capture->bytes) - capture->length) return 0;
    for (http_u32 index = 0; index < length; index++) capture->bytes[capture->length + index] = bytes[index];
    capture->length += length;
    return 1;
}
static int http_test_body(const struct http_test_capture *capture, const char *expected, http_u32 length) {
    if (capture->length != length) return 0;
    for (http_u32 index = 0; index < length; index++)
        if (capture->bytes[index] != (http_u8)expected[index]) return 0;
    return 1;
}
int http_response_self_test(void) {
    static const http_u8 fixed[] = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nX-Test: yes\r\n\r\nhelloNEXT";
    static const http_u8 chunked[] = "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\nTransfer-Encoding: Chunked\r\n\r\n4;x=1\r\nWiki\r\n5\r\npedia\r\n0\r\nX-Trailer: yes\r\n\r\nNEXT";
    static const http_u8 close_body[] = "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nclose body";
    static const http_u8 conflict[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nTransfer-Encoding: chunked\r\n\r\noops";
    static const http_u8 duplicate_length[] = "HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx";
    static const http_u8 bad_status[] = "HTTP/1.1 600 Bad Status\r\n\r\nbody";
    static const http_u8 short_body[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nab";
    http_response_parser parser;
    struct http_test_capture capture;
    http_u32 consumed, total_consumed;
    int result;
    capture.length = 0;
    http_response_init(&parser);
    result = http_response_feed(&parser, fixed, sizeof(fixed) - 1, &consumed, http_test_sink, &capture);
    if (result != HTTP_RESPONSE_DONE || consumed != sizeof(fixed) - 1 - 4 || parser.status_code != 200 ||
        !http_test_body(&capture, "hello", 5)) return 0;

    capture.length = 0;
    http_response_init(&parser);
    total_consumed = 0;
    for (http_u32 index = 0; index < sizeof(chunked) - 1; index++) {
        result = http_response_feed(&parser, chunked + index, 1, &consumed, http_test_sink, &capture);
        total_consumed += consumed;
        if (result == HTTP_RESPONSE_ERROR || consumed != 1) return 0;
        if (result == HTTP_RESPONSE_DONE) break;
    }
    if (result != HTTP_RESPONSE_DONE || total_consumed != sizeof(chunked) - 1 - 4 || parser.status_code != 200 ||
        !http_test_body(&capture, "Wikipedia", 9)) return 0;

    capture.length = 0;
    http_response_init(&parser);
    result = http_response_feed(&parser, close_body, sizeof(close_body) - 1, &consumed, http_test_sink, &capture);
    if (result != HTTP_RESPONSE_MORE || consumed != sizeof(close_body) - 1 ||
        !http_test_body(&capture, "close body", 10) || !http_response_finish_eof(&parser) ||
        parser.state != HTTP_RESPONSE_PARSE_DONE) return 0;

    http_response_init(&parser);
    result = http_response_feed(&parser, conflict, sizeof(conflict) - 1, &consumed, 0, 0);
    if (result != HTTP_RESPONSE_ERROR || parser.state != HTTP_RESPONSE_PARSE_ERROR) return 0;

    http_response_init(&parser);
    result = http_response_feed(&parser, duplicate_length, sizeof(duplicate_length) - 1, &consumed, 0, 0);
    if (result != HTTP_RESPONSE_ERROR || parser.state != HTTP_RESPONSE_PARSE_ERROR) return 0;

    http_response_init(&parser);
    result = http_response_feed(&parser, bad_status, sizeof(bad_status) - 1, &consumed, 0, 0);
    if (result != HTTP_RESPONSE_ERROR || parser.state != HTTP_RESPONSE_PARSE_ERROR) return 0;

    http_response_init(&parser);
    result = http_response_feed(&parser, short_body, sizeof(short_body) - 1, &consumed, 0, 0);
    if (result != HTTP_RESPONSE_MORE || http_response_finish_eof(&parser) ||
        parser.state != HTTP_RESPONSE_PARSE_ERROR) return 0;
    return 1;
}
