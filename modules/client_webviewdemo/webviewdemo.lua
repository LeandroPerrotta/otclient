local webViewWindow
local webViewButton
local webView
local browserWebView
local urlEdit
local webViewTabBar

function init()
  webViewWindow = g_ui.displayUI('webviewdemo')
  webViewWindow:hide()

  webViewTabBar = webViewWindow:getChildById('webViewTabBar')
  webViewTabBar:setContentWidget(webViewWindow:getChildById('webViewTabContent'))

  local demoPanel = g_ui.loadUI('demo')
  webView = demoPanel:getChildById('demoWebView')
  if webView then
    local path = g_resources.guessFilePath('client_webviewdemo/demo/demo', 'html')
    local resolvePath = g_resources.resolvePath('demo')
    local realPath = g_resources.getRealPath(resolvePath)
    print('path: ' .. path, 'real path: ' .. realPath, 'resolve path: ' .. resolvePath)
    
    -- Verificar se o arquivo existe
    if g_resources.fileExists(path) then
        print('file exists in PhysFS')
    else
        print('file does not exist in PhysFS')
    end
    
    -- Fallback: construir o path manualmente se getRealPath não funcionar
    if not realPath or realPath == "" or realPath == "/" .. path then
        local workDir = g_resources.getWorkDir()
        realPath = workDir .. "modules/" .. path
        print('fallback path: ' .. realPath)
    end
    webView:registerJavaScriptCallback('greet', function(raw)
      print('JS says:', raw)
    end)
    webView:registerJavaScriptCallback('js_loaded', function()
      webView:sendToJavaScript('greet', 'Hello from Lua!')
    end)
    webView:loadUrl('otclient://webviews/demo/demo.html')
  end
  webViewTabBar:addTab(tr('Demo'), demoPanel)

  local browserPanel = g_ui.loadUI('browser')

  webViewTabBar:addTab(tr('Browser'), browserPanel)

  --browserWebView = 
  urlEdit = browserPanel:getChildById('urlEdit')
  local loadButton = browserPanel:getChildById('loadButton')
  if loadButton then

    loadButton.onClick = function()
      local url = urlEdit:getText()
      
      if url and url ~= '' then

        if(not browserWebView) then
            browserWebView = browserPanel:getChildById('browserWebView')
        end
        
        browserWebView:loadUrl(url)
      end
    end
  else
    print("Load button not found")
  end
  if urlEdit then
    urlEdit.onReturnPressed = function() loadButton.onClick() end
  end

  webViewButton = modules.client_topmenu.addLeftButton('webViewDemoButton', tr('WebView Demo'), '/images/topbuttons/terminal', toggle)
end

function terminate()
  webViewWindow:destroy()
  webViewButton:destroy()
end

function toggle()
  if webViewWindow:isVisible() then
    webViewWindow:hide()
  else
    webViewWindow:show()
    webViewWindow:raise()
    webViewWindow:focus()
  end
end