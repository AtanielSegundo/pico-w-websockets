#ifndef ENCRYPT_H
#define ENCRYPT_H

#define TEENY_SHA1_IMPLEMENTATION
#include "tenysha1.h"

#define B64_IMPLEMENTATION
#include "b64.h"

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

void compute_ws_accept(const char *key, char *out_accept) {
    uint8_t sha1_in[60];
    uint8_t sha1_out[20];
    char concat[ sizeof(sha1_in) ];
    size_t keylen = strlen(key);
    
    // concatena key + GUID
    memcpy(concat, key, keylen);
    memcpy(concat + keylen, WS_GUID, strlen(WS_GUID));
    size_t totlen = keylen + strlen(WS_GUID);
    
    SHA1 sha1 = {0};
    sha1_reset(&sha1);
    sha1_process_bytes(&sha1, concat, totlen);
    digest8_t digest;
    sha1_get_digest_bytes(&sha1, digest);
    size_t sec_websocket_accept_len = b64_encode_out_len(sizeof(digest)) + 1;
    b64_encode((void*)digest, sizeof(digest), out_accept, sec_websocket_accept_len, B64_STD_ALPHA, B64_DEFAULT_PAD);
    out_accept[sec_websocket_accept_len-1] = '\0';
}

#endif