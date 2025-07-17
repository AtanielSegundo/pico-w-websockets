#include <stdio.h>
#include "pico/stdlib.h"

#include "ap.h"
#include "http.h"
#include "websocket.h"

#include "routes/index.h"
#include "routes/status.h"
#include "routes/mouse.h"

char temp_buffer[512];
#define LED_PIN 11

// Inverte string (in-place)
void reverse_msg(char *str, size_t len) {
    for (int i = 0, j = len - 1; i < j; i++, j--) {
        char temp = str[i];
        str[i] = str[j];
        str[j] = temp;
    }
}

// ROTAS HTTP

void create_index_response(char* query_params, char* buffer, size_t len) {
    snprintf(buffer, len, HTTP_HEADER INDEX_BODY);
}

void create_status_response(char* query_params, char* buffer, size_t len) {
    snprintf(buffer, len, HTTP_HEADER STATUS_BODY);
}

void create_mouse_response(char* query_params, char* buffer, size_t len) {
    snprintf(buffer, len, HTTP_HEADER MOUSE_BODY);
}

// HANDLERS DE WEBSOCKET

void on_text(ws_client_tpcb client, uint8_t* msg, size_t len) {
    const char* route = ws_get_client_route(client);

    if (strcmp(route, "/mouse") == 0) {
        // Recebido do navegador: posição do mouse
        printf("MOUSE (X,Y) = (%.*s)\n", len, msg);
    } else {
        // Resto: inverte e envia de volta
        reverse_msg((char*)msg, len);
        // Feedback visual
        gpio_put(LED_PIN,!gpio_get(LED_PIN));
    }

    ws_send_message(client, WS_OP_TEXT, msg, len);
}

void on_ping(ws_client_tpcb client, uint8_t* msg, size_t len) {
    ws_get_client_ip(client, temp_buffer, sizeof(temp_buffer));
    printf("[INFO] PING RECEBIDO, PONG ENVIADO | IP %s\n", temp_buffer);
}

void on_pong(ws_client_tpcb client, uint8_t* msg, size_t len) {
    ws_get_client_ip(client, temp_buffer, sizeof(temp_buffer));
    printf("[INFO] PONG RECEBIDO | IP %s\n", temp_buffer);
}

void on_upgrade(ws_client_tpcb client, uint8_t* msg, size_t len) {
    ws_get_client_ip(client, temp_buffer, sizeof(temp_buffer));
    printf("CLIENTE CONECTADO | IP %s\n", temp_buffer);
    ws_send_message(client, WS_OP_TEXT, "[INFO] CONECTADO AO SERVIDOR", 29);
}

void on_close(ws_client_tpcb client, uint8_t* msg, size_t len) {
    ws_get_client_ip(client, temp_buffer, sizeof(temp_buffer));
    printf("CLIENTE DESCONECTADO | IP %s\n", temp_buffer);
}

int main() {
    stdio_init_all();

    // Inicializando LED de demonstração
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN,GPIO_OUT);

    // Inicializa ponto de acesso Wi-Fi
    setup_access_point("PicoW-Websockets", "mysockets123", "examples.local");

    // Registra rotas HTTP
    add_http_route("/",        create_index_response);
    add_http_route("/index",  create_index_response);
    add_http_route("/status", create_status_response);
    add_http_route("/mouse",  create_mouse_response);

    // WebSocket: registra esquema e eventos
    add_new_schema_route("websocket", websocket_schema_upgrade);
    ws_add_on_text_handler(on_text);
    ws_add_on_upgrade_handler(on_upgrade);
    ws_add_on_close_handler(on_close);
    ws_add_on_ping_handler(on_ping);
    ws_add_on_pong_handler(on_pong);

    start_http_server();

    absolute_time_t last = get_absolute_time();

    // Loop principal
    while (true) {
        cyw43_arch_poll();

        // Envia "uptime" a cada segundo
        if (absolute_time_diff_us(last, get_absolute_time()) >= 1000000) {
            last = get_absolute_time();

            uint32_t ms = to_ms_since_boot(get_absolute_time());
            uint32_t h = ms / 3600000;
            uint32_t m = (ms / 60000) % 60;
            uint32_t s = (ms / 1000) % 60;

            char msg[32];
            int len = snprintf(msg, sizeof(msg), "status:%02u:%02u:%02u", h, m, s);

            ws_send_to_all_clients("/status", WS_OP_TEXT, msg, len);
        }

        sleep_ms(5);
    }
}