#ifndef HTTP_H

#define HTTP_H

#include <string.h>
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "common.h"

extern char dnss_captive_site[64];
#define KB(v) (v*1024U)

#define HTTP_RESPONSE_BUFFER_SIZE KB(4)
#define PAYLOAD_TEMP_BUFFER_SIZE  KB(4)

typedef void(*route_response_handler_t)(char* http_response_buffer, size_t http_response_len);
typedef err_t(*new_schema_handler_t)(char* payload_buffer,struct tcp_pcb *tpcb, struct pbuf *p);

typedef struct {
    const char* route_path;
    size_t route_path_len;
    route_response_handler_t route_response_handler;
} http_route_t;

typedef struct {
    size_t route_path_len;
    route_response_handler_t route_response_handler;
} http_route_item_t;

typedef struct {
    const char* key; //route_path;
    http_route_item_t value;
} http_routes_hashmap_t;

typedef struct {
    const char* new_schema;
    new_schema_handler_t new_schema_handler;
} new_schema_route_t;

typedef struct {
    const char* key;
    new_schema_handler_t value;
} new_schemas_hashmap_t;

typedef struct {
    http_route_t *items;
    size_t count;
    size_t capacity;
} http_routes_t;

typedef struct {
    new_schema_route_t *items;
    size_t count;
    size_t capacity;
} new_schemas_routes_t;

void start_http_server(void);
void add_http_route(const char* route_path, route_response_handler_t route_response_handler);
void add_new_schema_route(const char* new_schema, new_schema_handler_t new_schema_handler);

#define LB(str) str"\r\n"


#define DNS_CAPTIVE_RESPONSE_HEADER \
        LB("HTTP/1.1 302 Found") \
        LB("Location: http://%s/") \
        LB("Cache-Control: no-cache, no-store, must-revalidate") \
        LB("Connection: close") \
        LB("Content-Length: 0") \
        LB("") 

#define HTTP_HEADER \ 
        LB("HTTP/1.1 200 OK") \
        LB("Content-Type: text/html; charset=UTF-8") \
        LB("Cache-Control: no-cache, no-store, must-revalidate") \
        LB("Connection: close") \
        LB("") \

#endif