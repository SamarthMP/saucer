#pragma once
#include "webview.hpp"

#include <string>
#include <string_view>

#include <QWebChannel>
#include <QWebEngineView>
#include <QWebEngineUrlSchemeHandler>

namespace saucer
{
    //! The web_view should be the first member of the impl struct, as it should
    //! be easily accessible for modules.

    struct webview::impl
    {
        class web_page;
        class web_class;
        class url_scheme_handler;

      public:
        QWebEngineView *web_view;

      public:
        QWebEnginePage *page;
        QWebEngineProfile *profile;

      public:
        QObject *channel_obj;
        QWebChannel *web_channel;
        QWebEngineUrlSchemeHandler *scheme_handler;

      public:
        QWebEngineView *dev_view;

      public:
        bool is_ready{false};

      public:
        static const std::string inject_script;
        static constexpr std::string_view scheme_prefix = "saucer:/";
    };

    class webview::impl::web_page : public QWebEnginePage
    {
      public:
        using QWebEnginePage::QWebEnginePage;

      protected:
        void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel, const QString &, int, const QString &) override;
    };

    class webview::impl::web_class : public QObject
    {
        Q_OBJECT

      private:
        webview *m_parent;

      public:
        web_class(webview *);

      public slots:
        void on_message(const QString &);
    };

    class webview::impl::url_scheme_handler : public QWebEngineUrlSchemeHandler
    {
        webview *m_parent;

      public:
        url_scheme_handler(webview *);

      public:
        void requestStarted(QWebEngineUrlRequestJob *) override;
    };
} // namespace saucer