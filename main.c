#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define RAPIDHTTP_IMPLEMENTAION
#include "./rapidhttp.h"

#define HOST "tsolding.org"
#define PORT "80"

ssize_t rapidhttp_write(RapidHTTP_Socket socket, const void *buf, size_t count)
{
    return write((int) (int64_t) socket, buf, count);
}

ssize_t rapidhttp_read(RapidHTTP_Socket socket, void *buf, size_t count)
{
    return read((int) (int64_t) socket, buf, count);
}

int main()
{
    struct addrinfo hints = {0};
    hints.ai_flags = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol =IPPROTO_TCP;

    struct addrinfo *addrs;
    if (getaddrinfo(HOST, PORT, &hints, &addrs) < 0) {
        fprintf(stderr, "Could not get address of `"HOST"`: %s\n", strerror(errno));
        exit(1);
    }

    int sd = 0;
    for (struct addrinfo *addr = addrs; addr != NULL; addr = addr -> ai_next) {
        sd = socket(addr -> ai_family, addr -> ai_socktype, addr -> ai_protocol);

        if (sd == -1) break;
        if (connect(sd, addr -> ai_addr, addr -> ai_addrlen) == 0) break;

        close(sd);
        sd = -1;
    }
    freeaddrinfo(addrs);

    if (sd == -1) {
        fprintf(stderr, "Could not connect to "HOST":"PORT": %s\n", strerror(errno));
        exit(1);
    }

    static RapidHTTP rapidhttp = {
        .write = rapidhttp_write,
        .read = rapidhttp_read,
    };
    rapidhttp.socket = (void*) (int64_t) sd;

    rapidhttp_request_begin(&rapidhttp, RapidHTTP_GET, "/index.html");
    {
        rapidhttp_request_header(&rapidhttp, "Host", HOST);
        rapidhttp_request_headers_end(&rapidhttp);
    }
    rapidhttp_request_end(&rapidhttp);

    rapidhttp_response_begin(&rapidhttp);
    {
        // Status code
        {
            uint64_t code = rapidhttp_response_status_code(&rapidhttp);
            printf("Status code: %lu\n", code);
        }

        // Headers
        {
            String_View name, value;
            while (rapidhttp_response_next_header(&rapidhttp, &name, &value))
            {
                printf("==================================\n");
                printf("Header name: "SV_Fmt"\n", SV_Arg(name));
                printf("Header value: "SV_Fmt"\n", SV_Arg(value));
            }
            printf("==================================\n");
        }

        // Body 
        {
            String_View chunk;
            while (rapidhttp_response_next_body_chunk(&rapidhttp, &chunk)){
                printf(SV_Fmt, SV_Arg(chunk));
            }
        }
    }
    rapidhttp_response_end(&rapidhttp);

    close(sd);

    return 0;
}
