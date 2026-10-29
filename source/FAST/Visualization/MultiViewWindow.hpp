#pragma once

#include <FAST/Visualization/Window.hpp>
#include <FAST/Visualization/View.hpp>
#include <FAST/Visualization/Renderer.hpp>
#include <FAST/Visualization/WindowWidget.hpp>

namespace fast {

/**
 * @brief Window with multiple views
 * @ingroup window
 */
class FAST_EXPORT MultiViewWindow : public Window {
    FAST_OBJECT_V4(MultiViewWindow)
    public:
        FAST_CONSTRUCTOR(MultiViewWindow,
                         int, viewCount,,
                         Color, bgcolor, = Color::White(),
                         int, width, = 0,
                         int, height, = 0,
                         bool, verticalMode, = false,
                         std::vector<std::string>, labels, = {}
        )
        void addView(View* view);
        void addRenderer(int viewIndex, Renderer::pointer renderer);
        void removeAllRenderers();
        void setHorizontalMode();
        void setVerticalMode();
        void setBackgroundColor(Color color);
        void setNrOfViews(int views);
        void start() override;
        ~MultiViewWindow();
        std::shared_ptr<MultiViewWindow> connect(int viewNr, std::shared_ptr<Renderer> renderer);
        std::shared_ptr<MultiViewWindow> connect(int viewNr, std::vector<std::shared_ptr<Renderer>> renderers);
        std::shared_ptr<MultiViewWindow> connect(QWidget* widget, WidgetPosition position = WidgetPosition::BOTTOM);
        std::shared_ptr<MultiViewWindow> connect(std::vector<QWidget*> widgets, WidgetPosition position = WidgetPosition::BOTTOM);
    protected:
        void createLayout();

        bool mVerticalMode = false;
        std::vector<std::string> m_labels;
};

/**
 * @brief Window with multiple 2D views
 * @ingroup window
 */
class FAST_EXPORT MultiViewWindow2D : public MultiViewWindow {
    FAST_OBJECT_V4(MultiViewWindow)
    public:
        FAST_CONSTRUCTOR(MultiViewWindow2D,
                     int, viewCount,,
                     Color, bgcolor, = Color::White(),
                     int, width, = 0,
                     int, height, = 0,
                     bool, verticalMode, = false,
                     std::vector<std::string>, labels, = {}
        )
};

/**
 * @brief Window with multiple 3D views
 * @ingroup window
 */
class FAST_EXPORT MultiViewWindow3D : public MultiViewWindow {
    FAST_OBJECT_V4(MultiViewWindow)
    public:
        FAST_CONSTRUCTOR(MultiViewWindow3D,
                         int, viewCount,,
                         Color, bgcolor, = Color::White(),
                         int, width, = 0,
                         int, height, = 0,
                         bool, verticalMode, = false,
                         std::vector<std::string>, labels, = {}
        )
};

} // end namespace fast

