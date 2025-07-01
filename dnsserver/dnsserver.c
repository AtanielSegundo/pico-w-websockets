/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>

#include "dnsserver.h"
#include "lwip/udp.h"

#define PORT_DNS_SERVER 53
#define DUMP_DATA 0

#define DEBUG_printf(...)
#define ERROR_printf printf

typedef struct dns_header_t_ {
    uint16_t id;
    uint16_t flags;
    uint16_t question_count;
    uint16_t answer_record_count;
    uint16_t authority_record_count;
    uint16_t additional_record_count;
} dns_header_t;

#define MAX_DNS_MSG_SIZE 300

static int dns_socket_new_dgram(struct udp_pcb **udp, void *cb_data, udp_recv_fn cb_udp_recv) {
    *udp = udp_new();
    if (*udp == NULL) {
        return -ENOMEM;
    }
    udp_recv(*udp, cb_udp_recv, (void *)cb_data);
    return ERR_OK;
}

static void dns_socket_free(struct udp_pcb **udp) {
    if (*udp != NULL) {
        udp_remove(*udp);
        *udp = NULL;
    }
}

static int dns_socket_bind(struct udp_pcb **udp, uint32_t ip, uint16_t port) {
    ip_addr_t addr;
    IP4_ADDR(&addr, (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
    err_t err = udp_bind(*udp, &addr, port);
    if (err != ERR_OK) {
        ERROR_printf("dns failed to bind to port %u: %d", port, err);
        assert(false);
    }
    return err;
}

#if DUMP_DATA
static void dump_bytes(const uint8_t *bptr, uint32_t len) {
    unsigned int i = 0;
    for (i = 0; i < len;) {
        if ((i & 0x0f) == 0) printf("\n");
        else if ((i & 0x07) == 0) printf(" ");
        printf("%02x ", bptr[i++]);
    }
    printf("\n");
}
#endif

static int dns_socket_sendto(struct udp_pcb **udp, const void *buf, size_t len, const ip_addr_t *dest, uint16_t port) {
    if (len > 0xffff) len = 0xffff;
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (!p) {
        ERROR_printf("DNS: Failed to send message out of memory\n");
        return -ENOMEM;
    }
    memcpy(p->payload, buf, len);
    err_t err = udp_sendto(*udp, p, dest, port);
    pbuf_free(p);
    if (err != ERR_OK) {
        ERROR_printf("DNS: Failed to send message %d\n", err);
        return err;
    }
#if DUMP_DATA
    dump_bytes(buf, len);
#endif
    return len;
}

static void dns_server_process(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *src_addr, u16_t src_port) {
    dns_server_t *d = arg;
    DEBUG_printf("dns_server_process %u\n", p->tot_len);

    uint8_t dns_msg[MAX_DNS_MSG_SIZE];
    dns_header_t *dns_hdr = (dns_header_t*)dns_msg;

    size_t msg_len = pbuf_copy_partial(p, dns_msg, sizeof(dns_msg), 0);
    if (msg_len < sizeof(dns_header_t)) goto ignore_request;

#if DUMP_DATA
    dump_bytes(dns_msg, msg_len);
#endif

    uint16_t flags = lwip_ntohs(dns_hdr->flags);
    uint16_t question_count = lwip_ntohs(dns_hdr->question_count);
    if (((flags >> 15) & 0x1) || (((flags >> 11) & 0xf) != 0) || question_count < 1) goto ignore_request;

    // Extrai QNAME
    const uint8_t *qptr = dns_msg + sizeof(dns_header_t);
    const uint8_t *qend = dns_msg + msg_len;
    char qname[256] = {0};
    int qpos = 0;
    while (qptr < qend && *qptr && qpos < (int)sizeof(qname) - 1) {
        int lablen = *qptr++;
        if (lablen > 63) break;
        if (qpos) qname[qpos++] = '.';
        memcpy(&qname[qpos], qptr, lablen);
        qpos += lablen;
        qptr += lablen;
    }
    qname[qpos] = '\0';

    // Avança além do terminador e QTYPE/QCLASS
    while (qptr < qend && *qptr) qptr++;
    qptr += 1 + 4;

    // **CATCH-ALL**: responde para qualquer qname

    // Monta resposta A record
    uint8_t *answer_ptr = dns_msg + (qptr - dns_msg);
    *answer_ptr++ = 0xc0;
    *answer_ptr++ = (uint8_t)(sizeof(dns_header_t)); // Ponteiro para QNAME
    *answer_ptr++ = 0;
    *answer_ptr++ = 1; // Tipo A
    *answer_ptr++ = 0;
    *answer_ptr++ = 1; // Classe IN
    *answer_ptr++ = 0;
    *answer_ptr++ = 0;
    *answer_ptr++ = 0;
    *answer_ptr++ = 60; // TTL
    *answer_ptr++ = 0;
    *answer_ptr++ = 4; // comprimento
    memcpy(answer_ptr, &d->ip.addr, 4);
    answer_ptr += 4;

    dns_hdr->flags = lwip_htons((1 << 15) | (1 << 10) | (1 << 7));
    dns_hdr->question_count = lwip_htons(1);
    dns_hdr->answer_record_count = lwip_htons(1);
    dns_hdr->authority_record_count = 0;
    dns_hdr->additional_record_count = 0;

    DEBUG_printf("Respondendo '%s' -> %s:%d, %d bytes\n",
                 qname, ipaddr_ntoa(src_addr), src_port, answer_ptr - dns_msg);
    dns_socket_sendto(&d->udp, dns_msg, answer_ptr - dns_msg, src_addr, src_port);

ignore_request:
    pbuf_free(p);
}

void dns_server_init(dns_server_t *d, ip_addr_t *ip, const char *site_name) {
    ip_addr_copy(d->ip, *ip);
    strncpy(d->site_name, site_name, MAX_SITE_NAME-1);
    d->site_name[MAX_SITE_NAME-1] = '\0';
    if (dns_socket_new_dgram(&d->udp, d, dns_server_process) != ERR_OK) return;
    if (dns_socket_bind(&d->udp, 0, PORT_DNS_SERVER) != ERR_OK) return;
    DEBUG_printf("DNS server listening on port %d (catch-all)\n", PORT_DNS_SERVER);
}

void dns_server_deinit(dns_server_t *d) {
    dns_socket_free(&d->udp);
}