#include "cef_inputhandler.h"
#ifdef USE_CEF

#include <framework/platform/platformwindow.h>
#include <unordered_map>

int CefInputHandler::getCefModifiers()
{
    int mods = 0;

    int keyMods = g_window.getKeyboardModifiers();
    if (keyMods & Fw::KeyboardCtrlModifier)
        mods |= EVENTFLAG_CONTROL_DOWN;
    if (keyMods & Fw::KeyboardShiftModifier)
        mods |= EVENTFLAG_SHIFT_DOWN;
    if (keyMods & Fw::KeyboardAltModifier)
        mods |= EVENTFLAG_ALT_DOWN;

    if (g_window.isMouseButtonPressed(Fw::MouseLeftButton))
        mods |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    if (g_window.isMouseButtonPressed(Fw::MouseRightButton))
        mods |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    if (g_window.isMouseButtonPressed(Fw::MouseMidButton))
        mods |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;

    return mods;
}

int CefInputHandler::translateKeyCode(uchar key)
{
    static const std::unordered_map<uchar, int> vkMap = {
        {Fw::KeyBackspace, 0x08}, // VK_BACK
        {Fw::KeyTab, 0x09},       // VK_TAB
        {Fw::KeyEnter, 0x0D},     // VK_RETURN
        {Fw::KeyEscape, 0x1B},    // VK_ESCAPE
        {Fw::KeyPageUp, 0x21},    // VK_PRIOR
        {Fw::KeyPageDown, 0x22},  // VK_NEXT
        {Fw::KeyEnd, 0x23},       // VK_END
        {Fw::KeyHome, 0x24},      // VK_HOME
        {Fw::KeyLeft, 0x25},      // VK_LEFT
        {Fw::KeyUp, 0x26},        // VK_UP
        {Fw::KeyRight, 0x27},     // VK_RIGHT
        {Fw::KeyDown, 0x28},      // VK_DOWN
        {Fw::KeyInsert, 0x2D},    // VK_INSERT
        {Fw::KeyDelete, 0x2E},    // VK_DELETE
        {Fw::KeyPause, 0x13},     // VK_PAUSE
        {Fw::KeyPrintScreen, 0x2C}, // VK_SNAPSHOT
        {Fw::KeyNumLock, 0x90},   // VK_NUMLOCK
        {Fw::KeyScrollLock, 0x91},// VK_SCROLL
        {Fw::KeyCapsLock, 0x14},  // VK_CAPITAL
        {Fw::KeyCtrl, 0x11},      // VK_CONTROL
        {Fw::KeyShift, 0x10},     // VK_SHIFT
        {Fw::KeyAlt, 0x12},       // VK_MENU
        {Fw::KeyMeta, 0x5B},      // VK_LWIN
        {Fw::KeyMenu, 0x5D},      // VK_APPS
        {Fw::KeySpace, 0x20},     // VK_SPACE
        {Fw::KeyExclamation, 0x31}, // '1' key
        {Fw::KeyQuote, 0xDE},
        {Fw::KeyNumberSign, 0x33},
        {Fw::KeyDollar, 0x34},
        {Fw::KeyPercent, 0x35},
        {Fw::KeyAmpersand, 0x37},
        {Fw::KeyApostrophe, 0xDE},
        {Fw::KeyLeftParen, 0x39},
        {Fw::KeyRightParen, 0x30},
        {Fw::KeyAsterisk, 0x38},
        {Fw::KeyPlus, 0xBB},
        {Fw::KeyComma, 0xBC},
        {Fw::KeyMinus, 0xBD},
        {Fw::KeyPeriod, 0xBE},
        {Fw::KeySlash, 0xBF},
        {Fw::Key0, 0x30},
        {Fw::Key1, 0x31},
        {Fw::Key2, 0x32},
        {Fw::Key3, 0x33},
        {Fw::Key4, 0x34},
        {Fw::Key5, 0x35},
        {Fw::Key6, 0x36},
        {Fw::Key7, 0x37},
        {Fw::Key8, 0x38},
        {Fw::Key9, 0x39},
        {Fw::KeyColon, 0xBA},
        {Fw::KeySemicolon, 0xBA},
        {Fw::KeyLess, 0xBC},
        {Fw::KeyEqual, 0xBB},
        {Fw::KeyGreater, 0xBE},
        {Fw::KeyQuestion, 0xBF},
        {Fw::KeyAtSign, 0x32},
        {Fw::KeyA, 0x41}, {Fw::KeyB, 0x42}, {Fw::KeyC, 0x43}, {Fw::KeyD, 0x44},
        {Fw::KeyE, 0x45}, {Fw::KeyF, 0x46}, {Fw::KeyG, 0x47}, {Fw::KeyH, 0x48},
        {Fw::KeyI, 0x49}, {Fw::KeyJ, 0x4A}, {Fw::KeyK, 0x4B}, {Fw::KeyL, 0x4C},
        {Fw::KeyM, 0x4D}, {Fw::KeyN, 0x4E}, {Fw::KeyO, 0x4F}, {Fw::KeyP, 0x50},
        {Fw::KeyQ, 0x51}, {Fw::KeyR, 0x52}, {Fw::KeyS, 0x53}, {Fw::KeyT, 0x54},
        {Fw::KeyU, 0x55}, {Fw::KeyV, 0x56}, {Fw::KeyW, 0x57}, {Fw::KeyX, 0x58},
        {Fw::KeyY, 0x59}, {Fw::KeyZ, 0x5A},
        {Fw::KeyLeftBracket, 0xDB},
        {Fw::KeyBackslash, 0xDC},
        {Fw::KeyRightBracket, 0xDD},
        {Fw::KeyCaret, 0x36},
        {Fw::KeyUnderscore, 0xBD},
        {Fw::KeyGrave, 0xC0},
        {Fw::KeyLeftCurly, 0xDB},
        {Fw::KeyBar, 0xDC},
        {Fw::KeyRightCurly, 0xDD},
        {Fw::KeyTilde, 0xC0},
        {Fw::KeyF1, 0x70},  {Fw::KeyF2, 0x71},  {Fw::KeyF3, 0x72},
        {Fw::KeyF4, 0x73},  {Fw::KeyF5, 0x74},  {Fw::KeyF6, 0x75},
        {Fw::KeyF7, 0x76},  {Fw::KeyF8, 0x77},  {Fw::KeyF9, 0x78},
        {Fw::KeyF10, 0x79}, {Fw::KeyF11, 0x7A}, {Fw::KeyF12, 0x7B},
        {Fw::KeyNumpad0, 0x60}, {Fw::KeyNumpad1, 0x61}, {Fw::KeyNumpad2, 0x62},
        {Fw::KeyNumpad3, 0x63}, {Fw::KeyNumpad4, 0x64}, {Fw::KeyNumpad5, 0x65},
        {Fw::KeyNumpad6, 0x66}, {Fw::KeyNumpad7, 0x67}, {Fw::KeyNumpad8, 0x68},
        {Fw::KeyNumpad9, 0x69}
    };

    auto it = vkMap.find(key);
    if (it != vkMap.end())
        return it->second;
    return key;
}

std::u16string CefInputHandler::cp1252ToUtf16(const std::string& text)
{
    static const char16_t table[32] = {
        0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
        0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178
    };

    std::u16string out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
        if (ch < 0x80)
            out.push_back(static_cast<char16_t>(ch));
        else if (ch < 0xA0)
            out.push_back(table[ch - 0x80]);
        else
            out.push_back(0x00A0 + (ch - 0xA0));
    }
    return out;
}

void CefInputHandler::handleMousePress(CefRefPtr<CefBrowser> browser, const Point& localPos, Fw::MouseButton button)
{
    if (!browser)
        return;
    CefRefPtr<CefBrowserHost> host = browser->GetHost();
    if (!host)
        return;

    CefMouseEvent event;
    event.x = localPos.x;
    event.y = localPos.y;
    event.modifiers = getCefModifiers();

    CefBrowserHost::MouseButtonType btnType = MBT_LEFT;
    if (button == Fw::MouseRightButton)
        btnType = MBT_RIGHT;
    else if (button == Fw::MouseMidButton)
        btnType = MBT_MIDDLE;

    host->SendMouseClickEvent(event, btnType, false, 1);
}

void CefInputHandler::handleMouseRelease(CefRefPtr<CefBrowser> browser, const Point& localPos, Fw::MouseButton button)
{
    if (!browser)
        return;
    CefRefPtr<CefBrowserHost> host = browser->GetHost();
    if (!host)
        return;

    CefMouseEvent event;
    event.x = localPos.x;
    event.y = localPos.y;
    event.modifiers = getCefModifiers();

    CefBrowserHost::MouseButtonType btnType = MBT_LEFT;
    if (button == Fw::MouseRightButton)
        btnType = MBT_RIGHT;
    else if (button == Fw::MouseMidButton)
        btnType = MBT_MIDDLE;

    host->SendMouseClickEvent(event, btnType, true, 1);
}

void CefInputHandler::handleMouseMove(CefRefPtr<CefBrowser> browser, const Point& localPos, bool leave)
{
    if (!browser)
        return;
    CefRefPtr<CefBrowserHost> host = browser->GetHost();
    if (!host)
        return;

    CefMouseEvent event;
    event.x = localPos.x;
    event.y = localPos.y;
    event.modifiers = getCefModifiers();
    host->SendMouseMoveEvent(event, leave);
}

void CefInputHandler::handleMouseWheel(CefRefPtr<CefBrowser> browser, const Point& localPos, Fw::MouseWheelDirection direction)
{
    if (!browser)
        return;
    CefRefPtr<CefBrowserHost> host = browser->GetHost();
    if (!host)
        return;

    CefMouseEvent event;
    event.x = localPos.x;
    event.y = localPos.y;
    event.modifiers = getCefModifiers();

    int deltaY = direction == Fw::MouseWheelUp ? 120 : -120;
    host->SendMouseWheelEvent(event, 0, deltaY);
}

void CefInputHandler::handleKeyDown(CefRefPtr<CefBrowser> browser, uchar keyCode)
{
    if (!browser)
        return;
    CefRefPtr<CefBrowserHost> host = browser->GetHost();
    if (!host)
        return;

    CefKeyEvent event = {};
    event.type = KEYEVENT_RAWKEYDOWN;
    event.modifiers = getCefModifiers();
    event.windows_key_code = translateKeyCode(keyCode);
    event.native_key_code = event.windows_key_code;
    event.is_system_key = 0;
    event.character = 0;
    event.unmodified_character = 0;
    event.focus_on_editable_field = 0;
    host->SendKeyEvent(event);
}

void CefInputHandler::handleKeyPress(CefRefPtr<CefBrowser> browser, uchar keyCode)
{
    handleKeyDown(browser, keyCode);
}

void CefInputHandler::handleKeyUp(CefRefPtr<CefBrowser> browser, uchar keyCode)
{
    if (!browser)
        return;
    CefRefPtr<CefBrowserHost> host = browser->GetHost();
    if (!host)
        return;

    CefKeyEvent event = {};
    event.type = KEYEVENT_KEYUP;
    event.modifiers = getCefModifiers();
    event.windows_key_code = translateKeyCode(keyCode);
    event.native_key_code = event.windows_key_code;
    event.is_system_key = 0;
    event.character = 0;
    event.unmodified_character = 0;
    event.focus_on_editable_field = 0;
    host->SendKeyEvent(event);
}

void CefInputHandler::handleKeyText(CefRefPtr<CefBrowser> browser, const std::string& keyText)
{
    if (!browser || keyText.empty())
        return;
    CefRefPtr<CefBrowserHost> host = browser->GetHost();
    if (!host)
        return;

    std::u16string chars = cp1252ToUtf16(keyText);
    for (char16_t ch : chars) {
        CefKeyEvent event = {};
        event.type = KEYEVENT_CHAR;
        event.modifiers = getCefModifiers();
        event.character = ch;
        event.unmodified_character = ch;
        event.windows_key_code = static_cast<int>(ch);
        event.native_key_code = event.windows_key_code;
        event.is_system_key = 0;
        event.focus_on_editable_field = 0;
        host->SendKeyEvent(event);
    }
}

#endif
