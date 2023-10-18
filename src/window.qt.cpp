#include "window.hpp"
#include "window.qt.impl.hpp"

namespace saucer
{
    window::window(const options &options) : m_impl(std::make_unique<impl>())
    {
        static QApplication *application;

        static int argc{1};
        static std::vector<char *> argv{strdup("saucer")};

        if (!application)
        {
#ifndef SAUCER_TESTS
            qputenv("QT_LOGGING_RULES", "*=false");
#endif

            if (options.hardware_acceleration)
            {
                qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
                        "--enable-oop-rasterization --enable-gpu-rasterization --enable-native-gpu-memory-buffers "
                        "--use-gl=desktop");
            }

#ifdef SAUCER_QT5
            QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

            application = new QApplication(argc, argv.data());
        }

        m_impl->window = new impl::main_window(*this);

        //? Fixes QT-Bug where Web-View will not render when background color is transparent.

        auto palette = m_impl->window->palette();
        palette.setColor(QPalette::ColorRole::Window, QColor(255, 255, 255));

        m_impl->window->setPalette(palette);
    }

    window::~window()
    {
        m_impl->window->deleteLater();
    }

    bool window::resizable() const
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([this] { return resizable(); });
        }

        return m_impl->window->maximumSize() != m_impl->window->minimumSize();
    }

    bool window::decorations() const
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([this] { return decorations(); });
        }

        return !m_impl->window->windowFlags().testFlag(Qt::FramelessWindowHint);
    }

    std::string window::title() const
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([this] { return title(); });
        }

        return m_impl->window->windowTitle().toStdString();
    }

    color window::background() const
    {
        const auto color = m_impl->window->palette().color(QPalette::ColorRole::Window);

        return {
            static_cast<std::uint8_t>(color.red()),
            static_cast<std::uint8_t>(color.green()),
            static_cast<std::uint8_t>(color.blue()),
            static_cast<std::uint8_t>(color.alpha()),
        };
    }

    bool window::always_on_top() const
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([this] { return always_on_top(); });
        }

        return m_impl->window->windowFlags().testFlag(Qt::WindowStaysOnTopHint);
    }

    std::pair<int, int> window::size() const
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([this] { return size(); });
        }

        return {m_impl->window->width(), m_impl->window->height()};
    }

    std::pair<int, int> window::max_size() const
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([this] { return max_size(); });
        }

        return {m_impl->window->maximumWidth(), m_impl->window->maximumHeight()};
    }

    std::pair<int, int> window::min_size() const
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([this] { return min_size(); });
        }

        return {m_impl->window->minimumWidth(), m_impl->window->minimumHeight()};
    }

    void window::hide()
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([this] { return hide(); });
        }

        m_impl->window->hide();
    }

    void window::show()
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([this] { return show(); });
        }

        m_impl->window->show();
    }

    void window::close()
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([this] { return close(); });
        }

        m_impl->window->close();
    }

    void window::set_resizable(bool enabled)
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([enabled, this] { return set_resizable(enabled); });
        }

        if (!enabled)
        {
            m_impl->window->setFixedSize(m_impl->window->size());
            return;
        }

        auto max_size = m_impl->max_size.value_or(QSize{QWIDGETSIZE_MAX, QWIDGETSIZE_MAX});
        m_impl->window->setMaximumSize(max_size);

        auto min_size = m_impl->min_size.value_or(QSize{0, 0});
        m_impl->window->setMinimumSize(min_size);
    }

    void window::set_decorations(bool enabled)
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([enabled, this] { return set_decorations(enabled); });
        }

        m_impl->window->setWindowFlag(Qt::FramelessWindowHint, !enabled);
    }

    void window::set_title(const std::string &title)
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([title, this] { return set_title(title); });
        }

        m_impl->window->setWindowTitle(QString::fromStdString(title));
    }

    void window::set_always_on_top(bool enabled)
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([enabled, this] { return set_always_on_top(enabled); });
        }

        m_impl->window->setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
    }

    void window::set_size(int width, int height)
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([width, height, this] { return set_size(width, height); });
        }

        m_impl->window->resize(width, height);
    }

    void window::set_max_size(int width, int height)
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([width, height, this] { return set_max_size(width, height); });
        }

        m_impl->window->setMaximumSize(width, height);
        m_impl->max_size = m_impl->window->maximumSize();
    }

    void window::set_min_size(int width, int height)
    {
        if (!m_impl->is_thread_safe())
        {
            return m_impl->post_safe([width, height, this] { return set_min_size(width, height); });
        }

        m_impl->window->setMinimumSize(width, height);
        m_impl->min_size = m_impl->window->minimumSize();
    }

    void window::set_background(const color &color)
    {
        auto palette = m_impl->window->palette();

        auto [r, g, b, a] = color;
        palette.setColor(QPalette::ColorRole::Window, {r, g, b, a});

        m_impl->window->setPalette(palette);
    }

    void window::clear(window_event event)
    {
        m_events.clear(event);
    }

    void window::remove(window_event event, std::uint64_t id)
    {
        m_events.remove(event, id);
    }

    template <>
    std::uint64_t window::on<window_event::close>(events::type_t<window_event::close> &&callback)
    {
        return m_events.at<window_event::close>().add(std::move(callback));
    }

    template <>
    std::uint64_t window::on<window_event::closed>(events::type_t<window_event::closed> &&callback)
    {
        return m_events.at<window_event::closed>().add(std::move(callback));
    }

    template <>
    std::uint64_t window::on<window_event::resize>(events::type_t<window_event::resize> &&callback)
    {
        return m_events.at<window_event::resize>().add(std::move(callback));
    }

    template <>
    void window::run<true>()
    {
        QApplication::exec();
    }

    template <>
    void window::run<false>()
    {
        QApplication::processEvents();
    }
} // namespace saucer