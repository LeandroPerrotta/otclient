-- WebView Translation Callbacks Wrapper
-- Wrapper to automatically configure all translation callbacks in webviews

WebViewTranslationCallbacks = {}

-- Function to configure all translation callbacks in a webview
function WebViewTranslationCallbacks.setup(webView)
    if not webView then
        print("WebViewTranslationCallbacks: webView is nil")
        return
    end

    -- Callback to translate a specific key
    webView:registerJavaScriptCallback('translate', function(rawData)
        local key = rawData
        if type(rawData) == 'string' and rawData:match('^%s*{%s*') then
            local success, decoded = pcall(json.decode, rawData)
            if success and type(decoded) == 'table' then
                key = decoded.key or rawData
            end
        end
        
        local translation = WebViewTranslations.translate(key)
        webView:sendToJavaScript('translation_result', key, translation)
    end)

    -- Callback to translate multiple keys
    webView:registerJavaScriptCallback('translate_multiple', function(keysString)
        local keys = {}
        for key in keysString:gmatch('[^,]+') do
            local cleanKey = key:trim()
            table.insert(keys, cleanKey)
        end

        local translations = WebViewTranslations.translateMultiple(keys)
        local result = {}
        for i, key in ipairs(keys) do 
            result[key] = translations[key] 
        end

        webView:sendToJavaScript('translations_result', json.encode(result))
    end)

    print("WebViewTranslationCallbacks: Callbacks configured successfully")
end

-- Function to configure callbacks and send locale information
function WebViewTranslationCallbacks.setupWithLocale(webView)
    -- Configure translation callbacks
    WebViewTranslationCallbacks.setup(webView)
    
    -- Send locale information
    local localeInfo = WebViewTranslations.getLocaleInfo()
    webView:sendToJavaScript('set_locale_info', json.encode(localeInfo))
end
