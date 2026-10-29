#include "MultiViewWindow.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fast {

void MultiViewWindow::addRenderer(
        int viewIndex,
        Renderer::pointer renderer) {
    if(getViews().size() == 0)
        throw Exception("You must set the nr of views before adding renderers to MultiViewWindow");
    getView(viewIndex)->addRenderer(renderer);
}

void MultiViewWindow::removeAllRenderers() {
    std::vector<View*> views = getViews();
    for(View* view : views) {
        view->removeAllRenderers();
    }
}
MultiViewWindow::MultiViewWindow(int viewCount, Color bgcolor, int width, int height, bool verticalMode, std::vector<std::string> labels) {
    setNrOfViews(viewCount);
    setBackgroundColor(bgcolor);
    if(width > 0)
        setWidth(width);
    if(height > 0)
        setHeight(height);
    if(verticalMode)
        setVerticalMode();
    m_labels = labels;
}

void MultiViewWindow::setNrOfViews(int viewCount) {
    clearViews();
    for(int i = 0; i < viewCount; i++) {
        auto view = createView();
    }
}

void MultiViewWindow::setHorizontalMode() {
    mVerticalMode = false;
    createLayout();
}

void MultiViewWindow::setVerticalMode() {
    mVerticalMode = true;
    createLayout();
}

MultiViewWindow::~MultiViewWindow() {
}

void MultiViewWindow::createLayout() {
    // Add new layout
    std::vector<View*> views = getViews();
    QBoxLayout* mainLayout;
    if(mVerticalMode) {
        mainLayout = new QVBoxLayout;
    } else {
        mainLayout = new QHBoxLayout;
    }

    for(int i = 0; i < views.size(); ++i) {
        auto view = views[i];
        if(m_labels.empty()) {
            mainLayout->addWidget(view);
        } else {
            auto viewLayout = new QVBoxLayout();
            auto label = new QLabel(QString::fromStdString("<h1><center>" + m_labels[i] + "</center></h1>"));
            viewLayout->addWidget(label);
            viewLayout->addWidget(view);
            mainLayout->addLayout(viewLayout);
        }
    }
    mainLayout->setContentsMargins(0, 0, 0, 0);
    setCenterLayout(mainLayout);
}

void MultiViewWindow::addView(View* view) {
    mThread->addView(view);
}

void MultiViewWindow::start() {
    createLayout();
    Window::start();
}

std::shared_ptr<MultiViewWindow> MultiViewWindow::connect(int viewNr, std::shared_ptr<Renderer> renderer) {
    addRenderer(viewNr, renderer);
    return std::dynamic_pointer_cast<MultiViewWindow>(mPtr.lock());
}

std::shared_ptr<MultiViewWindow>
MultiViewWindow::connect(int viewNr, std::vector<std::shared_ptr<Renderer>> renderers) {
    for(auto renderer : renderers)
        addRenderer(viewNr, renderer);
    return std::dynamic_pointer_cast<MultiViewWindow>(mPtr.lock());
}

void MultiViewWindow::setBackgroundColor(Color color) {
    for(auto view : getViews())
        view->setBackgroundColor(color);
}

std::shared_ptr<MultiViewWindow> MultiViewWindow::connect(QWidget *widget, WidgetPosition position) {
    return std::dynamic_pointer_cast<MultiViewWindow>(Window::connect(widget, position));
}

std::shared_ptr<MultiViewWindow> MultiViewWindow::connect(std::vector<QWidget *> widgets, WidgetPosition position) {
    return std::dynamic_pointer_cast<MultiViewWindow>(Window::connect(widgets, position));
}

MultiViewWindow2D::MultiViewWindow2D(int viewCount, Color bgcolor, int width, int height, bool verticalMode, std::vector<std::string> labels) : MultiViewWindow(viewCount, bgcolor, width, height, verticalMode, labels) {
    set2DMode();
}
MultiViewWindow3D::MultiViewWindow3D(int viewCount, Color bgcolor, int width, int height, bool verticalMode, std::vector<std::string> labels) : MultiViewWindow(viewCount, bgcolor, width, height, verticalMode, labels) {
    set3DMode();
}

} // end namespace fast
