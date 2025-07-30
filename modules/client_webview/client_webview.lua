-- WebView Module for OTClient
-- Provides HTML/CSS/JavaScript support through native WebView

WebViewModule = {}

-- WebView widget class
UIWebView = {}

function UIWebView.new()
    local webview = g_ui.createWidget('UIWebView')
    return webview
end

-- Create a WebView widget
function WebViewModule.createWebView(parent)
    local webview = UIWebView.new()
    if parent then
        webview:setParent(parent)
    end
    return webview
end

-- Load HTML content into WebView
function WebViewModule.loadHtml(webview, html, baseUrl)
    if webview and webview.loadHtml then
        webview:loadHtml(html, baseUrl or "")
    end
end

-- Load URL into WebView
function WebViewModule.loadUrl(webview, url)
    if webview and webview.loadUrl then
        webview:loadUrl(url)
    end
end

-- Load HTML file into WebView
function WebViewModule.loadFile(webview, filePath)
    if webview and webview.loadFile then
        webview:loadFile(filePath)
    end
end

-- Execute JavaScript in WebView
function WebViewModule.executeJavaScript(webview, script)
    if webview and webview.executeJavaScript then
        webview:executeJavaScript(script)
    end
end

-- Register JavaScript callback
function WebViewModule.registerCallback(webview, name, callback)
    if webview and webview.registerJavaScriptCallback then
        webview:registerJavaScriptCallback(name, callback)
    end
end

-- Example usage functions
function WebViewModule.createExampleWebView()
    local webview = WebViewModule.createWebView()
    webview:setSize({width = 400, height = 300})
    webview:center()
    
    -- Load example HTML
    local html = [[
        <!DOCTYPE html>
        <html>
        <head>
            <title>OTClient WebView Example</title>
            <style>
                body { 
                    font-family: Arial, sans-serif; 
                    margin: 20px; 
                    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                    color: white;
                }
                .container {
                    max-width: 600px;
                    margin: 0 auto;
                    padding: 20px;
                    background: rgba(255,255,255,0.1);
                    border-radius: 10px;
                    backdrop-filter: blur(10px);
                }
                button {
                    background: #4CAF50;
                    border: none;
                    color: white;
                    padding: 10px 20px;
                    text-align: center;
                    text-decoration: none;
                    display: inline-block;
                    font-size: 16px;
                    margin: 4px 2px;
                    cursor: pointer;
                    border-radius: 5px;
                }
                button:hover {
                    background: #45a049;
                }
            </style>
        </head>
        <body>
            <div class="container">
                <h1>OTClient WebView Demo</h1>
                <p>This is a WebView running inside OTClient!</p>
                <button onclick="sendToOTClient('Hello from WebView!')">Send Message to OTClient</button>
                <button onclick="changeColor()">Change Background</button>
                <div id="messages"></div>
            </div>
            
            <script>
                function sendToOTClient(message) {
                    // Send message to OTClient
                    if (window.otclient) {
                        window.otclient.sendMessage(message);
                    }
                    addMessage('Sent: ' + message);
                }
                
                function changeColor() {
                    const colors = ['#667eea', '#764ba2', '#f093fb', '#f5576c', '#4facfe'];
                    const randomColor = colors[Math.floor(Math.random() * colors.length)];
                    document.body.style.background = 'linear-gradient(135deg, ' + randomColor + ' 0%, #764ba2 100%)';
                    addMessage('Changed background color');
                }
                
                function addMessage(text) {
                    const div = document.getElementById('messages');
                    const p = document.createElement('p');
                    p.textContent = new Date().toLocaleTimeString() + ': ' + text;
                    div.appendChild(p);
                }
                
                // Initialize OTClient bridge
                window.otclient = {
                    sendMessage: function(message) {
                        // This will be handled by the C++ side
                        console.log('Sending to OTClient:', message);
                    }
                };
                
                addMessage('WebView loaded successfully!');
            </script>
        </body>
        </html>
    ]]
    
    WebViewModule.loadHtml(webview, html)
    
    -- Register callback for JavaScript messages
    WebViewModule.registerCallback(webview, 'sendMessage', function(message)
        print('Message from WebView:', message)
        -- You can handle the message here
    end)
    
    return webview
end

-- Initialize the module
function WebViewModule.init()
    print('WebView module loaded')
    
    -- WebView is already available through Lua bindings
    -- No need to register it manually
end

-- Cleanup the module
function WebViewModule.terminate()
    print('WebView module terminated')
end

-- Auto-initialize when module loads
WebViewModule.init() 