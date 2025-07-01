# Pico W WebSockets

A lightweight framework para Raspberry Pi Pico W que simplifica:

* **WebSockets** sobre LWIP (bare‑metal, thread‑safe)
* **HTTP server** minimalista com rotas dinâmicas
* **Access Point** configurável (SSID, senha, canal)
* **Captive portal** (redirecionamento 302)

Funciona em *bare-metal* com polling (`cyw43_arch_poll()`), mas também pode ser integrado ao FreeRTOS criando uma task dedicada para rede (usar `pico_cyw43_arch_lwip_threadsafe_background()` ou port SMP para RP2040).

---

## 📂 Estrutura do repositório

```
├── CMakeLists.txt          # Configuração do Pico SDK
├── src/
│   ├── main.c              # Exemplo de uso e inicialização
│   ├── ap.h / ap.c         # Setup de Access Point (canal, SSID, senha)
│   ├── http.h / http.c     # Framework HTTP / rotas / captive portal
│   ├── websocket.h / websocket.c  # WebSocket handshake, parse e callbacks
│   ├── encrypt.h           # SHA1 & Base64 helpers
│   └── routes/
│       ├── index.h         # Página Chat
│       ├── mouse.h         # Página Mouse Tracker
│       └── status.h        # Página Uptime
└── README.md
```

---

## 🚀 Objetivo

Prover, com **poucas linhas de código**, um servidor HTTP + WebSockets no Pico W, incluindo:

1. **AP Wi‑Fi**: escolha de SSID, senha e canal.
2. **Captive Portal**: redireciona requisições “golden URLs” típicas de captive-detect.
3. **Rotas HTTP** dinâmicas (GET → handler que preenche buffer).
4. **WebSockets**: handshake, parsing de frames, callbacks de eventos e broadcast.
5. **Extras**: páginas de exemplo — chat, mouse tracker e status de uptime.

---

## 📝 Exemplo de uso (`src/main.c`)

```c
#include "pico/stdlib.h"
#include "ap.h"
#include "http.h"
#include "websocket.h"

// Páginas de exemplo
#include "routes/index.h"
#include "routes/mouse.h"
#include "routes/status.h"

int main() {
    stdio_init_all();

    // 1) Inicializa AP: SSID, senha e canal
    setup_access_point("MyPicoAP", "senha123", "pico.local");

    // 2) Registra rotas HTTP
    add_http_route("/",       create_index_response);
    add_http_route("/index",  create_index_response);
    add_http_route("/mouse",  create_mouse_response);
    add_http_route("/status", create_status_response);

    // 3) Habilita WebSocket em rotas HTTP via esquema “Upgrade: websocket”
    add_new_schema_route("websocket", websocket_schema_upgrade);

    // 4) Callbacks de eventos WS
    ws_add_on_upgrade_handler(on_upgrade);
    ws_add_on_text_handler   (on_text);
    ws_add_on_close_handler  (on_close);
    ws_add_on_ping_handler   (on_ping);
    ws_add_on_pong_handler   (on_pong);

    // 5) Sobe o servidor e entra no loop
    start_http_server();

    absolute_time_t last = get_absolute_time();
    while (true) {
        cyw43_arch_poll();

        // Envia status (uptime) a cada 1 segundo
        if (absolute_time_diff_us(last, get_absolute_time()) >= 1000000) {
            last = get_absolute_time();
            send_uptime_to_all();
        }
        sleep_ms(5);
    }
}
```

---

## 🔧 Principais blocos de código

### Access Point

```c
void setup_access_point(const char *ssid,
                        const char *password,
                        const char *hostname);
```

* **ssid**: nome da rede
* **password**: deixe `NULL` para rede aberta
* **hostname**: domínio local (ex.: `pico.local`)

### HTTP + Rotas

```c
typedef void(*route_response_handler_t)(char* buf, size_t len);

// Registra rota GET → handler(buffer, len)
void add_http_route(const char* path,
                    route_response_handler_t handler);

// Inicia o servidor HTTP na porta 80
void start_http_server(void);
```

No *handler*, preencha `buf` com cabeçalhos + body:

```c
void create_index_response(char* buf, size_t len) {
    snprintf(buf, len, HTTP_HEADER INDEX_BODY);
}
```

### Captive Portal

Monitora requisições em *golden URLs*:

```c
const char* captive_site_routes[] = {
  "GET /generate_204",
  "GET /hotspot-detect.html",
  "GET /connecttest.txt",
  "GET /redirect"
};
```

e envia *302 Found* para `http://<ap_ip>/`.

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
char *ws_get_client_ip(ws_client_tpcb wc, char *buf, size_t buflen);
char *ws_get_client_route(ws_client_tpcb wc);
```

---

## 🖥️ Rotas de exemplo

| Rota      | Página            | Descrição                                               |
| --------- | ----------------- | ------------------------------------------------------- |
| `/`       | **Chat**          | Chat simples com input e log de mensagens.              |
| `/mouse`  | **Mouse Tracker** | Envia coords do mouse para o Pico e exibe no browser.   |
| `/status` | **Uptime Status** | Atualiza tempo de execução via WebSocket em tempo real. |

---

## 🔐 Criptografia

* **Base64** (`b64.h`) por René Nyffenegger (public domain)
* **SHA1** (`tenysha1.h`) por Steve Reid (public domain)

Usadas no cálculo de `Sec-WebSocket-Accept` durante o handshake.

---

## 💾 Build & Deploy

1. Clone o repositório:

   ```bash
   git clone https://github.com/usuario/pico-w-websockets.git
   cd pico-w-websockets
   ```
2. Configure o Pico SDK (`PICO_SDK_PATH`) e crie build:

   ```bash
   mkdir build && cd build
   cmake .. -DPICO_SDK_PATH=~/pico-sdk
   make
   ```
3. Arraste o `.uf2` para a unidade *RPI-RP2* no Pico W.

---

## 🤝 Contribuições

Sinta‑se à vontade para abrir **issues**, enviar **pull requests** ou sugerir **novas rotas** e **funcionalidades**!

---

**Autores:**
– Ataniel Silva Santos Segundo
– René Nyffenegger (`b64.h`)
– Steve Reid (`tenysha1.h`)
