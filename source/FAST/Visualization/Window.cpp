#include "Window.hpp"
#include <QApplication>
#include <QGLPixelBuffer>
#include <QEventLoop>
#include <QScreen>
#include <QIcon>
#include <QFontDatabase>
#include <QMessageBox>
#include <QGridLayout>
#include <QOffscreenSurface>
#ifndef WIN32
#ifndef __APPLE__
#include <X11/Xlib.h>
#endif
#endif

namespace fast {

QOpenGLContext* Window::mMainGLContext = nullptr; // Lives in main thread or computation thread if it exists.
QOpenGLContext* Window::mSecondaryGLContext = nullptr; // Lives in main thread always
QOffscreenSurface* Window::m_offscreenSurface = nullptr;

class FAST_EXPORT FASTApplication : public QApplication {
public:
    FASTApplication(int &argc, char **argv) : QApplication(argc, argv) {
    }

    virtual ~FASTApplication() {
        Reporter::info() << "FASTApplication (QApplication) is destroyed" << Reporter::end();
    }

    // reimplemented from QApplication so we can throw exceptions in slots
    virtual bool notify(QObject *receiver, QEvent *event) {
        QString msg;
        try {
            return QApplication::notify(receiver, event);
        } catch(Exception &e) {
            msg = "FAST exception caught in Qt event handler " + QString(e.what());
            Reporter::error() << msg.toStdString() << Reporter::end();
			if(!Config::getVisualization())
				throw e;
        } catch(cl::Error &e) {
			msg = "OpenCL exception caught in Qt event handler " + QString(e.what()) + "(" + QString(getCLErrorString(e.err()).c_str()) + ")";
            Reporter::error() << msg.toStdString() << Reporter::end();
			if(!Config::getVisualization())
				throw e;
        } catch(std::exception &e) {
            msg = "Standard (std) exception caught in Qt event handler " + QString(e.what());
            Reporter::error() << msg.toStdString() << Reporter::end();
			if(!Config::getVisualization())
				throw e;
        }
		int ret = QMessageBox::critical(nullptr, "Error", msg);

        return false;
    }
};

Window::Window() {
    mThread = ComputationThread::create();
    mTimeout = 0;
    initializeQtApp();

    // Scaling GUI
    QFont defaultFont("Ubuntu");
	int screenWidth = getScreenWidth();
    uint windowScaling = 1;
    if(screenWidth > 2000) {
        if(screenWidth > 3000) {
            windowScaling = 2;
            // 4K screens
            Reporter::info() << "Large screen detected with width: " << screenWidth << Reporter::end();
            if(defaultFont.pointSize() <= 12)
                mGUIScalingFactor = 1.75;
        } else {
            windowScaling = 2;
            Reporter::info() << "Medium large screen detected with width: " << screenWidth << Reporter::end();
            if(defaultFont.pointSize() <= 12)
                mGUIScalingFactor = 1.5;
        }
        //Reporter::info() << "Scaling default font with factor " << mGUIScalingFactor << Reporter::end();
    }
    //defaultFont.setPointSize((int)round(defaultFont.pointSize() * mGUIScalingFactor));
    QApplication::setFont(defaultFont);

	mEventLoop = NULL;
    mWidget = new WindowWidget;
    mEventLoop = new QEventLoop(mWidget);
    mWidget->connect(mWidget, SIGNAL(widgetHasClosed()), this, SLOT(stop()));
    mWidget->setContentsMargins(0, 0, 0, 0);

    // default window size
    mWidth = 512*windowScaling;
    mHeight = 512*windowScaling;
    mFullscreen = false;
    mMaximized = false;

    QObject::connect(mThread.get(), &ComputationThread::criticalError, mWidget, [this](QString msg) {
        //std::cout << "Got critical error signal. Thread: " << std::this_thread::get_id() << std::endl;
        int ret = QMessageBox::critical(mWidget, "Error", msg);
    }, Qt::QueuedConnection); // Queued connection ensures this runs in main thread

    // Initialize main layout
    m_mainHLayout = new QHBoxLayout;
    mWidget->setLayout(m_mainHLayout);
    m_mainLeftLayout = new QVBoxLayout;
    m_mainLeftLayout->setAlignment(Qt::AlignTop);
    m_mainHLayout->addLayout(m_mainLeftLayout);
    m_mainVLayout = new QVBoxLayout;
    m_mainTopLayout = new QVBoxLayout;
    m_mainVLayout->addLayout(m_mainTopLayout);
    QHBoxLayout* dummy = new QHBoxLayout;
    m_mainVLayout->addLayout(dummy);
    m_mainBottomLayout = new QVBoxLayout;
    m_mainVLayout->addLayout(m_mainBottomLayout);
    m_mainHLayout->addLayout(m_mainVLayout);
    m_mainRightLayout = new QVBoxLayout;
    m_mainRightLayout->setAlignment(Qt::AlignTop);
    m_mainHLayout->addLayout(m_mainRightLayout);

    mWidget->setContentsMargins(0, 0, 0, 0);
    m_mainVLayout->setContentsMargins(0, 0, 0, 0);
    m_mainHLayout->setContentsMargins(0, 0, 0, 0);
    m_mainTopLayout->setContentsMargins(0, 0, 0, 0);
    m_mainBottomLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLeftLayout->setContentsMargins(0, 0, 0, 0);
    m_mainRightLayout->setContentsMargins(0, 0, 0, 0);

    m_mainHLayout->setStretch(0, 0); // stretch factors set to zero they will only get more space if no other widgets want the space
    m_mainHLayout->setStretch(1, 1);
    m_mainHLayout->setStretch(2, 0);
}

void Window::enableFullscreen() {
    mFullscreen = true;
    disableMaximized();
}

void Window::disableFullscreen() {
    mFullscreen = false;
}

void Window::enableMaximized() {
    mMaximized = true;
    disableFullscreen();
}

void Window::disableMaximized() {
    mMaximized = false;
}

void Window::setTitle(std::string title) {
    mWidget->setWindowTitle(title.c_str());
}

void Window::cleanup() {
    //delete QApplication::instance();
}

void Window::initializeQtApp() {
    // First: Tell Qt where to finds its plugins
    QCoreApplication::setLibraryPaths({ Config::getQtPluginsPath().c_str() }); // Removes need for qt.conf

    // Make sure only one QApplication is created
    if(!QApplication::instance()) {
        Reporter::info() << "Creating new QApp" << Reporter::end();
        // Create some dummy argc and argv options as QApplication requires it
        int* argc = new int[1];
        *argc = 0;

        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

        // Set default OpenGL format for all GL contexts to use:
        QSurfaceFormat qglFormat = QSurfaceFormat::defaultFormat();
        qglFormat.setVersion(3, 3);
        qglFormat.setProfile(QSurfaceFormat::CoreProfile);
        QSurfaceFormat::setDefaultFormat(qglFormat);

#if defined(WIN32) || defined(__APPLE__)
        QApplication* app = new FASTApplication(*argc,NULL);
#else
        if(XOpenDisplay(nullptr) == nullptr) {
            Reporter::warning() << "Unable to open X display. Disabling visualization." << Reporter::end();
            // Give the -platform offscreen option to Qt. This will stop Qt for trying to initiating X and open a display
            *argc = 3;
            char const* argv[3] = {"fast", "-platform", "offscreen"};
            QApplication *app = new FASTApplication(*argc, (char**)&argv);
            Config::setVisualization(false);
        } else {
            QApplication *app = new FASTApplication(*argc, NULL);
        }
#endif


        // Set default window icon
        QApplication::setWindowIcon(QIcon((Config::getDocumentationPath() + "images/fast_icon.png").c_str()));

#ifndef WIN32
        // Get rid of font warning on linux
        std::string env = Config::getDocumentationPath()+"/fonts/";
        setenv("QT_QPA_FONTDIR", env.c_str(), 1);
#endif
        // Add all fonts in fonts folder
        for(auto&& filename : getDirectoryList(join(Config::getDocumentationPath(), "fonts"))) {
            if(filename.substr(filename.size()-4) == ".ttf") {
                QFontDatabase::addApplicationFont(join(Config::getDocumentationPath(), "fonts", filename).c_str());
            }
        }
    } else {
        Reporter::info() << "QApp already exists.." << Reporter::end();
    }

     // Create computation GL context, if it doesn't exist
    if(mMainGLContext == nullptr && Config::getVisualization()) {
        Reporter::info() << "Creating new GL context for computation thread" << Reporter::end();

        // Create GL context to be shared with the CL contexts
        m_offscreenSurface = new QOffscreenSurface;
        m_offscreenSurface->create();
        if(!m_offscreenSurface->isValid())
            throw Exception("QOffscreenSurface was invalid");
        mMainGLContext = new QOpenGLContext(); // by including widget here the context becomes valid
        mMainGLContext->setShareContext(QOpenGLContext::globalShareContext());
        mMainGLContext->create();
        mSecondaryGLContext = new QOpenGLContext(); // by including widget here the context becomes valid
        mSecondaryGLContext->setShareContext(mMainGLContext);
        mSecondaryGLContext->create();
        if(!mMainGLContext->isValid()) {
            throw Exception("Qt GL context is invalid!");
        }
        if(!mSecondaryGLContext->isValid()) {
            throw Exception("Secondary Qt GL context is invalid!");
        }
    }

    // There is a bug in AMD OpenCL related to comma (,) as decimal point
    // This will change decimal point to dot (.)
    struct lconv * lc;
    lc = localeconv();
    if(strcmp(lc->decimal_point, ",") == 0) {
        //Reporter::warning() << "Your system uses comma as decimal point." << Reporter::end();
        //Reporter::warning() << "This will now be changed to dot to avoid any comma related bugs." << Reporter::end();
        setlocale(LC_NUMERIC, "C");
        // Check again to be sure
        lc = localeconv();
        if(strcmp(lc->decimal_point, ",") == 0) {
            throw Exception("Failed to convert decimal point to dot.");
        }
    }
}


void Window::stop() {
    reportInfo() << "Stop signal recieved.." << Reporter::end();
    stopComputationThread();
    if(mEventLoop != NULL)
        mEventLoop->quit();
    /*
    // Make sure event is not quit twice
    if(!mWidget->getView()->hasQuit()) {
        // This event occurs when window is closed or timeout is reached
        mWidget->getView()->quit();
    }
    */
}

View* Window::createView() {
    std::lock_guard<std::mutex> lock(m_mutex);
    View *view = new View();
    view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mWidget->installEventFilter(view); // Forward key presses and changeEvents
    mThread->addView(view);

    return view;
}

void Window::addView(View* view) {
    std::lock_guard<std::mutex> lock(m_mutex);
    mWidget->installEventFilter(view); // Forward key presses and changeEvents
    mThread->addView(view);
}

void Window::start() {

	int screenHeight = getScreenHeight();
	int screenWidth = getScreenWidth();

    if(mFullscreen) {
        mWidth = screenWidth;
        mHeight = screenHeight;
        mWidget->resize(mWidth, mHeight);
        mWidget->showFullScreen();
    } else if(mMaximized) {
        mWidth = screenWidth;
        mHeight = screenHeight;
        mWidget->resize(mWidth, mHeight);
        mWidget->showMaximized();
    } else {
        // Move window to center
        int x = (screenWidth - mWidth) / 2;
        int y = (screenHeight - mHeight) / 2;
        mWidget->move(x, y);
        mWidget->show();
        reportInfo() << "Resizing window to " << mWidth << " " << mHeight << reportEnd();
        mWidget->resize(mWidth, mHeight);
    }

    if(mTimeout > 0) {
        QTimer* timer = new QTimer(mWidget);
        timer->start(mTimeout);
        timer->setSingleShot(true);
        mWidget->connect(timer,SIGNAL(timeout()),mWidget,SLOT(close()));
    }

    for(auto view : getViews()) {
        view->reinitialize();
    }

    startComputationThread();

    mEventLoop->exec(); // This call blocks and starts rendering
}

Window::~Window() {
    // Cleanup
    reportInfo() << "Destroying window.." << Reporter::end();
    // Event loop is child of widget
    //reportInfo() << "Deleting event loop" << Reporter::end();
    //if(mEventLoop != NULL)
    //    delete mEventLoop;
    mThread->stop();
    reportInfo() << "Deleting widget" << Reporter::end();
    if(mWidget != NULL) {
        delete mWidget;
        mWidget = NULL;
    }
    reportInfo() << "Finished deleting window widget" << Reporter::end();
    reportInfo() << "Window destroyed" << Reporter::end();
}

void Window::setTimeout(unsigned int milliseconds) {
    mTimeout = milliseconds;
}

QOpenGLContext* Window::getMainGLContext() {
    if(mMainGLContext == nullptr) {
        if(!Config::getVisualization())
            throw Exception("Visualization in FAST was disabled, unable to continue.\nIf you want to run FAST with visualization on a remote server, see the wiki page\nhttps://github.com/smistad/FAST/wiki/Running-FAST-on-a-remote-server");
        initializeQtApp();
    }

    return mMainGLContext;
}

QOpenGLContext* Window::getSecondaryGLContext() {
    if(mSecondaryGLContext == nullptr) {
        if(!Config::getVisualization())
            throw Exception("Visualization in FAST was disabled, unable to continue.\nIf you want to run FAST with visualization on a remote server, see the wiki page\nhttps://github.com/smistad/FAST/wiki/Running-FAST-on-a-remote-server");
        initializeQtApp();
    }

    return mSecondaryGLContext;
}



void Window::setMainGLContext(QOpenGLContext* context) {
    mMainGLContext = context;
}

void Window::startComputationThread() {
    mThread->start();
}

void Window::stopComputationThread() {
    if(mThread != nullptr) {
        mThread->stop();
    }
}

std::vector<View*> Window::getViews() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return mThread->getViews();
}

View* Window::getView(uint i) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return mThread->getView(i);
}

void Window::setWidth(uint width) {
    mWidth = width;
}

void Window::setHeight(uint height) {
    mHeight = height;
}

void Window::setSize(uint width, uint height) {
    setWidth(width);
    setHeight(height);
}

float Window::getScalingFactor() const {
    return mGUIScalingFactor;
}

int Window::getScreenWidth() const {
	return QGuiApplication::primaryScreen()->geometry().width();
}

int Window::getScreenHeight() const {
	return QGuiApplication::primaryScreen()->geometry().height();
}

QWidget* Window::getWidget() {
    return mWidget;
}

void Window::addProcessObject(std::shared_ptr<ProcessObject> po) {
    std::lock_guard<std::mutex> lock(m_mutex);
    mThread->addProcessObject(po);
}

std::vector<std::shared_ptr<ProcessObject>> Window::getProcessObjects() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return mThread->getProcessObjects();
}

void Window::clearProcessObjects() {
    std::lock_guard<std::mutex> lock(m_mutex);
    mThread->clearProcessObjects();
}

void Window::set2DMode() {
    for(auto view : getViews()) {
        view->set2DMode();
    }
}

void Window::set3DMode() {
    for(auto view : getViews()) {
        view->set3DMode();
    }
}

void Window::run() {
    start();
}

void Window::clearViews() {
    mThread->clearViews();
}

std::shared_ptr<ComputationThread> Window::getComputationThread() {
    return mThread;
}

std::shared_ptr<Window> Window::connect(uint id, std::shared_ptr<DataObject> data) {
    // Do nothing
    return std::dynamic_pointer_cast<Window>(mPtr.lock());
}

std::shared_ptr<Window> Window::connect(uint id, std::shared_ptr<ProcessObject> PO, uint portID) {
    // Only add if renderer
    auto renderer = std::dynamic_pointer_cast<Renderer>(PO);
    if(renderer != nullptr)
        getView(id)->addRenderer(renderer);
    return std::dynamic_pointer_cast<Window>(mPtr.lock());
}

std::shared_ptr<Window> Window::connect(QWidget * widget, WidgetPosition position) {
    switch(position) {
        case WidgetPosition::BOTTOM:
            m_mainBottomLayout->addWidget(widget);
            break;
        case WidgetPosition::TOP:
            m_mainTopLayout->addWidget( widget);
            break;
        case WidgetPosition::LEFT:
            m_mainLeftLayout->addWidget(widget);
            break;
        case WidgetPosition::RIGHT:
            m_mainRightLayout->addWidget(widget);
            break;
    }
    return std::dynamic_pointer_cast<Window>(mPtr.lock());
}
std::shared_ptr<Window> Window::connect(std::vector<QWidget *> widgets, WidgetPosition position) {
    for(auto widget : widgets) {
        connect(widget, position);
    }
    return std::dynamic_pointer_cast<Window>(mPtr.lock());
}

void Window::setCenterWidget(QWidget* widget) {
    auto old = m_mainVLayout->takeAt(1);
    delete old; // TODO delete?
    m_mainVLayout->insertWidget(1, widget);
}

void Window::setCenterLayout(QLayout *layout) {
    auto old = m_mainVLayout->takeAt(1);
    delete old; // TODO delete?
    m_mainVLayout->insertLayout(1, layout);
}

QSurface *Window::getQSurface() {
    if(mMainGLContext == nullptr) {
        if(!Config::getVisualization())
            throw Exception("Visualization in FAST was disabled, unable to continue.\nIf you want to run FAST with visualization on a remote server, see the wiki page\nhttps://github.com/smistad/FAST/wiki/Running-FAST-on-a-remote-server");
        initializeQtApp();
    }
    return (QSurface*)m_offscreenSurface;
}

} // end namespace fast
