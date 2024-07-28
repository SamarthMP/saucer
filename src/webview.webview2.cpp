#include "webview.webview2.impl.hpp"

#include "requests.hpp"
#include "instantiate.hpp"

#include "utils.win32.hpp"

#include "icon.win32.impl.hpp"
#include "window.win32.impl.hpp"

#include <ranges>
#include <filesystem>

#include <fmt/core.h>
#include <fmt/xchar.h>

#include <shlobj.h>
#include <WebView2EnvironmentOptions.h>

namespace saucer
{
    webview::webview(const options &options) : window(options), m_impl(std::make_unique<impl>())
    {
        static std::once_flag flag;
        std::call_once(flag, []() { register_scheme("saucer"); });

        auto copy = options;

        if (options.persistent_cookies && options.storage_path.empty())
        {
            copy.storage_path = fs::current_path() / ".saucer";
            SetFileAttributesW(utils::widen(copy.storage_path.string()).c_str(), FILE_ATTRIBUTE_HIDDEN);
        }

        m_impl->set_wnd_proc(window::m_impl->hwnd);
        m_impl->create_webview(this, window::m_impl->hwnd, std::move(copy));

        m_impl->web_view->get_Settings(&m_impl->settings);

        auto resource_requested = [this](auto, auto *args)
        {
            m_impl->scheme_handler(args);
            return S_OK;
        };

        m_impl->web_view->add_WebResourceRequested(Callback<ResourceRequested>(resource_requested).Get(), nullptr);

        if (!impl::gdi_token)
        {
            Gdiplus::GdiplusStartupInput input{};
            Gdiplus::GdiplusStartup(&impl::gdi_token, &input, nullptr);
        }

        auto receive_message = [this](auto, auto *args)
        {
            LPWSTR message{};
            args->TryGetWebMessageAsString(&message);

            on_message(utils::narrow(message));
            CoTaskMemFree(message);

            return S_OK;
        };

        m_impl->web_view->add_WebMessageReceived(Callback<WebMessageHandler>(receive_message).Get(), nullptr);

        auto navigation_start = [this](auto...)
        {
            m_impl->dom_loaded = false;
            m_events.at<web_event::load_started>().fire();

            return S_OK;
        };

        m_impl->web_view->add_NavigationStarting(Callback<NavigationStarting>(navigation_start).Get(), nullptr);

        if (ComPtr<ICoreWebView2_2> webview; SUCCEEDED(m_impl->web_view.As(&webview)))
        {
            auto on_loaded = [this](auto...)
            {
                m_impl->dom_loaded = true;

                for (const auto &script : m_impl->scripts)
                {
                    execute(script);
                }

                for (const auto &pending : m_impl->pending)
                {
                    execute(pending);
                }

                m_impl->pending.clear();
                m_events.at<web_event::dom_ready>().fire();

                return S_OK;
            };

            webview->add_DOMContentLoaded(Callback<DOMLoaded>(on_loaded).Get(), nullptr);
        }

        if (ComPtr<ICoreWebView2_15> webview; SUCCEEDED(m_impl->web_view.As(&webview)))
        {
            auto icon_received = [this](auto, auto *stream)
            {
                m_impl->favicon = icon{{std::make_shared<Gdiplus::Bitmap>(stream)}};
                m_events.at<web_event::icon_changed>().fire(m_impl->favicon);

                return S_OK;
            };

            auto icon_changed = [this, webview, icon_received](auto...)
            {
                webview->GetFavicon(COREWEBVIEW2_FAVICON_IMAGE_FORMAT_PNG, Callback<GetFavicon>(icon_received).Get());
                return S_OK;
            };

            webview->add_FaviconChanged(Callback<FaviconChanged>(icon_changed).Get(), nullptr);
        }

        // Ensure consistent behavior with other platforms.
        // TODO: Window-Request Event.

        auto window_request = [](auto, auto *args)
        {
            args->put_Handled(true);
            return S_OK;
        };

        m_impl->web_view->add_NewWindowRequested(Callback<NewWindowRequest>(window_request).Get(), nullptr);

        set_dev_tools(false);

        if (ComPtr<ICoreWebView2Settings3> settings; SUCCEEDED(m_impl->settings.As(&settings)))
        {
            settings->put_AreBrowserAcceleratorKeysEnabled(false);
        }

        inject(impl::inject_script(), load_time::creation);
    }

    webview::~webview() = default;

    bool webview::on_message(const std::string &message)
    {
        auto request = requests::parse(message);

        if (!request)
        {
            return false;
        }

        if (std::holds_alternative<requests::resize>(request.value()))
        {
            const auto data = std::get<requests::resize>(request.value());
            start_resize(static_cast<window_edge>(data.edge));

            return true;
        }

        if (std::holds_alternative<requests::drag>(request.value()))
        {
            start_drag();
            return true;
        }

        return false;
    }

    icon webview::favicon() const
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this] { return favicon(); });
        }

        return m_impl->favicon;
    }

    std::string webview::page_title() const
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this] { return page_title(); });
        }

        LPWSTR title{};
        m_impl->web_view->get_DocumentTitle(&title);

        auto rtn = utils::narrow(title);
        CoTaskMemFree(title);

        return rtn;
    }

    bool webview::dev_tools() const
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this] { return dev_tools(); });
        }

        BOOL rtn{false};
        m_impl->settings->get_AreDevToolsEnabled(&rtn);

        return static_cast<bool>(rtn);
    }

    std::string webview::url() const
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this] { return url(); });
        }

        LPWSTR url{};
        m_impl->web_view->get_Source(&url);

        auto rtn = utils::narrow(url);
        CoTaskMemFree(url);

        return rtn;
    }

    bool webview::context_menu() const
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this] { return context_menu(); });
        }

        BOOL rtn{false};
        m_impl->settings->get_AreDefaultContextMenusEnabled(&rtn);

        return static_cast<bool>(rtn);
    }

    void webview::set_dev_tools(bool enabled)
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this, enabled] { return set_dev_tools(enabled); });
        }

        m_impl->settings->put_AreDevToolsEnabled(enabled);

        if (!enabled)
        {
            return;
        }

        m_impl->web_view->OpenDevToolsWindow();
    }

    void webview::set_context_menu(bool enabled)
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this, enabled] { return set_context_menu(enabled); });
        }

        m_impl->settings->put_AreDefaultContextMenusEnabled(enabled);
    }

    void webview::set_file(const fs::path &file)
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this, file] { return set_file(file); });
        }

        auto path = fmt::format("file://{}", fs::canonical(file).string());
        m_impl->web_view->Navigate(utils::widen(path).c_str());
    }

    void webview::set_url(const std::string &url)
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this, url] { return set_url(url); });
        }

        m_impl->web_view->Navigate(utils::widen(url).c_str());
    }

    void webview::clear_scripts()
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this] { return clear_scripts(); });
        }

        for (const auto &script : m_impl->injected | std::views::drop(1))
        {
            m_impl->web_view->RemoveScriptToExecuteOnDocumentCreated(script.c_str());
        }

        if (!m_impl->injected.empty())
        {
            m_impl->injected.resize(1);
        }

        m_impl->scripts.clear();
    }

    void webview::execute(const std::string &java_script)
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this, java_script] { return execute(java_script); });
        }

        if (!m_impl->dom_loaded)
        {
            m_impl->pending.emplace_back(java_script);
            return;
        }

        m_impl->web_view->ExecuteScript(utils::widen(java_script).c_str(), nullptr);
    }

    void webview::inject(const std::string &java_script, const load_time &load_time)
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this, java_script, load_time] { return inject(java_script, load_time); });
        }

        if (load_time == load_time::ready)
        {
            m_impl->scripts.emplace_back(java_script);
            return;
        }

        auto callback = [this](auto, LPCWSTR id)
        {
            m_impl->injected.emplace_back(id);
            return S_OK;
        };

        m_impl->web_view->AddScriptToExecuteOnDocumentCreated(utils::widen(java_script).c_str(),
                                                              Callback<ScriptInjected>(callback).Get());
    }

    void webview::handle_scheme(const std::string &name, scheme_handler handler)
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this, name, handler = std::move(handler)]
                                             { return handle_scheme(name, handler); });
        }

        if (m_impl->schemes.contains(name))
        {
            return;
        }

        m_impl->schemes.emplace(name, std::move(handler));

        auto pattern = utils::widen(fmt::format("{}*", name));
        m_impl->web_view->AddWebResourceRequestedFilter(pattern.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    }

    void webview::remove_scheme(const std::string &name)
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this, name] { return remove_scheme(name); });
        }

        auto it = m_impl->schemes.find(name);

        if (it == m_impl->schemes.end())
        {
            return;
        }

        auto pattern = utils::widen(fmt::format("{}*", name));
        m_impl->web_view->RemoveWebResourceRequestedFilter(pattern.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

        m_impl->schemes.erase(it);
    }

    void webview::clear(web_event event)
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this, event] { return clear(event); });
        }

        switch (event)
        {
        case web_event::title_changed:
            m_impl->web_view->remove_DocumentTitleChanged(m_impl->title_token.value_or(EventRegistrationToken{}));
            m_impl->title_token.reset();
            break;
        case web_event::load_finished:
            m_impl->web_view->remove_NavigationCompleted(m_impl->load_token.value_or(EventRegistrationToken{}));
            m_impl->load_token.reset();
            break;
        case web_event::url_changed:
            m_impl->web_view->remove_SourceChanged(m_impl->source_token.value_or(EventRegistrationToken{}));
            m_impl->source_token.reset();
            break;
        default:
            break;
        };

        m_events.clear(event);
    }

    void webview::remove(web_event event, std::uint64_t id)
    {
        m_events.remove(event, id);
    }

    template <web_event Event>
    void webview::once(events::type_t<Event> callback)
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this, callback = std::move(callback)] { return once<Event>(callback); });
        }

        m_impl->setup<Event>(this);
        m_events.at<Event>().once(std::move(callback));
    }

    template <web_event Event>
    std::uint64_t webview::on(events::type_t<Event> callback)
    {
        if (!window::m_impl->is_thread_safe())
        {
            return window::m_impl->post_safe([this, callback = std::move(callback)] { return on<Event>(callback); });
        }

        m_impl->setup<Event>(this);
        return m_events.at<Event>().add(std::move(callback));
    }

    INSTANTIATE_EVENTS(webview, 6, web_event)

    void webview::register_scheme(const std::string &name)
    {
        static std::unordered_map<std::string, ComPtr<ICoreWebView2CustomSchemeRegistration>> schemes;

        ComPtr<ICoreWebView2EnvironmentOptions4> options;

        if (!SUCCEEDED(impl::env_options.As(&options)))
        {
            utils::throw_error("Failed to query ICoreWebView2EnvironmentOptions4");
        }

        static LPCWSTR allowed_origins = L"*";
        auto scheme                    = Make<CoreWebView2CustomSchemeRegistration>(utils::widen(name).c_str());

        scheme->put_TreatAsSecure(true);
        scheme->SetAllowedOrigins(1, &allowed_origins);

        schemes.emplace(name, std::move(scheme));

        auto mapped = std::views::transform(schemes, [](const auto &item) { return item.second.Get(); });
        std::vector<ICoreWebView2CustomSchemeRegistration *> raw{mapped.begin(), mapped.end()};

        if (SUCCEEDED(options->SetCustomSchemeRegistrations(static_cast<UINT32>(schemes.size()), raw.data())))
        {
            return;
        }

        utils::throw_error("Failed to register scheme(s)");
    }
} // namespace saucer
