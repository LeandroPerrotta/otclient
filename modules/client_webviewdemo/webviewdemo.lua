local webViewWindow
local webViewButton
local webView

local html = [[
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8"/>
  <style>
    body { font-family: Arial, sans-serif; background:#f2f2f2; margin:20px; }
    h1 { color:#0080ff; }
    button { padding:6px 12px; margin-top:10px; }
  </style>
</head>
<body>
  <h1>Demo WebView</h1>
  <p>This is a simple demonstration of the <strong>UICEFWebView</strong> component.</p>
  <button onclick="alert('Hello from WebView!')">Click me</button>
</body>
</html>
]]

function init()
  webViewWindow = g_ui.displayUI('webviewdemo')
  webViewWindow:hide()
  webView = webViewWindow:getChildById('demoWebView')
  if webView then
    webView:loadHtml(html)
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