#include "websocket.h"
#include "encrypt.h"

ws_connected_clients_t* ws_connected_clients = NULL;
ws_context_handlers_t ws_context_handlers = {0};

static uint8_t frame_buf[WS_BUFFER_SIZE];
static uint8_t out_buf[WS_BUFFER_SIZE];

bool extract_ws_key(const char *req, char *out_key, size_t maxlen) {
    static const char key[] = "Sec-WebSocket-Key:";
    const char *p = strstr(req, key);
    if (!p) return false;
    
    p += sizeof(key) - 1;
    while (*p == ' ') p++;
    
    const char *e = strstr(p, "\r\n");
    if (!e || (size_t)(e - p) >= maxlen) return false;
    
    size_t len = e - p;
    memcpy(out_key, p, len);
    out_key[len] = '\0';
    return true;
}

/**
 * Preenche `buf` com o IP remoto do cliente WebSocket `wc`, no formato "x.x.x.x".
 * @param wc      Ponteiro para tcp_pcb (ws_client).
 * @param buf     Buffer onde a string será escrita.
 * @param buflen  Tamanho de `buf` em bytes.
 * @return        Ponteiro para `buf`, ou NULL em caso de erro.
 */
char* ws_get_client_ip(ws_client_tpcb wc, char *buf, size_t buflen) {
    if (!wc || !buf || buflen == 0) return NULL;
    
    ip_addr_t peer = wc->remote_ip;

    char *res = ipaddr_ntoa_r(&peer, buf, buflen);
    return res;
}

char* ws_get_client_route(ws_client_tpcb wc){
    if(ws_connected_clients == NULL) return NULL;
    for(int ii = 0 ; ii < ws_connected_clients->count ; ii++){
        if(ws_connected_clients->items[ii].tpcb == wc){
            return ws_connected_clients->items[ii].route;
        }
    }
}

void ws_add_on_text_handler(ws_message_handler handler){
    ws_context_handlers.on_text = handler; 
};
void ws_add_on_ping_handler(ws_message_handler handler){
    ws_context_handlers.on_ping = handler; 
};
void ws_add_on_pong_handler(ws_message_handler handler){
    ws_context_handlers.on_pong = handler; 
};
void ws_add_on_close_handler(ws_message_handler handler){
    ws_context_handlers.on_close = handler; 
};
void ws_add_on_upgrade_handler(ws_message_handler handler){
    ws_context_handlers.on_upgrade = handler; 
};

void ws_send_message(ws_client_tpcb wc, WS_OPCODE opcode, uint8_t *msg, uint64_t msg_len){
    uint64_t out_len = ws_build_packet(out_buf, WS_BUFFER_SIZE,opcode,msg, msg_len,0);
    tcp_write(wc, out_buf, out_len, TCP_WRITE_FLAG_COPY);   
}

static err_t websocket_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }
    
    tcp_recved(tpcb, p->tot_len);
    
    ws_packet_header_t hdr;

    if (p->tot_len > WS_BUFFER_SIZE) {
        tcp_close(tpcb);
        pbuf_free(p);
        return ERR_MEM;
    }

    uint32_t offset = 0;
    for (struct pbuf *q = p; q; q = q->next) {
        if (offset + q->len > WS_BUFFER_SIZE) {
            pbuf_free(p);
            return ERR_MEM;
        }
        memcpy(frame_buf + offset, q->payload, q->len);
        offset += q->len;
    }
    
    uint32_t total_len = offset;

    ws_parse_packet(&hdr, frame_buf, total_len);

    switch (hdr.meta.bits.OPCODE) {
        case WS_OP_TEXT: {
            uint32_t msg_len = hdr.length;
            
            char *msg = malloc(msg_len);
            memcpy(msg, frame_buf + hdr.start, msg_len);
            
            if(ws_context_handlers.on_text){
                ws_context_handlers.on_text(tpcb,msg,msg_len);
            }
            
            free(msg);
            break;
        }

        case WS_OP_PING: {
            uint64_t out_len = ws_build_packet(out_buf,WS_BUFFER_SIZE,WS_OP_PONG,frame_buf + hdr.start,hdr.length,0);
            tcp_write(tpcb, out_buf, out_len, TCP_WRITE_FLAG_COPY);
            if(ws_context_handlers.on_ping){
                ws_context_handlers.on_ping(tpcb,NULL,0);
            }
            break;
        }

        case WS_OP_PONG:
            if(ws_context_handlers.on_pong){
                ws_context_handlers.on_pong(tpcb,NULL,0);
            }
            break;

        case WS_OP_CLOSE: {
            if(ws_context_handlers.on_close){
                ws_context_handlers.on_close(tpcb,NULL,0);
            }
            uint64_t out_len = ws_build_packet(out_buf,WS_BUFFER_SIZE,WS_OP_CLOSE,frame_buf + hdr.start,hdr.length,0);
            tcp_write(tpcb, out_buf, out_len, TCP_WRITE_FLAG_COPY);
            tcp_output(tpcb);
            pbuf_free(p);
            tcp_close(tpcb);
            
            for(int ii = 0 ; ii < ws_connected_clients->count ; ii++){
                if(ws_connected_clients->items[ii].tpcb == tpcb){
                    ws_connected_clients->items[ii].tpcb = NULL;
                    free(ws_connected_clients->items[ii].route);
                }
            }

            return ERR_OK;
        }

        default:
            printf("WS OPCODE %u not supported\n", hdr.meta.bits.OPCODE);
            break;
    }

    tcp_output(tpcb);
    pbuf_free(p);
    
    return ERR_OK;
}

void ws_send_to_all_clients(const char* route,WS_OPCODE opcode, uint8_t *msg, uint64_t msg_len){
    if(ws_connected_clients == NULL) return;
    for(int ii = 0 ; ii < ws_connected_clients->count ; ii++){
        if(ws_connected_clients->items[ii].tpcb && strcmp(ws_connected_clients->items[ii].route,route) == 0){
            ws_send_message(ws_connected_clients->items[ii].tpcb,opcode,msg,msg_len);
        }
    }
};

int websocket_handshake(struct tcp_pcb *tpcb, char *req) {
    uint8_t client_key[256];
    uint8_t accept_key[256];
    uint8_t resp[1024];
    int len;

    if (!extract_ws_key(req, client_key, sizeof(client_key))) {
        return ERR_ARG;
    }
    compute_ws_accept(client_key, accept_key);
    
    len = snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key
    );

    tcp_write(tpcb, resp, len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    tcp_recv(tpcb, websocket_recv);

    return ERR_OK;
}

err_t websocket_schema_upgrade(char* payload_buffer,struct tcp_pcb *tpcb, struct pbuf *p){
    int err = websocket_handshake(tpcb, payload_buffer);
    if (err == ERR_OK){
        if(ws_context_handlers.on_upgrade){
            ws_context_handlers.on_upgrade(tpcb,NULL,0);
        }

        if(ws_connected_clients == NULL){
            ws_connected_clients = (ws_connected_clients_t*)calloc(1,sizeof(ws_connected_clients_t));
        } 
        
        if (ws_connected_clients->count >= ws_connected_clients->capacity) {                                         
            if (ws_connected_clients->capacity == 0) ws_connected_clients->capacity = 4;                           
            else ws_connected_clients->capacity *= 2;                                                
            ws_connected_clients->items = realloc(ws_connected_clients->items, ws_connected_clients->capacity*sizeof(*ws_connected_clients->items)); 
        }                         

        int len = p->tot_len < sizeof(payload_buffer) - 1
                  ? p->tot_len
                  : sizeof(payload_buffer) - 1;

        char *line_end = strstr(payload_buffer, "\r\n");
        if (!line_end) line_end = payload_buffer + len;
        *line_end = '\0';
        char* route = (char*)malloc(64*sizeof(char));
        char method[8];
        sscanf(payload_buffer, "%7s %63s", method, route);
        ws_connected_clients->items[ws_connected_clients->count++] = (ws_client){.tpcb=tpcb,.route=route};
    }        
    pbuf_free(p);
    return err;
};