-- HTTP Enter Game System
-- Minimalist middleware between webview and g_game.loginWorld
-- Replaces traditional login system with HTTP authentication
HTTPEnterGame = {}

-- Login API configuration
local LOGIN_API_CONFIG = {
    baseUrl = "http://localhost:3000/auth", -- Node.js login server URL
    endpoints = {login = "/login"},
    timeout = 10000 -- 10 seconds
}

-- Private variables
local httpLoginWindow
local httpLoginWebView
local currentLoginData = {}

-- Private functions
local function onGameLoginError(message)
    print('Gameserver login error:', message)
    local errorBox = displayErrorBox(tr('Login Error'), message)
    connect(errorBox, {onOk = HTTPEnterGame.show})
end

local function onGameConnectionError(message, code)
    print('Gameserver connection error:', message, code)
    local text = translateNetworkError(code, g_game.getProtocolGame() and
                                           g_game.getProtocolGame():isConnecting(),
                                       message)
    local errorBox = displayErrorBox(tr('Connection Error'), text)
    connect(errorBox, {onOk = HTTPEnterGame.show})
end

local function onGameStart()
    print('Game started successfully!')
    -- Default system already removes loadBox automatically

    -- Send signal to JavaScript that player is online
    if httpLoginWebView then
        httpLoginWebView:sendToJavaScript('game_state_changed',
                                          json.encode({isOnline = true}))
    end
end

local function onGameEnd()
    print('Game ended')
    -- Send signal to JavaScript that player is offline
    if httpLoginWebView then
        httpLoginWebView:sendToJavaScript('game_state_changed',
                                          json.encode({isOnline = false}))
        -- Then reset state and show form
        httpLoginWebView:sendToJavaScript('reset_and_show_login', '')
    end
    HTTPEnterGame.show()
end

local function onMotd(protocol, motd)
    G.motdNumber = tonumber(motd:sub(0, motd:find("\n")))
    G.motdMessage = motd:sub(motd:find("\n") + 1, #motd)

    -- Send MOTD to webview if needed
    httpLoginWebView:sendToJavaScript('show_motd', json.encode(
                                          {
            number = G.motdNumber,
            message = G.motdMessage
        }))
end

local function onSessionKey(protocol, sessionKey) G.sessionKey = sessionKey end

local function onUpdateNeeded(protocol, signature)
    if loadBox then
        loadBox:destroy()
        loadBox = nil
    end

    local errorBox = displayErrorBox(tr('Update needed'), tr(
                                         'Your client needs updating, try redownloading it.'))
    connect(errorBox, {onOk = HTTPEnterGame.show})
end

-- Function to handle login error
local function handleLoginError(error)
    -- Show error in interface
    httpLoginWebView:sendToJavaScript('show_error', json.encode(
                                          {
            message = error.message,
            code = error.code
        }))

    -- Show error to user
    local errorBox = displayErrorBox(tr('Login Error'), error.message)
    connect(errorBox, {onOk = HTTPEnterGame.show})
end

-- Single function to connect to gameserver
local function connectToGameServer(result)
    -- Extract data from final result
    local character = result.character
    local gameToken = result.game_token -- can be nil for traditional method
    local credentials = result.credentials -- can be nil for JWT method

    -- Configure server data
    G.host = character.world_host
    G.port = character.world_port
    G.account = credentials and credentials.email or currentLoginData.account
    G.password = gameToken or (credentials and credentials.password or "")
    G.authenticatorToken = nil
    G.stayLogged = currentLoginData.stayLogged
    G.sessionKey = nil

    -- Save settings
    g_settings.set('host', G.host)
    g_settings.set('port', G.port)
    g_settings.set('client-version', g_game.getClientVersion())

    -- Check if already online
    if g_game.isOnline() then
        local errorBox = displayErrorBox(tr('Login Error'), tr(
                                             'Cannot login while already in game.'))
        connect(errorBox, {onOk = HTTPEnterGame.show})
        return
    end

    -- Configure client version if provided by API
    if character.world_version then
        g_game.setClientVersion(character.world_version)
    end

    -- Configure client version
    g_game.setCustomOs(5) -- HACK for compatibility
    g_game.setProtocolVersion(g_game.getClientProtocolVersion(
                                  g_game.getClientVersion()))
    g_game.chooseRsa(G.host)

    -- Hide HTTP login window
    HTTPEnterGame.hide()

    -- Connect to gameserver using g_game.loginWorld
    if modules.game_things.isLoaded() then
        g_game.loginWorld(G.account, G.password, -- Token or password
        character.world_name, G.host, G.port, character.name, nil, -- authenticatorToken
                          nil -- sessionKey
        )

        -- Save last used character
        g_settings.set('last-used-character', character.name)
        g_settings.set('last-used-world', character.world_name)
    else
        HTTPEnterGame.show()
    end
end

-- Configure callbacks for JavaScript communication
local function setupHTTPLoginCallbacks()
    -- Callback when JavaScript is loaded
    httpLoginWebView:registerJavaScriptCallback('js_loaded', function()
        print('HTTP login interface loaded!')

        -- Send initial configuration to JavaScript
        local config = {
            apiUrl = LOGIN_API_CONFIG.baseUrl,
            endpoints = LOGIN_API_CONFIG.endpoints,
            timeout = LOGIN_API_CONFIG.timeout
        }

        httpLoginWebView:sendToJavaScript('init_config', json.encode(config))

        -- Send current game state
        httpLoginWebView:sendToJavaScript('game_state_changed', json.encode(
                                              {isOnline = g_game.isOnline()}))

        -- Send locale information
        local localeInfo = WebViewTranslations.getLocaleInfo()
        if localeInfo then
            httpLoginWebView:sendToJavaScript('set_locale_info',
                                              json.encode(localeInfo))
        end
    end)

    -- Configure translation callbacks using wrapper
    WebViewTranslationCallbacks.setupWithLocale(httpLoginWebView)

    -- Final callback: receive login result (selected character + server data)
    httpLoginWebView:registerJavaScriptCallback('login_complete',
                                                function(rawData)
        local success, result = pcall(json.decode, rawData)
        if not success then
            print('Error decoding JSON:', result)
            return
        end

        print('Login complete! Connecting to gameserver...')

        -- Connect to gameserver with final data
        connectToGameServer(result)
    end)
end

-- Main function to start login process
function HTTPEnterGame.doLogin()
    -- Show HTTP login window
    HTTPEnterGame.show()
end

-- Function to show login window
function HTTPEnterGame.show()
    if httpLoginWindow then
        httpLoginWindow:show()
        httpLoginWindow:raise()
        httpLoginWindow:focus()

        -- Send command to JavaScript to reset state and show form
        httpLoginWebView:sendToJavaScript('reset_and_show_login', '')

        -- Send current game state
        if httpLoginWebView then
            httpLoginWebView:sendToJavaScript('game_state_changed', json.encode(
                                                  {isOnline = g_game.isOnline()}))
        end
    end
end

-- Function to hide login window
function HTTPEnterGame.hide() if httpLoginWindow then httpLoginWindow:hide() end end

-- Function to toggle login window visibility
function HTTPEnterGame.toggle()
    if httpLoginWindow then
        if httpLoginWindow:isVisible() then
            HTTPEnterGame.hide()
        else
            HTTPEnterGame.show()
        end
    end
end

-- Function to clear login data
function HTTPEnterGame.clearLoginData() currentLoginData = {} end

-- Initialization function
function HTTPEnterGame.init()
    -- Create HTTP login window
    httpLoginWindow = g_ui.displayUI('http_entergame')

    -- Get webview
    httpLoginWebView = httpLoginWindow:getChildById('loginWebView')

    if httpLoginWebView then
        -- Configure callbacks for JavaScript communication
        setupHTTPLoginCallbacks()

        -- Load login interface
        httpLoginWebView:loadUrl('otclient://webviews/entergame/entergame.html')
    end

    -- Configure game protocol callbacks
    connect(g_game, {onLoginError = onGameLoginError})
    connect(g_game, {onConnectionError = onGameConnectionError})
    connect(g_game, {onGameStart = onGameStart})
    connect(g_game, {onGameEnd = onGameEnd})

    -- Create menu button
    local enterGameButton = modules.client_topmenu.addLeftButton(
                                'httpEnterGameButton', tr('Enter Game'),
                                '/images/topbuttons/login', HTTPEnterGame.toggle)
end

-- Termination function
function HTTPEnterGame.terminate()
    -- Disconnect protocol callbacks
    disconnect(g_game, {onLoginError = onGameLoginError})
    disconnect(g_game, {onConnectionError = onGameConnectionError})
    disconnect(g_game, {onGameStart = onGameStart})
    disconnect(g_game, {onGameEnd = onGameEnd})

    -- Destroy login window
    if httpLoginWindow then
        httpLoginWindow:destroy()
        httpLoginWindow = nil
    end

    -- Clear login data
    currentLoginData = {}

    -- Remove menu button
    if modules.client_topmenu and modules.client_topmenu.removeLeftButton then
        modules.client_topmenu.removeLeftButton('httpEnterGameButton')
    end
end
