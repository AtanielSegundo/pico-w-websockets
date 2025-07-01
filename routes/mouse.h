#ifndef MOUSE_H
#define MOUSE_H

#define MOUSE_BODY \
    "<!DOCTYPE html>" \
    "<html lang=\"pt-BR\">" \
    "<head>" \
    "  <meta charset=\"utf-8\">" \
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">" \
    "  <title>Mouse Tracker</title>" \
    "  <style>" \
    "    body { font-family: 'Segoe UI', Tahoma, sans-serif; background: #fafafa; color: #333; display: flex; flex-direction: column; align-items: center; padding: 20px; margin: 0; }" \
    "    header { font-size: 1.5rem; margin-bottom: 1rem; }" \
    "    #coords { font-size: 1.25rem; margin: 10px 0; }" \
    "    #log { width: 100%; max-width: 600px; height: 200px; overflow-y: auto; background: #fff; border: 1px solid #ddd; border-radius: 8px; padding: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.05); }" \
    "  </style>" \
    "</head>" \
    "<body>" \
    "  <header>Mouse Tracker → MCU</header>" \
    "  <div id=\"coords\">X: 0, Y: 0</div>" \
    "  <div id=\"log\"></div>" \
    "  <script>" \
    "    const coordsEl = document.getElementById('coords');" \
    "    const logEl = document.getElementById('log');" \
    "    const ws = new WebSocket('ws://' + location.host + '/mouse');" \
    "    ws.addEventListener('open', () => appendLog('[Sistema] WS conectado'));" \
    "    ws.addEventListener('message', e => appendLog('[MCU] ' + e.data));" \
    "    ws.addEventListener('close', () => appendLog('[Sistema] WS desconectado'));" \
    "    ws.addEventListener('error', err => appendLog('[Erro] ' + err));" \
    "    window.addEventListener('mousemove', e => {" \
    "      const x = e.clientX, y = e.clientY;" \
    "      coordsEl.textContent = `X: ${x}, Y: ${y}`;" \
    "      const msg = `${x},${y}`;" \
    "      if (ws.readyState === WebSocket.OPEN) ws.send(msg);" \
    "    });" \
    "    function appendLog(text) {" \
    "      const div = document.createElement('div');" \
    "      div.textContent = text;" \
    "      logEl.appendChild(div);" \
    "      logEl.scrollTop = logEl.scrollHeight;" \
    "    }" \
    "  </script>" \
    "</body>" \
    "</html>"

#endif /* MOUSE_H */
