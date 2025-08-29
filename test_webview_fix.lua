-- Test script to verify webview fix
-- This script helps test webview functionality before and after login

local function createTestWebView()
    local webview = g_ui.createWidget('UIWebView')
    webview:setSize({width = 400, height = 300})
    webview:setPosition({x = 50, y = 50})
    webview:setParent(modules.game_interface.getMapPanel())
    
    -- Load a simple test page
    local testHtml = [[
        <html>
        <head>
            <style>
                body { 
                    font-family: Arial, sans-serif; 
                    background: linear-gradient(45deg, #ff6b6b, #4ecdc4);
                    color: white;
                    display: flex;
                    justify-content: center;
                    align-items: center;
                    height: 100vh;
                    margin: 0;
                }
                .container {
                    text-align: center;
                    background: rgba(0,0,0,0.3);
                    padding: 20px;
                    border-radius: 10px;
                }
                button {
                    background: #fff;
                    color: #333;
                    border: none;
                    padding: 10px 20px;
                    border-radius: 5px;
                    cursor: pointer;
                    margin: 5px;
                }
            </style>
        </head>
        <body>
            <div class="container">
                <h1>WebView Test</h1>
                <p>Status: <span id="status">Working!</span></p>
                <p>Time: <span id="time"></span></p>
                <button onclick="testCallback()">Test Callback</button>
                <button onclick="changeStatus()">Change Status</button>
            </div>
            
            <script>
                function updateTime() {
                    document.getElementById('time').textContent = new Date().toLocaleTimeString();
                }
                
                function testCallback() {
                    if (window.sendToLua) {
                        window.sendToLua('test_callback', 'Hello from webview!');
                    }
                }
                
                function changeStatus() {
                    const statuses = ['Working!', 'Still working!', 'Excellent!', 'Perfect!'];
                    const current = document.getElementById('status').textContent;
                    const currentIndex = statuses.indexOf(current);
                    const nextIndex = (currentIndex + 1) % statuses.length;
                    document.getElementById('status').textContent = statuses[nextIndex];
                }
                
                // Update time every second
                setInterval(updateTime, 1000);
                updateTime();
                
                // Set up Lua callback receiver
                window.receiveFromLua = function(data) {
                    console.log('Received from Lua:', data);
                };
            </script>
        </body>
        </html>
    ]]
    
    webview:loadHtml(testHtml)
    
    -- Register callback
    webview:registerJavaScriptCallback('test_callback', function(data)
        print('WebView callback received:', data)
        modules.game_textmessage.displayEventAdvance('WebView: ' .. data, modules.game_textmessage.types.eventDefault)
    end)
    
    return webview
end

local function showWebViewStatus()
    local status = "WebView Status:\n"
    status = status .. "- Active WebViews: " .. UICefWebView.getActiveWebViewCount() .. "\n"
    status = status .. "- CEF Initialized: " .. tostring(g_cefInitialized) .. "\n"
    
    modules.game_textmessage.displayEventAdvance(status, modules.game_textmessage.types.statusDefault)
end

-- Test commands
local testWebView = nil

g_ui.importStyle('webview_test.otui')

local testWebViewWindow = g_ui.createWidget('TestWebViewWindow', modules.game_interface.getRootPanel())
testWebViewWindow:hide()

function toggleTestWebView()
    if testWebViewWindow:isVisible() then
        testWebViewWindow:hide()
        if testWebView then
            testWebView:destroy()
            testWebView = nil
        end
    else
        testWebViewWindow:show()
        if not testWebView then
            testWebView = createTestWebView()
            testWebView:setParent(testWebViewWindow)
            testWebView:setPosition({x = 10, y = 30})
            testWebView:setSize({width = 580, height = 370})
        end
    end
end

function sendTestMessage()
    if testWebView then
        testWebView:sendToJavaScript('test_message', 'Hello from Lua!')
    end
end

-- Connect to game events to test before/after login
connect(g_game, {
    onGameStart = function()
        print("Game started - testing webview functionality...")
        showWebViewStatus()
    end,
    onGameEnd = function()
        if testWebView then
            testWebView:destroy()
            testWebView = nil
        end
        if testWebViewWindow then
            testWebViewWindow:hide()
        end
    end
})

-- Console commands for testing
if not g_webviewTest then
    g_webviewTest = {}
end

g_webviewTest.toggle = toggleTestWebView
g_webviewTest.send = sendTestMessage
g_webviewTest.status = showWebViewStatus

print("WebView test loaded! Commands:")
print("- g_webviewTest.toggle() - Show/hide test webview")
print("- g_webviewTest.send() - Send test message to webview") 
print("- g_webviewTest.status() - Show webview status")