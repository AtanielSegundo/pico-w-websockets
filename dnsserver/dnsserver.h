// dnsserver.h

#ifndef DNSSERVER_H
#define DNSSERVER_H

#include "lwip/ip_addr.h"
#include "lwip/udp.h"

#define MAX_SITE_NAME 64

typedef struct {
    struct udp_pcb *udp;
    ip_addr_t ip;
    char site_name[MAX_SITE_NAME];
} dns_server_t;

void dns_server_init(dns_server_t *d, ip_addr_t *ip, const char *site_name);
void dns_server_deinit(dns_server_t *d);

#endif // DNSSERVER_H
