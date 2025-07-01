#include "websocket.h"

packet_length ws_build_packet(uint8_t* buffer, uint64_t buffer_len, WS_OPCODE opcode, uint8_t* payload, uint64_t payload_len, int mask) {
    uint64_t headerSize = 2;
    if (payload_len >= 65536) {
        headerSize += 8;
    } else if (payload_len >= 126) {
        headerSize += 2;
    }

    if (mask) {
        headerSize += 4;
    }

    if (headerSize + payload_len > buffer_len) {
        return 1;
    }

    buffer[0] = (1 << 7) | (opcode & 0x0F);     // FIN=1, RSV=0, OPCODE
    buffer[1] = (mask ? 0x80 : 0x00);           // MASK flag

    int payloadIndex = 2;
    
    if (payload_len < 126) {
        buffer[1] |= payload_len;
    } else if (payload_len < 65536) {
        buffer[1] |= 126;
        buffer[2] = (payload_len >> 8) & 0xFF;
        buffer[3] = payload_len & 0xFF;
        payloadIndex = 4;
    } else {
        buffer[1] |= 127;
        for (int i = 0; i < 8; i++) {
            buffer[2+i] = (payload_len >> (56 - i*8)) & 0xFF;
        }
        payloadIndex = 10;
    }

    uint32_t maskKey = 0;
    if (mask) {
        uint32_t key = ((uint32_t)rand() << 16) ^ rand();  
        for (int i = 3; i >= 0; --i) {
            buffer[payloadIndex++] = (uint8_t)(key >> (i * 8));
        }
    }
    
    if (payload_len > 0) {
        if (mask) {
            const uint8_t* m = (uint8_t*)&maskKey;
            for (uint64_t i = 0; i < payload_len; i++) {
                buffer[payloadIndex + i] = payload[i] ^ m[i & 3];
            }
        } else {
            memcpy(buffer + payloadIndex, payload, payload_len);
        }
    }

    return headerSize + payload_len;
}

WS_PARSE_RESULT ws_parse_packet(ws_packet_header_t *header, uint8_t* buffer, uint32_t len) {
    if (len < 2) return WS_PARSE_OVERFLOW;

    header->meta.bytes = (uint16_t) buffer[1] | (uint16_t) buffer[0] << 8;
    uint64_t payloadLen = header->meta.bits.PAYLOADLEN;
    uint32_t headerLen = 2;

    if (payloadLen == 126) {
        if (len < 4) return WS_PARSE_OVERFLOW;
        payloadLen = (buffer[2] << 8) | buffer[3];
        headerLen = 4;
    } else if (payloadLen == 127) {
        if (len < 10) return WS_PARSE_OVERFLOW;
        for(int ii = 2 ; ii < 10 ; ii+=1 ){
            payloadLen |=  (uint64_t)buffer[ii] << (56 - (ii-2)*8);
        }
        headerLen = 10;
    }

    if (header->meta.bits.MASK) {
        if (len < headerLen + 4) return WS_PARSE_OVERFLOW;
        memcpy(header->mask.bytes, buffer + headerLen, 4);
        headerLen += 4;
    }

    if (len < headerLen + payloadLen) return WS_PARSE_OVERFLOW;

    header->start  = headerLen;
    header->length = payloadLen;
    
    if (header->meta.bits.MASK && payloadLen > 0) {
        const uint8_t* m = header->mask.bytes;
        for (uint64_t i = 0; i < payloadLen; i++) {
            buffer[headerLen + i] ^= m[i & 3];
        }
    }

    return WS_PARSE_SUCCESS;
}