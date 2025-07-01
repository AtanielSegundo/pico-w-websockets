#ifndef STATUS_H
#define STATUS_H

// Página Status modernizada: exibe uptime em tempo real via WebSocket
#define STATUS_BODY \
    "<!DOCTYPE html>" \
    "<html lang=\"pt-BR\">" \
    "<head>" \
    "  <meta charset=\"utf-8\">" \
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">" \
    "  <title>Status de Execução</title>" \
    "  <style>" \
    "    body { font-family: 'Segoe UI', Tahoma, sans-serif; background: #eef2f5; color: #333; display: flex; flex-direction: column; align-items: center; padding: 20px; margin: 0; }" \
    "    header { font-size: 1.5rem; margin-bottom: 1rem; color: #4a90e2; }" \
    "    #uptime { font-size: 2rem; margin: 20px 0; background: #fff; padding: 10px 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }" \
    "    #log { display: none; }" \
    "  </style>" \
    "</head>" \
    "<body>" \
    "  <header>⏱ Tempo de Atividade</header>" \
    "  <div id=\"uptime\">--:--:--</div>" \
    "  <script>" \
    "    const uptimeEl = document.getElementById('uptime');" \
    "    const ws = new WebSocket('ws://' + location.host + '/status');" \
    "    ws.addEventListener('open', () => console.log('[Sistema] Conectado ao WS de status'));" \
    "    ws.addEventListener('message', e => {" \
    "      const data = e.data.startsWith('status:') ? e.data.slice(7) : e.data;" \
    "      uptimeEl.textContent = data;" \
    "    });" \
    "    ws.addEventListener('close', () => console.log('[Sistema] Conexão WS encerrada'));" \
    "    ws.addEventListener('error', err => console.error('[Erro WS]', err));" \
    "  </script>" \
    "</body>" \
    "</html>"

#endif /* STATUS_H */
