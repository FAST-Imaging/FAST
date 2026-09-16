#include "SimpleWindow.hpp"
#include <QHBoxLayout>

namespace fast {

void SimpleWindow::addRenderer(std::shared_ptr<Renderer> renderer) {
    getViews()[0]->addRenderer(renderer);
}

void SimpleWindow::removeAllRenderers() {
    getViews()[0]->removeAllRenderers();
}

SimpleWindow::~SimpleWindow() {
}

SimpleWindow::SimpleWindow(bool mode2D, Color bgcolor, uint width, uint height) {
    init();
    if(mode2D) {
        set2DMode();
    } else {
        set3DMode();
    }
    getView()->setBackgroundColor(std::move(bgcolor));
    if(width > 0)
        setWidth(width);
    if(height > 0)
        setHeight(height);
}

void SimpleWindow::init() {

    View* view = createView();
    setCenterWidget(view);

    //mWidget->setContentsMargins(0, 0, 0, 0);
}

View* SimpleWindow::getView() {
    return Window::getView(0);
}

std::shared_ptr<SimpleWindow> SimpleWindow::connect(std::shared_ptr<Renderer> renderer) {
    addRenderer(renderer);
    return std::dynamic_pointer_cast<SimpleWindow>(mPtr.lock());
}
std::shared_ptr<SimpleWindow> SimpleWindow::connect(std::vector<std::shared_ptr<Renderer>> renderers) {
    for(auto renderer : renderers)
        addRenderer(renderer);
    return std::dynamic_pointer_cast<SimpleWindow>(mPtr.lock());
}

void SimpleWindow::addWidget(QWidget *widget) {
    connect(widget);
}

std::shared_ptr<SimpleWindow> SimpleWindow::connect(QWidget* widget, WidgetPosition position) {
    return std::dynamic_pointer_cast<SimpleWindow>(Window::connect(widget, position));
}
std::shared_ptr<SimpleWindow> SimpleWindow::connect(std::vector<QWidget*> widgets, WidgetPosition position) {
    return std::dynamic_pointer_cast<SimpleWindow>(Window::connect(widgets, position));
}

SimpleWindow2D::SimpleWindow2D(Color bgcolor, uint width, uint height) : SimpleWindow(true, bgcolor, width, height) {
}

SimpleWindow3D::SimpleWindow3D(Color bgcolor, uint width, uint height) : SimpleWindow(false, bgcolor, width, height) {
}

} // end namespace fast
