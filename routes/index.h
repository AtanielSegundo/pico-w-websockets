#ifndef INDEX_H
#define INDEX_H

// Enhanced WebSocket Chat page with navigation links
#define INDEX_BODY \
    "<!DOCTYPE html>" \
    "<html lang=\"pt-BR\">" \
    "<head>" \
    "  <meta charset=\"utf-8\">" \
    "  <meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">" \
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">" \
    "  <title>WebSocket Chat</title>" \
    "  <style>" \
    "    body { font-family: 'Segoe UI', Tahoma, sans-serif; background: linear-gradient(135deg, #74ABE2 0%, #5563DE 100%); color: #fff; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; }" \
    "    #container { background: rgba(0,0,0,0.7); padding: 2rem; border-radius: 12px; box-shadow: 0 8px 16px rgba(0,0,0,0.8); width: 90%; max-width: 500px; }" \
    "    h1 { text-align: center; margin-bottom: 1rem; }" \
    "    nav { display: flex; justify-content: center; gap: 1rem; margin-bottom: 1.5rem; }" \
    "    nav a { color: #FFD700; text-decoration: none; font-weight: bold; }" \
    "    #log { height: 200px; overflow-y: auto; background: rgba(255,255,255,0.1); padding: 1rem; border-radius: 8px; margin-bottom: 1rem; }" \
    "    .chat-input { display: flex; }" \
    "    .chat-input input { flex: 1; padding: 0.75rem 1rem; border: none; border-radius: 4px 0 0 4px; outline: none; }" \
    "    .chat-input button { padding: 0.75rem 1rem; border: none; background: #FF6B6B; color: #fff; cursor: pointer; border-radius: 0 4px 4px 0; }" \
    "    .message { margin: 0.5rem 0; }" \
    "  </style>" \
    "</head>" \
    "<body>" \
    "  <div id=\"container\">" \
    "    <h1>WebSocket Chat</h1>" \
    "    <nav>" \
    "      <a href=\"/index\">Chat</a>" \
    "      <a href=\"/mouse\">Mouse</a>" \
    "      <a href=\"/status\">Status</a>" \
    "    </nav>" \
    "    <div id=\"log\"></div>" \
    "    <div class=\"chat-input\">" \
    "      <input type=\"text\" id=\"Itext\" placeholder=\"Digite sua mensagem...\" />" \
    "      <button id=\"sendBtn\">Enviar</button>" \
    "    </div>" \
    "  </div>" \
    "  <script>" \
    "    const logEl = document.getElementById('log');" \
    "    const inputEl = document.getElementById('Itext');" \
    "    const btnEl = document.getElementById('sendBtn');" \
    "    const socket = new WebSocket('ws://' + location.host);" \
    "    socket.addEventListener('open', () => appendMessage('[Sistema] Conectado'));" \
    "    socket.addEventListener('message', e => appendMessage('[Servidor] ' + e.data));" \
    "    socket.addEventListener('close', e => appendMessage('[Sistema] Desconectado (código ' + e.code + ')'));" \
    "    socket.addEventListener('error', err => appendMessage('[Erro] ' + err));" \
    "    btnEl.addEventListener('click', sendMsg);" \
    "    inputEl.addEventListener('keypress', e => { if (e.key === 'Enter') sendMsg(); });" \
    "    function sendMsg() { const msg = inputEl.value.trim(); if (msg) { socket.send(msg); appendMessage('[Você] ' + msg); inputEl.value = ''; } }" \
    "    function appendMessage(text) { const div = document.createElement('div'); div.className = 'message'; div.textContent = text; logEl.appendChild(div); logEl.scrollTop = logEl.scrollHeight; }" \
    "  </script>" \
    "</body>" \
    "</html>"

#endif /* INDEX_H */