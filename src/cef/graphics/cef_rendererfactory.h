#pragma once

#include <memory>

class UICEFWebView;
class CefRenderer;

class CefRendererFactory
{
public:
    static std::unique_ptr<CefRenderer> createRenderer(UICEFWebView& view);
};
