# Pico W WebSockets

A lightweight framework para Raspberry Pi Pico W que simplifica:

* **WebSockets** sobre LWIP (bare‑metal, thread‑safe)  
* **HTTP server** minimalista com rotas dinâmicas  
* **Access Point** configurável (SSID, senha, canal)  
* **Captive portal** (redirecionamento 302)  

Funciona em *bare‑metal* com polling (`cyw43_arch_poll()`), mas também pode ser integrado ao FreeRTOS criando uma *task* dedicada para rede.

---

## 📂 Estrutura do repositório

```

├── CMakeLists.txt          # Configuração do Pico SDK
├── configs/
│   └── lwipopts.h          # Cabeçalho de configuração da stack LWIP
├── dhcpserver/             # Retirado do projeto MicroPython, serviço DHCP
├── dnsserver/              # Fork do dnsserver em pico-examples, serviço DNS catch‑all
├── includes/               # Pasta de cabeçalhos
│   ├── ap.h                # Funções para configurar o CYW43 como AP/STA
│   ├── http.h              # Funções para o servidor HTTP (inicialização e adição de rotas)
│   └── stb\_ds.h            # Biblioteca STB de estruturas dinâmicas (hashmap e arrays)
├── picow\_websockets/       # Biblioteca estática com as implementações de WebSocket
│   ├── websocket.h         # Protótipos e documentação dos métodos
│   ├── websocket.c         # Implementação das funções de WebSocket
│   └── CMakeLists.txt      # CMake para build da biblioteca estática
├── routes/                 # Páginas HTML convertidas para .h
│   ├── index.h             # Rota principal
│   ├── mouse.h             # Rota `/mouse`
│   └── status.h            # Rota `/status`
├── src/
│   ├── ap.c                # Configuração do CYW43 (AP/STA)
│   └── http.c              # Servidor HTTP e registro de rotas
├── CMakeLists.txt          # CMake para exemplo de uso do WebSocket
├── htmltoh.py              # Script Python para converter `.html` em `.h`
└── main.c                  # Exemplo de uso da biblioteca picow-websockets

````

---

## 🚀 Objetivo

Prover, com **poucas linhas de código**, um servidor HTTP + WebSockets no Pico W, incluindo:

1. **AP Wi‑Fi**: escolha de SSID e senha, serviço DHCP e colocação automática no canal menos congestionado.  
2. **Domínio personalizado**: configuração de um domínio local para o Pico via DNS catch‑all.  
3. **Captive Portal**: redireciona requisições típicas de detecção de portal cautivo para o domínio principal.  
4. **Rotas HTTP** dinâmicas (GET → handler que preenche o buffer de resposta).  
5. **WebSockets**: handshake, parsing de frames, callbacks de eventos e broadcast.  
6. **Extras**: páginas de exemplo — chat, mouse tracker e status de uptime.

---

## 📝 Exemplo de uso (`main.c`)

```c
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

// Inverte string (in‑place)
void reverse_msg(char *str, size_t len) {
    for (int i = 0, j = len - 1; i < j; i++, j--) {
        char tmp = str[i];
        str[i] = str[j];
        str[j] = tmp;
    }
}

// ROTAS HTTP
void create_index_response(char* buffer, size_t len) {
    snprintf(buffer, len, HTTP_HEADER INDEX_BODY);
}

void create_status_response(char* buffer, size_t len) {
    snprintf(buffer, len, HTTP_HEADER STATUS_BODY);
}

void create_mouse_response(char* buffer, size_t len) {
    snprintf(buffer, len, HTTP_HEADER MOUSE_BODY);
}

// HANDLERS DE WEBSOCKET
void on_text(ws_client_tpcb client, uint8_t* msg, size_t len) {
    const char* route = ws_get_client_route(client);

    if (strcmp(route, "/mouse") == 0) {
        // Recebido do navegador: posição do mouse
        printf("MOUSE (X,Y) = (%.*s)\n", (int)len, msg);
    } else {
        // Outros: inverte e envia de volta
        reverse_msg((char*)msg, len);
        // Feedback visual
        gpio_put(LED_PIN, !gpio_get(LED_PIN));
    }

    ws_send_message(client, WS_OP_TEXT, msg, len);
}

void on_ping(ws_client_tpcb client, uint8_t* msg, size_t len) {
    ws_get_client_ip(client, temp_buffer, sizeof(temp_buffer));
    printf("[INFO] PING recebido, PONG enviado | IP %s\n", temp_buffer);
}

void on_pong(ws_client_tpcb client, uint8_t* msg, size_t len) {
    ws_get_client_ip(client, temp_buffer, sizeof(temp_buffer));
    printf("[INFO] PONG recebido | IP %s\n", temp_buffer);
}

void on_upgrade(ws_client_tpcb client, uint8_t* msg, size_t len) {
    ws_get_client_ip(client, temp_buffer, sizeof(temp_buffer));
    printf("CLIENTE conectado | IP %s\n", temp_buffer);
    ws_send_message(client, WS_OP_TEXT, "[INFO] CONECTADO AO SERVIDOR", 29);
}

void on_close(ws_client_tpcb client, uint8_t* msg, size_t len) {
    ws_get_client_ip(client, temp_buffer, sizeof(temp_buffer));
    printf("CLIENTE desconectado | IP %s\n", temp_buffer);
}

int main() {
    stdio_init_all();

    // Inicializa LED de demonstração
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Inicializa Access Point Wi‑Fi
    setup_access_point("PicoW-Websockets", "mysockets123", "examples.local");

    // Registra rotas HTTP
    add_http_route("/",       create_index_response);
    add_http_route("/index",  create_index_response);
    add_http_route("/status", create_status_response);
    add_http_route("/mouse",  create_mouse_response);

    // Configura WebSocket
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
        if (absolute_time_diff_us(last, get_absolute_time()) >= 1_000_000) {
            last = get_absolute_time();
            uint32_t ms = to_ms_since_boot(get_absolute_time());
            uint32_t h = ms / 3_600_000;
            uint32_t m = (ms / 60_000) % 60;
            uint32_t s = (ms / 1_000) % 60;

            char msg[32];
            int len = snprintf(msg, sizeof(msg),
                               "status:%02u:%02u:%02u", h, m, s);
            ws_send_to_all_clients("/status", WS_OP_TEXT, (uint8_t*)msg, len);
        }

        sleep_ms(5);
    }
}
````

---

## 🔧 Principais blocos de código

### Access Point

```c
void setup_access_point(const char *ssid,
                        const char *password,
                        const char *hostname);
```

* **ssid**: nome da rede
* **password**: `NULL` para rede aberta
* **hostname**: domínio personalizado (ex.: `pico.local`)

### HTTP + Rotas

```c
typedef void(*route_response_handler_t)(char* buf, size_t len);

// Registra rota GET → handler(buf, len)
void add_http_route(const char* path,
                    route_response_handler_t handler);

// Inicia o servidor HTTP na porta 80
void start_http_server(void);
```

No *handler*, preencha `buf` com cabeçalhos + body e faça o que desejar:

```c
void create_index_response(char* buf, size_t len) {
    snprintf(buf, len, HTTP_HEADER INDEX_BODY);
    // Outras ações...
}
```

### Captive Portal

Monitora URLs típicas de detecção de captive portal:

```c
const char* captive_site_routes[] = {
  "GET /generate_204",           // Android
  "GET /hotspot-detect.html",    // iOS/macOS
  "GET /connecttest.txt",        // Windows
  "GET /redirect"                // Genérico (alguns navegadores)
};
```

E responde com *302 Found* para `http://<ip_do_access_point>/`.

### WebSocket

#### Handshake e registro de cliente

```c
int websocket_schema_upgrade(char *payload,
                             struct tcp_pcb *tpcb,
                             struct pbuf *p);
```

#### Callbacks de eventos

```c
ws_add_on_upgrade_handler(ws_message_handler cb);
ws_add_on_text_handler(ws_message_handler cb);
ws_add_on_close_handler(ws_message_handler cb);
ws_add_on_ping_handler(ws_message_handler cb);
ws_add_on_pong_handler(ws_message_handler cb);
```

#### Envio de mensagens

```c
void ws_send_message(ws_client_tpcb wc,
                     WS_OPCODE opcode,
                     uint8_t *msg, size_t len);

void ws_send_to_all_clients(const char* route,
                            WS_OPCODE opcode,
                            uint8_t *msg, size_t len);
```

#### Utilitários

```c
char* ws_get_client_ip(ws_client_tpcb wc, char *buf, size_t buflen);
char* ws_get_client_route(ws_client_tpcb wc);
```

---

## 🖥️ Rotas de exemplo

Capturas de tela das páginas criadas no exemplo:

| Página                       | Imagem                                                                                          |
| ---------------------------- | ----------------------------------------------------------------------------------------------- |
| **Chat** (`/`)               | ![Chat](https://github.com/AtanielSegundo/pico-w-websockets/raw/main/assets/chat.png)          |
| **Mouse Tracker** (`/mouse`) | ![Mouse Tracker](https://github.com/AtanielSegundo/pico-w-websockets/raw/main/assets/mouse.png) |
| **Status** (`/status`)       | ![Status](https://github.com/AtanielSegundo/pico-w-websockets/raw/main/assets/status.png)       |

---


## 🔐 Criptografia

* **Base64** (`b64.h`) por René Nyffenegger (public domain)
* **SHA1** (`tenysha1.h`) por Steve Reid (public domain)

Usadas no cálculo de `Sec-WebSocket-Accept` durante o handshake.

---

## 💾 Build & Deploy

1. Clone o repositório:

   ```bash
   git clone https://github.com/usuario/pico-w-websockets.git
   cd pico-w-websockets
   ```

2. Configure o Pico SDK (`PICO_SDK_PATH`) e crie o build:

   ```bash
   mkdir build && cd build
   cmake .. -DPICO_SDK_PATH=~/pico-sdk
   make
   ```

   Ou importe este projeto na extensão “Raspberry Pi Pico Project” do VS Code e execute “Run Project (USB)”.

3. Arraste o `.uf2` para a unidade *RPI-RP2* no Pico W.

---

## 🤝 Contribuições

Sinta‑se à vontade para abrir **issues**, enviar **pull requests** ou adicionar **novos exemplos** e **funcionalidades**!

---

**Autores:**
– Ataniel Silva Santos Segundo
– René Nyffenegger (`b64.h`)
– Steve Reid (`tenysha1.h`)