#pragma once
#ifdef USE_CEF

#include <framework/global.h>
#include <include/cef_browser.h>

class CefInputHandler {
public:
    static void handleMousePress(CefRefPtr<CefBrowser> browser, const Point& localPos, Fw::MouseButton button);
    static void handleMouseRelease(CefRefPtr<CefBrowser> browser, const Point& localPos, Fw::MouseButton button);
    static void handleMouseMove(CefRefPtr<CefBrowser> browser, const Point& localPos, bool leave);
    static void handleMouseWheel(CefRefPtr<CefBrowser> browser, const Point& localPos, Fw::MouseWheelDirection direction);

    static void handleKeyDown(CefRefPtr<CefBrowser> browser, uchar keyCode);
    static void handleKeyPress(CefRefPtr<CefBrowser> browser, uchar keyCode);
    static void handleKeyUp(CefRefPtr<CefBrowser> browser, uchar keyCode);
    static void handleKeyText(CefRefPtr<CefBrowser> browser, const std::string& keyText);

private:
    static int getCefModifiers();
    static int translateKeyCode(uchar key);
    static std::u16string cp1252ToUtf16(const std::string& text);
};

#endif
