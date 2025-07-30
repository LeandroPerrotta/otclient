-- Teste simples do WebView
print("🎮 Testando WebView...")

-- Criar um WebView simples
local webview = UICEFWebView.create()
print("WebView criado:", webview)

if webview then
    print("✅ WebView criado com sucesso!")
    
    -- IMPORTANTE: Definir o parent como rootWidget
    webview:setParent(rootWidget)
    webview:setVisible(true)
    webview:setEnabled(true)
    print("✅ Parent definido como rootWidget e widget visível")
    
    -- Definir tamanho (retângulo centralizado)
    local screenSize = g_window.getSize()
    local webviewWidth = 600   -- Largura do WebView
    local webviewHeight = 400  -- Altura do WebView
    webview:setSize({width = webviewWidth, height = webviewHeight})
    print("Novo tamanho:", webview:getSize())
    
    -- Posicionar centralizado
    local x = (screenSize.width - webviewWidth) / 2
    local y = (screenSize.height - webviewHeight) / 2
    webview:setPosition({x = x, y = y})
    print("Posição centralizada:", x, y)
    
    -- Forçar redraw
    webview:setBackgroundColor('#a200ca')
    print("✅ Background vermelho definido")
    
    -- Testar carregamento de HTML
    local simpleHtml = [[
        <html>
            <body style='background-color:#00ff15; color: #ffffff; font-size: 24px;'>
                <h1>TESTE</h1>
                <p>WEBVIEW FUNCIONANDO! (LUA)</p><br>
                <p>WEBVIEW FUNCIONANDO! (LUA)</p><br>
                <p>WEBVIEW FUNCIONANDO! (LUA)</p><br>
                <p>WEBVIEW FUNCIONANDO! (LUA)</p><br>
                <p>WEBVIEW FUNCIONANDO! (LUA)</p><br>
                <p>WEBVIEW FUNCIONANDO! (LUA)</p><br>
            </body>
        </html>   
    ]]
    webview:loadHtml(simpleHtml)
    print("✅ HTML carregado!")
    
    -- Verificar se ainda está visível
    print("Widget visível:", webview:isVisible())
    print("Widget habilitado:", webview:isEnabled())
    print("Parent:", webview:getParent())
    
else
    print("❌ Falha ao criar WebView")
end 