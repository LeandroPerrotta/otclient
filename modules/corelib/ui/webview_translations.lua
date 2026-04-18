-- WebView Translations Manager - Simplified Version
-- System where JavaScript requests the translations it needs

WebViewTranslations = {}

-- Function to convert CP1252 to UTF-8
function WebViewTranslations.cp1252ToUtf8(text)
  if not text or type(text) ~= 'string' then
    return text
  end
  
  -- Use OTClient's native function to convert CP1252 to UTF-8
  return latin1_to_utf8(text)
end

-- Function to translate a specific key
function WebViewTranslations.translate(key)
  if not key or type(key) ~= 'string' then
    return key
  end
  
  local translation = tr(key)
  -- Convert CP1252 to UTF-8 for webview
  return WebViewTranslations.cp1252ToUtf8(translation)
end

-- Function to translate multiple keys at once
function WebViewTranslations.translateMultiple(keys)
  if not keys or type(keys) ~= 'table' then
    return {}
  end
  
  local translations = {}
  for _, key in ipairs(keys) do
    local translation = tr(key)
    -- Convert CP1252 to UTF-8 for webview
    translations[key] = WebViewTranslations.cp1252ToUtf8(translation)
  end
  
  return translations
end

-- Function to get current locale information
function WebViewTranslations.getLocaleInfo()
  if not modules.client_locales then
    return nil
  end
  
  local currentLocale = modules.client_locales.getCurrentLocale()
  if not currentLocale then
    return nil
  end
  
  return {
    name = currentLocale.name,
    languageName = currentLocale.languageName,
    charset = currentLocale.charset
  }
end
