-- Exemplo de uso do WebView no OTClient
-- Este arquivo demonstra como usar o WebView para criar interfaces HTML/CSS/JS

-- Função para criar um WebView de exemplo
function createExampleWebView()
    -- Criar o WebView
    local webview = WebViewModule.createWebView()
    webview:setSize({width = 600, height = 400})
    webview:center()
    
    -- HTML com CSS e JavaScript moderno
    local html = [[
        <!DOCTYPE html>
        <html lang="pt-BR">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>OTClient WebView Demo</title>
            <style>
                * {
                    margin: 0;
                    padding: 0;
                    box-sizing: border-box;
                }
                
                body {
                    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
                    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                    color: white;
                    min-height: 100vh;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                }
                
                .container {
                    background: rgba(255, 255, 255, 0.1);
                    backdrop-filter: blur(10px);
                    border-radius: 20px;
                    padding: 30px;
                    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
                    border: 1px solid rgba(255, 255, 255, 0.2);
                    max-width: 500px;
                    width: 100%;
                }
                
                h1 {
                    text-align: center;
                    margin-bottom: 20px;
                    font-size: 2.5em;
                    text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
                }
                
                .description {
                    text-align: center;
                    margin-bottom: 30px;
                    font-size: 1.1em;
                    line-height: 1.6;
                }
                
                .button-group {
                    display: flex;
                    gap: 15px;
                    flex-wrap: wrap;
                    justify-content: center;
                }
                
                .btn {
                    background: linear-gradient(45deg, #4CAF50, #45a049);
                    border: none;
                    color: white;
                    padding: 12px 24px;
                    text-align: center;
                    text-decoration: none;
                    display: inline-block;
                    font-size: 16px;
                    margin: 4px 2px;
                    cursor: pointer;
                    border-radius: 25px;
                    transition: all 0.3s ease;
                    box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2);
                }
                
                .btn:hover {
                    transform: translateY(-2px);
                    box-shadow: 0 6px 20px rgba(0, 0, 0, 0.3);
                }
                
                .btn:active {
                    transform: translateY(0);
                }
                
                .btn.secondary {
                    background: linear-gradient(45deg, #2196F3, #1976D2);
                }
                
                .btn.danger {
                    background: linear-gradient(45deg, #f44336, #d32f2f);
                }
                
                .status {
                    margin-top: 20px;
                    padding: 15px;
                    background: rgba(255, 255, 255, 0.1);
                    border-radius: 10px;
                    border-left: 4px solid #4CAF50;
                }
                
                .status.error {
                    border-left-color: #f44336;
                }
                
                .status.info {
                    border-left-color: #2196F3;
                }
                
                .counter {
                    text-align: center;
                    font-size: 2em;
                    font-weight: bold;
                    margin: 20px 0;
                    text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
                }
                
                .progress-bar {
                    width: 100%;
                    height: 20px;
                    background: rgba(255, 255, 255, 0.2);
                    border-radius: 10px;
                    overflow: hidden;
                    margin: 20px 0;
                }
                
                .progress-fill {
                    height: 100%;
                    background: linear-gradient(45deg, #4CAF50, #45a049);
                    width: 0%;
                    transition: width 0.3s ease;
                }
                
                @keyframes pulse {
                    0% { transform: scale(1); }
                    50% { transform: scale(1.05); }
                    100% { transform: scale(1); }
                }
                
                .pulse {
                    animation: pulse 2s infinite;
                }
            </style>
        </head>
        <body>
            <div class="container">
                <h1>🎮 OTClient WebView</h1>
                <p class="description">
                    Esta é uma demonstração do WebView integrado ao OTClient.
                    Você pode usar HTML, CSS e JavaScript para criar interfaces modernas!
                </p>
                
                <div class="counter" id="counter">0</div>
                
                <div class="progress-bar">
                    <div class="progress-fill" id="progress"></div>
                </div>
                
                <div class="button-group">
                    <button class="btn" onclick="incrementCounter()">➕ Incrementar</button>
                    <button class="btn secondary" onclick="decrementCounter()">➖ Decrementar</button>
                    <button class="btn" onclick="sendToOTClient('Hello from WebView!')">📤 Enviar para OTClient</button>
                    <button class="btn secondary" onclick="changeTheme()">🎨 Mudar Tema</button>
                    <button class="btn danger" onclick="showAlert()">⚠️ Alerta</button>
                </div>
                
                <div class="status" id="status">
                    <strong>Status:</strong> WebView carregado com sucesso!
                </div>
            </div>
            
            <script>
                let counter = 0;
                let progress = 0;
                const themes = [
                    'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
                    'linear-gradient(135deg, #f093fb 0%, #f5576c 100%)',
                    'linear-gradient(135deg, #4facfe 0%, #00f2fe 100%)',
                    'linear-gradient(135deg, #43e97b 0%, #38f9d7 100%)',
                    'linear-gradient(135deg, #fa709a 0%, #fee140 100%)'
                ];
                let currentTheme = 0;
                
                function incrementCounter() {
                    counter++;
                    updateCounter();
                    updateProgress();
                    updateStatus('Contador incrementado para ' + counter, 'success');
                }
                
                function decrementCounter() {
                    counter--;
                    updateCounter();
                    updateProgress();
                    updateStatus('Contador decrementado para ' + counter, 'success');
                }
                
                function updateCounter() {
                    document.getElementById('counter').textContent = counter;
                    document.getElementById('counter').classList.add('pulse');
                    setTimeout(() => {
                        document.getElementById('counter').classList.remove('pulse');
                    }, 200);
                }
                
                function updateProgress() {
                    progress = Math.max(0, Math.min(100, (counter + 10) * 5));
                    document.getElementById('progress').style.width = progress + '%';
                }
                
                function changeTheme() {
                    currentTheme = (currentTheme + 1) % themes.length;
                    document.body.style.background = themes[currentTheme];
                    updateStatus('Tema alterado!', 'info');
                }
                
                function showAlert() {
                    alert('Este é um alerta JavaScript funcionando no OTClient!');
                    updateStatus('Alerta exibido!', 'info');
                }
                
                function updateStatus(message, type) {
                    const status = document.getElementById('status');
                    status.innerHTML = '<strong>Status:</strong> ' + message;
                    status.className = 'status ' + (type || '');
                }
                
                function sendToOTClient(message) {
                    if (window.otclient && window.otclient.sendMessage) {
                        window.otclient.sendMessage(message);
                        updateStatus('Mensagem enviada para OTClient: ' + message, 'success');
                    } else {
                        updateStatus('Erro: Bridge OTClient não disponível', 'error');
                    }
                }
                
                // Inicializar OTClient bridge
                window.otclient = {
                    sendMessage: function(message) {
                        console.log('Sending to OTClient:', message);
                        // Esta função será chamada pelo C++ side
                    }
                };
                
                // Inicializar
                updateStatus('WebView inicializado com sucesso!', 'success');
                updateProgress();
                
                // Simular carregamento
                setTimeout(() => {
                    updateStatus('WebView totalmente carregado e pronto!', 'success');
                }, 1000);
            </script>
        </body>
        </html>
    ]]
    
    -- Carregar o HTML
    WebViewModule.loadHtml(webview, html)
    
    -- Registrar callbacks para comunicação JavaScript -> Lua
    WebViewModule.registerCallback(webview, 'sendMessage', function(message)
        print('📤 Mensagem do WebView:', message)
        
        -- Exemplo de resposta do Lua para JavaScript
        WebViewModule.executeJavaScript(webview, [[
            updateStatus('Resposta do OTClient: ]] .. message .. [[', 'info');
        ]])
    end)
    
    return webview
end

-- Função para criar um WebView simples
function createSimpleWebView()
    local webview = WebViewModule.createWebView()
    webview:setSize({width = 400, height = 300})
    webview:center()
    
    local simpleHtml = [[
        <html>
        <head>
            <style>
                body { 
                    font-family: Arial, sans-serif; 
                    margin: 20px; 
                    background: #f0f0f0;
                }
                .box {
                    background: white;
                    padding: 20px;
                    border-radius: 10px;
                    box-shadow: 0 2px 10px rgba(0,0,0,0.1);
                }
                button {
                    background: #007bff;
                    color: white;
                    border: none;
                    padding: 10px 20px;
                    border-radius: 5px;
                    cursor: pointer;
                }
                button:hover {
                    background: #0056b3;
                }
            </style>
        </head>
        <body>
            <div class="box">
                <h2>WebView Simples</h2>
                <p>Este é um exemplo básico de WebView no OTClient.</p>
                <button onclick="alert('Funcionando!')">Clique aqui</button>
            </div>
        </body>
        </html>
    ]]
    
    WebViewModule.loadHtml(webview, simpleHtml)
    return webview
end

-- Função para carregar uma URL externa
function createUrlWebView(url)
    local webview = WebViewModule.createWebView()
    webview:setSize({width = 800, height = 600})
    webview:center()
    
    WebViewModule.loadUrl(webview, url)
    return webview
end

-- Função para criar um WebView com arquivo HTML local
function createFileWebView(filePath)
    local webview = WebViewModule.createWebView()
    webview:setSize({width = 500, height = 400})
    webview:center()
    
    WebViewModule.loadFile(webview, filePath)
    return webview
end

-- Exemplo de uso
print('🎮 WebView Module carregado!')
print('📝 Use createExampleWebView() para criar um WebView de demonstração')
print('📝 Use createSimpleWebView() para criar um WebView simples')
print('📝 Use createUrlWebView(url) para carregar uma URL')
print('📝 Use createFileWebView(path) para carregar um arquivo HTML') 