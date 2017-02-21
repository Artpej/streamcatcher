#ifndef __NETWORK_H__
#define __NETWORK_H__

#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
#include <winsock2.h>
#include <unistd.h>
#include <stdio.h>
//#pragma comment(lib, "ws2_32.lib")
typedef SOCKET sock;
#else
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
typedef int sock;
#endif


typedef struct _packet {
    char boundary[64];
    char type[128];
    long size;
    char * payload;
} packet;


sock socket_create(const char * host, int port);

char socket_read(sock s);

void socket_write(sock s, char * buffer, int size);

void socket_close(sock s);

packet * read_packet(sock s);

void write_packet(sock s, packet * p);

void free_packet(packet * p);

#endif

#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)


void _print_last_error()
{
    int errCode = WSAGetLastError();
    LPSTR errString = NULL;
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                   0, errCode, 0, (LPSTR)&errString, 0, 0 );
    printf( "Error code %d:  %s\n", errCode, errString ) ;
    LocalFree( errString ) ;
}

sock socket_create(const char * host, int port)
{
    WSADATA WSAData;
    WSAStartup(MAKEWORD(2,0), &WSAData);
    SOCKADDR_IN sin;
    sock s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL,0,0 );
    if(s == INVALID_SOCKET) {
        _print_last_error();
    }
    struct hostent *hostinfo = gethostbyname(host);
    sin.sin_addr.s_addr = ((struct in_addr *)(hostinfo->h_addr))->s_addr;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    if( connect(s, (SOCKADDR *)&sin, sizeof(sin)) == SOCKET_ERROR ) {
        _print_last_error();
    }
    return s;

}

char socket_read(sock s)
{
    char c[1];
    int res = recv(s, c, 1, 0);
    if(res == SOCKET_ERROR) {
        _print_last_error();
    }
    return c[0];
}

void socket_write(sock s, char * buffer, int size)
{
    if(send(s, buffer, size, 0) == SOCKET_ERROR) {
        _print_last_error();
    }

}

void socket_close(sock s)
{
    close(s);
    WSACleanup();
}


#else

void _print_last_error()
{
    printf("Error code %d:  %s\n",errno, strerror(errno));
}

sock socket_create(const char * host, int port)
{
    struct sockaddr_in sin;
    sock s = socket(AF_INET, SOCK_STREAM, 0);
    if( s < 0) {
        _print_last_error();
    }
    struct hostent *hostinfo = gethostbyname(host);
    sin.sin_addr.s_addr = *(in_addr_t *) hostinfo->h_addr_list[0];
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    if( connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0 ) {
        _print_last_error();
    }
    return s;
}

char socket_read(sock s)
{
    char c[1];
    int res = recv(s, c, sizeof(char), 0);
    if(res < 0) {
        _print_last_error();
    }
    return c[0];
}

void socket_write(sock s, char * buffer, int size)
{
    if(send(s, buffer, size, 0) < 0) {
        _print_last_error();
    }
}

void socket_close(sock s)
{
    close(s);
}

#endif

char * read_trimmed_line(sock s)
{
    char c = socket_read(s);
    int ptr = 0;
    char buffer[1024];
    while(c && ptr < 1024) {
        if(c == '\n') {
            buffer[ptr] = '\0';
            ptr++;
            break;
        }
        if(c == ' ' || c == '\r') {
            c = socket_read(s);
            continue;
        }
        buffer[ptr] = c;
        ptr++;
        c = socket_read(s);
    }
    char * line = malloc(ptr * sizeof(char));
    strncpy(line, buffer, ptr);
    return line;

}



packet * read_packet(sock s)
{
    char * line;
    packet * pkt = malloc(sizeof(packet));
    line = read_trimmed_line(s);
    sscanf(line, "--%s",  pkt->boundary);
    free(line);
    line = read_trimmed_line(s);
    sscanf(line, "Content-Type:%s",  pkt->type);
    free(line);
    line = read_trimmed_line(s);
    sscanf(line, "Content-Length:%ld",  &pkt->size);
    free(line);
    free(read_trimmed_line(s));
    pkt->payload = malloc(pkt->size * sizeof(char));
    for(int i = 0; i < pkt->size; i++) {
        pkt->payload[i] = socket_read(s);
    }
    pkt->payload[pkt->size] = '\0';
    return pkt;
}

void write_packet(sock s, packet * p)
{
    char buffer[1024];
    int size = 0;
    size = sprintf(buffer,"--%s\r\n", p->boundary);
    socket_write(s, buffer , size);
    size = sprintf(buffer,"Content-Type:%s\r\n", p->type);
    socket_write(s, buffer , size);
    size = sprintf(buffer,"Content-Length:%ld\r\n\r\n", p->size);
    socket_write(s, buffer , size);
    socket_write(s, p->payload , p->size);
}


void free_packet(packet * p)
{
    free(p->payload);
    free(p);
}

