#pragma once

#include <FAST/Visualization/Window.hpp>
#include <FAST/Visualization/View.hpp>
#include <FAST/Visualization/Renderer.hpp>

namespace fast {

/**
 * @brief A Window with only 1 View
 * @ingroup window
 */
class FAST_EXPORT SimpleWindow : public Window {
    FAST_OBJECT(SimpleWindow)
    public:
        /**
         * Create a SimpleWindow object
         *
         * Args:
         * @param mode2D 2D or 3D mode
         * @param bgcolor Background color
         * @param width in pixels
         * @param height in pixels
         * @return shared ptr of new SimpleWindow
         */
        FAST_CONSTRUCTOR(SimpleWindow,
                         bool, mode2D, = false,
                         Color, bgcolor, = Color::White(),
                         uint, width, = 0,
                         uint, height, = 0);
        void addRenderer(std::shared_ptr<Renderer> renderer);
        void addWidget(QWidget* widget);
        void removeAllRenderers();
        View* getView();
        ~SimpleWindow();
        std::shared_ptr<SimpleWindow> connect(std::shared_ptr<Renderer> renderer);
        std::shared_ptr<SimpleWindow> connect(std::vector<std::shared_ptr<Renderer>> renderers);
        std::shared_ptr<SimpleWindow> connect(QWidget* widget, WidgetPosition position = WidgetPosition::BOTTOM);
        std::shared_ptr<SimpleWindow> connect(std::vector<QWidget*> widgets, WidgetPosition position = WidgetPosition::BOTTOM);
    protected:
        void init();

};

/**
 * @brief A Window with only 1 View in 2D mode
 * @ingroup window
 */
class FAST_EXPORT SimpleWindow2D : public SimpleWindow {
    FAST_OBJECT_V4(SimpleWindow2D)
    public:
        /**
         * Create a SimpleWindow 2D object
         *
         * Args:
         * @param bgcolor Background color
         * @param width in pixels
         * @param height in pixels
         * @return shared ptr of new SimpleWindow
         */
        FAST_CONSTRUCTOR(SimpleWindow2D, Color, bgcolor, = Color::White(), uint, width, = 0, uint, height, = 0)
};

/**
 * @brief A Window with only 1 View in 3D mode
 * @ingroup window
 */
class FAST_EXPORT SimpleWindow3D : public SimpleWindow {
    FAST_OBJECT_V4(SimpleWindow3D)
    public:
        /**
         * Create a SimpleWindow 3D object
         *
         * Args:
         * @param bgcolor Background color
         * @param width in pixels
         * @param height in pixels
         * @return shared ptr of new SimpleWindow
         */
        FAST_CONSTRUCTOR(SimpleWindow3D, Color, bgcolor, = Color::White(), uint, width, = 0, uint, height, = 0)
};
} // end namespace fast

