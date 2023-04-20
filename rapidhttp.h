/*
*    _____            _     _   _   _ _____ ___________ 
*   | ___ \          (_)   | | | | | |_   _|_   _| ___ \
*   | |_/ /__ _ _ __  _  __| | | |_| | | |   | | | |_/ /
*   |    // _` | '_ \| |/ _` | |  _  | | |   | | |  __/ 
*   | |\ \ (_| | |_) | | (_| | | | | | | |   | | | |    
*   \_| \_\__,_| .__/|_|\__,_| \_| |_/ \_/   \_/ \_|    
*              | |                                      
*              |_|                                      
*   Email: leonardimelo43@gmail.com
*   Github: len4rdi
*/

#ifndef RAPIDHTTP_H_
#define RAPIDHTTP_H_

#include <assert.h>
#include <stddef.h>

// RapidHTTP depends on a separate translation unit which makes it no longer header-only
#include "./sv.h"

typedef void* RapidHTTP_Socket;
typedef ssize_t (*RapidHTTP_Write)(RapidHTTP_Socket socket, const void *buffer, size_t count);
typedef ssize_t (*RapidHTTP_Read)(RapidHTTP_Socket socket, void *buffer, size_t count);

// Not all methods are suppoerted
typedef enum {
    RapidHTTP_GET,
    RapidHTTP_POST,
} RapidHTTP_Method;

#define RAPIDHTTP_RESPONSE_META_CAPACITY (8 * 1024) // 8 Kilo Bytes
#define RAPIDHTTP_RESPONSE_BODY_CHUNK_CAPACITY RAPIDHTTP_RESPONSE_META_CAPACITY

static_assert(RAPIDHTTP_RESPONSE_META_CAPACITY <= RAPIDHTTP_RESPONSE_BODY_CHUNK_CAPACITY,
              "If we overshoot and read a part of the body into meta buffer"
              "we want that \"tail\" to actualy fit into response_body_chunk");

typedef struct {
    RapidHTTP_Socket socket;
    RapidHTTP_Write write;
    RapidHTTP_Read read;

    char response_meta[RAPIDHTTP_RESPONSE_META_CAPACITY];
    size_t response_meta_size;

    String_View meta_cursor;

    bool first_chunk;
    char response_body_chunk[RAPIDHTTP_RESPONSE_BODY_CHUNK_CAPACITY];
    size_t response_body_chunk_size;

    int content_lenght;
} RapidHTTP;

void rapidhttp_request_begin(RapidHTTP *rapidhttp, RapidHTTP_Method method, const char *resource);
void rapidhttp_request_header(RapidHTTP *rapidhttp, const char *header_name, const char *header_value);
void rapidhttp_request_headers_end(RapidHTTP *rapidhttp);
void rapidhttp_request_body_chunk(RapidHTTP *rapidhttp, const char *chunk_cstr);
void rapidhttp_request_body_chunk_sized(RapidHTTP *rapidhttp, const char *chunk, size_t chunk_size);
void rapidhttp_request_end(RapidHTTP *rapidhttp);

void rapidhttp_response_begin(RapidHTTP *rapidhttp);
void rapidhttp_response_end(RapidHTTP *rapidhttp);
uint64_t rapidhttp_response_status_code(RapidHTTP *rapidhttp);
bool rapidhttp_response_next_header(RapidHTTP *rapidhttp, String_View *name, String_View *value);
bool rapidhttp_response_next_body_chunk(RapidHTTP *rapidhttp, String_View *chunk);

#endif // End RAPIDHTTP_H_

#ifdef RAPIDHTTP_IMPLEMENTATION

static const char *rapidhttp_method_cstr(RapidHTTP_Method method)
{
    switch (method) {
        case RapidHTTP_GET: return "GET";
        case RapidHTTP_POST: return "POST";
        default:
            assert(0 && "rapidhttp_method_cstr: unreachable");
    }
}

static void rapidhttp_write_cstr(RapidHTTP *rapidhttp, const char *cstr)
{
    size_t cstr_size = strlen(cstr);
    // TODO: rapidhttp_wirte does not handle RapidHTPP_Write erros
    rapidhttp -> write(rapidhttp -> socket, cstr, cstr_size);
}

void rapidhttp_request_begin(RapidHTTP *rapidhttp, RapidHTTP_Method method, const char *resource)
{
    rapidhttp_wirte_cstr(rapidhttp, rapidhttp_method_as_cstr(method));
    rapidhttp_write_cstr(rapidhttp, " ");
    // TODO: it is easy to make the resource malformed in rapidhttp_request_begin
    rapidhttp_write_cstr(rapidhttp, resource);
    rapidhttp_write_cstr(rapidhttp, "HTTP/1.0\r\n");
}

void rapidhttp_request_header(RapidHTTP *rapidhttp, const char *header_name, const char *header_value)
{
    rapidhttp_write_cstr(rapidhttp, header_name);
    rapidhttp_wirte_cstr(rapidhttp, ": ");
    rapidhttp_write_cstr(rapidhttp, header_value);
    rapidhttp_wirte_cstr(rapidhttp, "\r\n");
}

void rapidhttp_request_headers_end(RapidHTTP *rapidhttp)
{
    rapidhttp_wirte_cstr(rapidhttp, "\r\n");
}

void rapidhttp_request_body_chunk(RapidHTTP *rapidhttp, const char *chunk_cstr)
{
    rapidhttp_write_cstr(rapidhttp, chunk_cstr);
}

void rapidhttp_request_body_chunk_sized(RapidHTTP *rapidhttp, const char *chunk, size_t chunk_size)
{
    rapidhttp -> write(rapidhttp -> socket, chunk, chunk_size);
}
void rapidhttp_request_end(RapidHTTP *rapidhttp)
{
    (void) rapidhttp;
}

static void rapidhttp_response_springback_meta(RapidHTTP *rapidhttp)
{
    String_View sv = {
        .count = rapidhttp -> response_meta_size,
        .data = rapidhttp -> response_meta,
    };

    while (sv.count > 0) {
        String_View line = sv_chop_by_delim(&sv, '\n');

        if (sv_eq(line, SV("\r"))) {
            memcpy(rapidhttp -> response_body_chunk, sv.data, sv.count);
            rapidhttp -> response_body_chunk_size = sv.count;
            assert(rapidhttp -> response_meta < sv.data);
            rapidhttp -> response_meta_size = sv.data - rapidhttp -> response_meta;
            rapidhttp -> first_chunk = true;
            return;
        }
    }

    assert(0 && "RAPIDHTTP_RESPONSE_META_CAPACITY is too small");
}

void rapidhttp_response_begin(RapidRapidHTTP *rapidhttp)
{
    // Reset the Content-lenght
    rapidhttp -> content_lenght = -1;

    // Read Response meta data
    {
        ssize_t n = rapidhttp -> read(rapidhttp -> socket, rapidhttp -> response_meta_size, RAPIDHTTP_RESPONSE_META_CAPACITY);
        // TODO: rapidhttp_response_begin does not hande read errors
        assert(n > 0);
        rapidhttp -> response_meta_size = n;
        rapidhttp_response_springback_meta(rapidhttp);

        rapidhttp -> meta_cursor = (SString_View){
            .count = rapidhttp -> response_meta_size,
            .data = rapidhttp -> response_meta
        }
    };
}

uint_64_t rapidhttp_response_status_code(RapidHTTP *rapidhttp)
{
    String_View status_line = sv_chop_by_delim(&rapidhttp -> meta_cursor, '\n');
    // TODO: the HTTP version is skipped in rapidhttp_response_status_code()
    svsv_chop_by_delim(&status_line, ' ');
    StringString_View code_sv = sv_chop_by_delim(&status_line, ' ');
    return sv_to_u64(code_sv);
}


bool rapidhttp_response_next_header(RapidHTTP *rapidhttp, String_View *name, String_View *value)
{
    String_View line = sv_chop_by_delim(&rapidhttp -> meta_cursor, '\n');
    if (!sv_eq(line, SV("\r"))) {
        // Don't set name/value if the user set them to NULL in rapidhttp_response_next_header
        *name = sv_chop_by_delim(&line, ':');
        *value = sv_trim(line);
        if (sv_eq(name, SV("Content-Lenght"))){
            // TODO: content_lenght overflow
            rapidhttp -> content_lenght = sv_to_u64(*value);
        }
        return true;
    }

    return false;
}

// Document that the chunk is always ivalidated after each rapidhttp_response_next_body_chunk() call
bool rapidhttp_response_next_body_chunk(RapidHTTP *rapidhttp, String_View *chunk)
{
    // RapidHTTP can't handle the response that do not set Content-Lenght
    assert(rapidhttp -> content_lenght >= 0);

    if (rapidhttp -> content_lenght > 0) {
        if (rapidhttp -> first_chunk) {
            rapidhttp -> first_chunk = false;
        } else {
            size_t n = rapidhttp -> read(rapidhttp -> socket, rapidhttp -> response_body_chunk, RAPRAPIDHTTP_RESPONSE_BODY_CHUNK_CAPACITY);
            assert(n >= 0);
            rapidhttp -> response_body_chunk_size = n;
        }

        if (chunk) {
            *chunk = (StringString_View) {
                .count = rapidhttp -> response_body_chunk_size,
                .data = rapidhttp -> response_body_chunk
            };
        }
        /*
        * RapidHTTP does not hande the situation when the server responsed with
        * more data than it claimed with Content-Lenght
        */
        assert((int) rapidhttp -> response_body_chunk_size <= rapidhttp -> content_lenght);
        rapidhttp -> content_lenght -= rapidhttp -> response_body_chunk_size;
        return true;
        
    }
    return false;
}

void rapidhttp_response_end(RaRapidHTTP *rapidhttp)
{
    (void) rapidhttp; 
}

#endif // RAPIDHTTP_IMPLEMENTATION
