#include "wk.scheme.impl.hpp"

#include "cocoa.utils.hpp"

#import <objc/objc-runtime.h>

void saucer::scheme::init_objc()
{
    static bool init = false;

    if (init)
    {
        return;
    }

    class_replaceMethod(
        [SchemeHandler class], @selector(webView:startURLSchemeTask:),
        imp_implementationWithBlock(
            [](SchemeHandler *self, WKWebView *instance, id<WKURLSchemeTask> urlSchemeTask)
            {
                const utils::autorelease_guard guard{};

                if (!self->m_handlers.contains(instance))
                {
                    return;
                }

                auto req    = scheme::request{{urlSchemeTask}};
                auto result = std::invoke(self->m_handlers[instance], req);

                if (!result.has_value())
                {
                    [urlSchemeTask didFailWithError:[NSError errorWithDomain:NSURLErrorDomain
                                                                        code:std::to_underlying(result.error())
                                                                    userInfo:nil]];

                    return;
                }

                auto content = result->data;

                auto *const data    = [NSData dataWithBytes:content.data() length:static_cast<NSInteger>(content.size())];
                auto *const headers = [[[NSMutableDictionary<NSString *, NSString *> alloc] init] autorelease];

                for (const auto &[key, value] : result->headers)
                {
                    [headers setObject:[NSString stringWithUTF8String:value.c_str()]
                                forKey:[NSString stringWithUTF8String:key.c_str()]];
                }

                auto *const mime   = [NSString stringWithUTF8String:result->mime.c_str()];
                auto *const length = [NSString stringWithFormat:@"%zu", content.size()];

                [headers setObject:mime forKey:@"Content-Type"];
                [headers setObject:length forKey:@"Content-Length"];

                auto *const response = [[[NSHTTPURLResponse alloc] initWithURL:urlSchemeTask.request.URL
                                                                    statusCode:result->status
                                                                   HTTPVersion:nil
                                                                  headerFields:headers] autorelease];

                [urlSchemeTask didReceiveResponse:response];
                [urlSchemeTask didReceiveData:data];
                [urlSchemeTask didFinish];
            }),
        "v@:@");

    init = true;
}

@implementation SchemeHandler
- (void)add_handler:(saucer::scheme::handler)handler webview:(WKWebView *)instance
{
    saucer::scheme::init_objc();
    m_handlers.emplace(instance, std::move(handler));
}

- (void)remove_handler:(WKWebView *)instance
{
    m_handlers.erase(instance);
}

- (void)webView:(nonnull WKWebView *)webView startURLSchemeTask:(nonnull id<WKURLSchemeTask>)urlSchemeTask
{
}

- (void)webView:(nonnull WKWebView *)webView stopURLSchemeTask:(nonnull id<WKURLSchemeTask>)urlSchemeTask
{
}
@end
