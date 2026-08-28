#pragma once

#include <string>
#include <QWidget>
#include <FASTExport.hpp>
#include <mutex>
#include <iostream>

class QLabel;

namespace fast {

// The destructor is causing seg faults in python after is has been used in a window
#ifdef SWIG
%nodefaultdtor InputTextWidget;
%extend InputTextWidget {
    /**
   * @brief Create an input text widget
   * @param title Title of input text widget (can contain HTML)
   * @param text Initial text of input text widget
   * @param singleLine Whether to use a single line input text widget or a multi-line widget
   * @param callback Callback for when text is changed
   * @param parent
   */
    InputTextWidget(const std::string& title = "", const std::string& text = "", bool singleLine = true, InputTextWidgetCallback* callback = nullptr, QWidget* parent = nullptr) {
        auto widget = new InputTextWidget(title, text, singleLine, nullptr, parent);
        widget->setCallbackClass(callback);
        return widget;
    }
    ~InputTextWidget() {
    }
};

%feature("director") InputTextWidgetCallback;
%pythoncode %{
_input_text_callbacks = [] # Hack to avoid callbacks being deleted
def InputTextCallback(func):
    global _input_text_callbacks
    class CB(InputTextWidgetCallback):
        def __init__(self):
            super().__init__()

        def handle(self, value):
            func(value)
    obj = CB()
    _input_text_callbacks.append(obj)
    return obj
%}
#endif


/**
 * @brief Callback class for InputTextWidget
 * Used primarily by pyFAST
 */
class InputTextWidgetCallback {
public:
    virtual void handle(const std::string& text) = 0;
    virtual ~InputTextWidgetCallback() { std::cout << "Destroying InputTextWidgetCallback" << std::endl;};
};

/**
 * @brief A input text widget
 * @ingroup widgets
 */
class FAST_EXPORT InputTextWidget : public QWidget {
    Q_OBJECT
    public:
#ifndef SWIG
        /**
         * @brief Create an input text widget
         * @param title Title of input text widget (can contain HTML)
         * @param text Initial text of input text widget
         * @param singleLine Whether to use a single line input text widget or a multi-line widget
         * @param callback Callback function for when text is changed
         * @param parent
         */
        InputTextWidget(const std::string& title = "", const std::string& text = "", bool singleLine = true, std::function<void(std::string)> callback = {}, QWidget* parent = nullptr) : QWidget(parent) {
            m_callbackFunction = callback;
            init(title, text, singleLine);
        }
#endif
        /**
         * @brief Set text
         * @param text
         */
        void setText(const std::string& text);
        /**
         * @brief Get current text
         * @return text
         */
        std::string getText();
        void setCallbackClass(InputTextWidgetCallback* callback);
#ifndef SWIG
    Q_SIGNALS:
        void updateTextSignal(const QString& text);
#endif
    private:
        void init(const std::string& title, const std::string& text, bool singleLine);
        QWidget* m_inputWidget;
        QLabel* m_label;
        bool m_singleLine = true;
        std::string m_text;
        std::string m_title;
        std::mutex m_mutex;

        std::function<void(std::string)> m_callbackFunction;
        InputTextWidgetCallback* m_callbackClass = nullptr;
};


}
