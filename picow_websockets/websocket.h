#ifndef WS_H
#define WS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <lwip/tcp.h>

#define WS_BUFFER_SIZE 2048

/**
 * @typedef ws_client_tpcb
 * @brief Opaque handle for a WebSocket client, represented by a TCP PCB pointer.
 */
typedef struct tcp_pcb* ws_client_tpcb;

/**
 * @typedef output_payload_len
 * @brief Alias for payload length type.
 */
typedef size_t output_payload_len;

/**
 * @typedef ws_message_handler
 * @brief Callback type for handling incoming WebSocket messages or events.
 * @param wc        WebSocket client handle.
 * @param ws_msg    Pointer to the message payload (binary or text).
 * @param ws_msg_len Length of the message payload.
 */
typedef void(*ws_message_handler)(ws_client_tpcb wc, uint8_t* ws_msg, size_t ws_msg_len);

/**
 * @struct ws_context_handlers_t
 * @brief Collection of WebSocket event handler callbacks.
 */
typedef struct {
    ws_message_handler on_text;    /**< Called on receiving a text frame. */
    ws_message_handler on_ping;    /**< Called on receiving a ping frame. */
    ws_message_handler on_pong;    /**< Called on receiving a pong frame. */
    ws_message_handler on_close;   /**< Called on receiving a close frame. */
    ws_message_handler on_upgrade; /**< Called immediately after WebSocket handshake. */
} ws_context_handlers_t;

/**
 * @struct ws_packet_header_t
 * @brief Parsed header of a WebSocket frame.
 */
typedef struct {
   union {
       struct {
           uint8_t PAYLOADLEN : 7;  /**< 7-bit payload length. */
           uint8_t MASK       : 1;  /**< Mask flag. */
           uint8_t OPCODE     : 4;  /**< Operation code (text, binary, close, ping, pong). */
           uint8_t RSV        : 3;  /**< Reserved bits for extensions. */
           uint8_t FIN        : 1;  /**< Final frame flag. */
       } bits;
       uint16_t bytes;             /**< Raw header bytes. */
   } meta;
   uint8_t  start;    /**< Offset to start of payload data. */
   uint64_t length;   /**< Length of the payload. */
   union {
       uint32_t key;            /**< Masking key as 32-bit integer. */
       uint8_t  bytes[4];       /**< Masking key bytes. */
   } mask;
} ws_packet_header_t;

/**
 * @enum WS_OPCODE
 * @brief WebSocket frame opcodes.
 */
typedef enum {
    WS_OP_CONTINUE = 0x0, /**< Continuation frame. */
    WS_OP_TEXT     = 0x1, /**< Text frame. */
    WS_OP_BIN      = 0x2, /**< Binary frame. */
    WS_OP_CLOSE    = 0x8, /**< Connection close. */
    WS_OP_PING     = 0x9, /**< Ping frame. */
    WS_OP_PONG     = 0xA  /**< Pong frame. */
} WS_OPCODE;

/**
 * @enum WS_PARSE_RESULT
 * @brief Result codes for parsing a WebSocket frame.
 */
typedef enum {
    WS_PARSE_SUCCESS  =  0, /**< Frame parsed successfully. */
    WS_PARSE_OVERFLOW = -1, /**< Buffer overflow during parse. */
    WS_PARSE_UNKNOWN  = -2  /**< Unknown parse error. */
} WS_PARSE_RESULT;

/**
 * @struct ws_client
 * @brief Represents a connected WebSocket client and its associated route.
 */
typedef struct {
    ws_client_tpcb tpcb;  /**< TCP PCB pointer for the client. */
    char*          route; /**< HTTP route used for upgrade. */
} ws_client;

/**
 * @struct ws_connected_clients_t
 * @brief Dynamic array of connected WebSocket clients.
 */
typedef struct {
    ws_client *items;     /**< Array of client entries. */
    size_t     count;     /**< Current number of clients. */
    size_t     capacity;  /**< Allocated capacity of the array. */
} ws_connected_clients_t;

/**
 * @typedef packet_length
 * @brief Type for WebSocket packet length values.
 */
typedef uint64_t packet_length;

/**
 * @brief Build a WebSocket frame.
 * @param buffer       Buffer to write the frame into.
 * @param buffer_len   Size of the buffer.
 * @param opcode       WebSocket opcode for the frame.
 * @param payload      Payload data pointer.
 * @param payload_len  Length of payload data.
 * @param mask         Non-zero to mask the payload.
 * @return Number of bytes written to buffer.
 */
packet_length ws_build_packet(uint8_t* buffer, packet_length buffer_len,
                              WS_OPCODE opcode, uint8_t* payload,
                              packet_length payload_len, int mask);

/**
 * @brief Parse a WebSocket frame header.
 * @param header Pointer to header struct to fill.
 * @param buffer Input buffer containing raw frame.
 * @param len    Length of input buffer.
 * @return Parse result code.
 */
WS_PARSE_RESULT ws_parse_packet(ws_packet_header_t *header,
                                uint8_t* buffer, uint32_t len);

/**
 * @brief Extract the Sec-WebSocket-Key from an HTTP request.
 * @param req      Null-terminated HTTP header string.
 * @param out_key  Buffer to store the extracted key.
 * @param maxlen   Maximum length of the output buffer.
 * @return true on success, false otherwise.
 */
bool extract_ws_key(const char *req, char *out_key, size_t maxlen);

/**
 * @brief Perform a WebSocket handshake upgrade.
 * @param payload_buffer Full HTTP upgrade request buffer.
 * @param tpcb           TCP PCB pointer of the incoming connection.
 * @param p              pbuf containing the HTTP request.
 * @return ERR_OK on success or an lwIP err_t code on failure.
 */
err_t websocket_schema_upgrade(char* payload_buffer, struct tcp_pcb *tpcb, struct pbuf *p);

/**
 * @brief Retrieve the remote IP of a WebSocket client.
 * @param wc       WebSocket client handle.
 * @param buf      Buffer to write IP string into.
 * @param buflen   Size of the IP buffer.
 * @return Pointer to buf or NULL on error.
 */
char* ws_get_client_ip(ws_client_tpcb wc, char *buf, size_t buflen);

/**
 * @brief Retrieve the HTTP route that was used for the WebSocket upgrade.
 * @param wc  WebSocket client handle.
 * @return Pointer to route string or NULL if not found.
 */
char* ws_get_client_route(ws_client_tpcb wc);

/**
 * @brief Send a WebSocket message to a single client.
 * @param wc      WebSocket client handle.
 * @param opcode  WebSocket opcode (text, binary, etc.).
 * @param msg     Pointer to message payload.
 * @param msg_len Length of the message payload.
 */
void ws_send_message(ws_client_tpcb wc, WS_OPCODE opcode,
                     uint8_t *msg, packet_length msg_len);

/**
 * @brief Broadcast a WebSocket message to all clients on a given route.
 * @param route   HTTP route used by target clients.
 * @param opcode  WebSocket opcode.
 * @param msg     Message payload.
 * @param msg_len Payload length.
 */
void ws_send_to_all_clients(const char* route, WS_OPCODE opcode,
                            uint8_t *msg, packet_length msg_len);

/**
 * @brief Register a callback for incoming text frames.
 * @param handler Function to call on text frame.
 */
void ws_add_on_text_handler(ws_message_handler handler);

/**
 * @brief Register a callback for incoming ping frames.
 * @param handler Function to call on ping frame.
 */
void ws_add_on_ping_handler(ws_message_handler handler);

/**
 * @brief Register a callback for incoming pong frames.
 * @param handler Function to call on pong frame.
 */
void ws_add_on_pong_handler(ws_message_handler handler);

/**
 * @brief Register a callback for incoming close frames.
 * @param handler Function to call on close frame.
 */
void ws_add_on_close_handler(ws_message_handler handler);

/**
 * @brief Register a callback invoked immediately after a successful WebSocket handshake.
 * @param handler Function to call on upgrade event.
 */
void ws_add_on_upgrade_handler(ws_message_handler handler);

#endif /* WS_H */
