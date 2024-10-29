#pragma once

#include <saucer/cocoa.utils.hpp>
#include <saucer/modules/module.hpp>

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

namespace saucer
{
    struct window::impl
    {
        NSWindow *window;
    };

    struct webview::impl
    {
        utils::objc_ptr<WKWebViewConfiguration> config;
        utils::objc_ptr<WKWebView> web_view;
    };
} // namespace saucer
