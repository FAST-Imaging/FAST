#pragma once

#include <FASTExport.hpp>
#include <FAST/Object.hpp>
#include <FAST/Visualization/WindowWidget.hpp>
#include <FAST/Visualization/ComputationThread.hpp>
#include <FAST/Visualization/View.hpp>
#include <vector>

class QEventLoop;
class QVBoxLayout;
class QHBoxLayout;
class QOffscreenSurface;
class QOpenGLContext;

namespace fast {

class ProcessObject;
class TimerCallbackClass;

enum class WidgetPosition {
    BOTTOM,
    TOP,
    LEFT,
    RIGHT
};

/**
 * @defgroup window Windows
 * Windows to display data using renderers.
 */

/**
 * @brief Abstract base class for windows
 * @ingroup window
 */
class FAST_EXPORT  Window : public QObject, public AttributeObject {
    Q_OBJECT
    public:
        static void initializeQtApp();
        static QOpenGLContext* getMainGLContext();
        static QOpenGLContext* getSecondaryGLContext();
        static void setMainGLContext(QOpenGLContext* context);
        /**
         * Makes the window close after a specific number of ms
         */
        virtual void setTimeout(unsigned int milliseconds);
        ~Window();
        /**
         * Starts an update loop on all renderers attached to each view in this window.
         */
        virtual void start();
        /**
         * @brief Opens window and starts pipeline on all renderers
         */
        virtual void run();
        void setWidth(uint width);
        void setHeight(uint height);
        void setSize(uint width, uint height);
        void enableMaximized();
        void disableMaximized();
        void enableFullscreen();
        void disableFullscreen();
        void setTitle(std::string);
        void clearViews();
        std::vector<View*> getViews();
        View* getView(uint i);
        void addView(View* view);
        static void cleanup();
        /**
         * Get screen width in pixels
         * @return width in pixels
         */
        int getScreenWidth() const;
        /**
         * Get screen height in pixels
         * @return height in pixels
         */
        int getScreenHeight() const;
        /**
         * Get GUI scaling factor
         * @return
         */
        float getScalingFactor() const;
        QWidget* getWidget();
        /**
         * Add a process object to be updated by the computation thread.
         */
        void addProcessObject(std::shared_ptr<ProcessObject> po);
        /**
         * Get process objects to be updated by the computation thread.
         */
        std::vector<std::shared_ptr<ProcessObject>> getProcessObjects();
        /**
         * Clear the process objects to be updated by the computation thread.
         */
        void clearProcessObjects();

        /**
         * @brief Set 2D mode for all views in this window
         */
        void set2DMode();
        /**
         * @brief Set 3D mode for all views in this window
         */
        void set3DMode();

        /**
         * @brief Set a Qt stylesheet to style the widgets in this window.
         * Uses a syntax similar to that of CSS, called Qt Style Sheets (QSS).
         * @param stylesheet Stylesheet as string
         */
        void setStyleSheet(const std::string& stylesheet);

        /**
         * @brief Load Qt stylesheet from a file to style the widgets in this window.
         * Uses a syntax similar to that of CSS, called Qt Style Sheets (QSS).
         * @param path Path to stylesheet file to load
         */
        void setStyleSheetFile(const std::string& path);

        /**
         * @brief Reset all views, i.e. reset the camera, in this Window-
         * This will call reinitialize() on all the views of this window.
         */
        void resetViews();

        virtual std::shared_ptr<Window> connect(uint id, std::shared_ptr<DataObject> data);
        virtual std::shared_ptr<Window> connect(uint id, std::shared_ptr<ProcessObject> PO, uint portID = 0);
        std::shared_ptr<Window> connect(QWidget* widget, WidgetPosition position = WidgetPosition::BOTTOM);
        std::shared_ptr<Window> connect(std::vector<QWidget*> widgets, WidgetPosition position = WidgetPosition::BOTTOM);
        std::string getNameOfClass() {
            return "Window";
        }
        /**
         * @brief Perform callback after a given time
         * @param callback Callback class to use
         * @param milliseconds Timeout in milliseconds
         */
        void timerCallback(TimerCallbackClass* callback, int milliseconds);
        static QSurface* getQSurface();
protected:
        void startComputationThread();
        void stopComputationThread();
        std::shared_ptr<ComputationThread> getComputationThread();
        Window();
        View* createView();

        WindowWidget* mWidget;
        unsigned int mWidth, mHeight;
        bool mFullscreen, mMaximized;
        unsigned int mTimeout;
        float mGUIScalingFactor = 1.0f;
        QEventLoop* mEventLoop;
        std::shared_ptr<ComputationThread> mThread;
        std::mutex m_mutex;
        QHBoxLayout* m_mainHLayout;
        QVBoxLayout* m_mainVLayout;
        QVBoxLayout* m_mainTopLayout;
        QVBoxLayout* m_mainBottomLayout;
        QVBoxLayout* m_mainLeftLayout;
        QVBoxLayout* m_mainRightLayout;
        void setCenterWidget(QWidget* widget);
        void setCenterLayout(QLayout* layout);
    private:
        static QOpenGLContext* mMainGLContext;
        static QOpenGLContext* mSecondaryGLContext;
        static QOffscreenSurface* m_offscreenSurface;
    public Q_SLOTS:
        void stop();


};

/**
 * @brief Show a message in a popup window
 *
 * @param message Message to display
 * @param title Optional title
 *
 * @ingroup window
 */
FAST_EXPORT void showMessage(const std::string& message, const std::string& title = "");

/**
 * @brief Show a notification on screen
 *
 * A notification is not something the users need to interact with, and will disappear after a timeout.
 *
 * @param message Message to display
 * @param timeout Timeout to remove notification in seconds
 *
 * @ingroup window
 */
FAST_EXPORT void showNotification(const std::string& message, float timeout = 3.0f);

#ifdef SWIG
%rename(_cpp_showFileDialog) showFileDialog;
#endif

/**
 * @brief Show a file dialog for selecting files or folders for opening or saving.
 *
 * @param files Whether to open files
 * @param folders Whether to open folders
 * @param forSaving Wehther for opening or saving
 * @param allowMultiple Whether to allow selecting multiple files when opening
 * @param message Message to show in dialog
 * @param filters File filters to use
 * @param folder Folder to show initially
 *
 * @return list of files or folders selected
 */
FAST_EXPORT std::vector<std::string> showFileDialog(bool files = true, bool folders = false, bool forSaving = false, bool allowMultiple = false, const std::string& message = "", const std::string& filters = "", const std::string& folder = "");

#ifdef SWIG
%pythoncode %{
def showFileDialog(files=True, folders=False, forSaving=False, allowMultiple=False, message='', filters='', folder=''):
    result = _cpp_showFileDialog(files, folders, forSaving, allowMultiple, message, filters, folder)
    if len(result) == 0:
        return ''
    if not allowMultiple:
        result = result[0]
    return result

showFileDialog.__doc__ = _cpp_showFileDialog.__doc__
%}
#endif

#ifdef SWIG
%feature("director") TimerCallbackClass;
%pythoncode %{
_timer_callbacks = [] # Hack to avoid callbacks being deleted
def TimerCallback(func):
    global _timer_callbacks
    class CB(TimerCallbackClass):
        def __init__(self):
            super().__init__()

        def handle(self):
            func()
    obj = CB()
    _timer_callbacks.append(obj)
    return obj
%}
#endif

/**
 * @brief Timer callback class
 * Used primarily by pyFAST
 */
class TimerCallbackClass {
public:
    virtual void handle() = 0;
    virtual ~TimerCallbackClass() { std::cout << "Destroying TimerCallbackClass" << std::endl;};
};

} // end namespace fast

