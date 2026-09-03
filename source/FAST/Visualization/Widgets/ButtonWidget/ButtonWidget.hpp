#pragma once

#include <string>
#include <QWidget>
#include <FASTExport.hpp>

class QLabel;
class QPushButton;

namespace fast {


// The destructor is causing seg faults in python after is has been used in a window
#ifdef SWIG
%nodefaultdtor ButtonWidget;
%extend ButtonWidget {
    ~ButtonWidget() {
    }
};

%feature("director") ButtonWidgetCallback;

%pythoncode %{
_button_callbacks = [] # Hack to avoid callbacks being deleted
def ButtonCallback(func):
    class CB(ButtonWidgetCallback):
        def __init__(self):
            super().__init__()

        def handle(self, value):
            func(value)

    obj = CB()
    _slider_callbacks.append(obj)
    return obj
%}
#endif

/**
 * @brief Callback class for ButtonWidget
 * Used primarily by pyFAST
 */
class ButtonWidgetCallback {
    public:
        virtual void handle(bool checked) = 0;
        virtual ~ButtonWidgetCallback() {};
};

/**
 * @brief A button widget
 * @ingroup widgets
 */
class FAST_EXPORT ButtonWidget : public QWidget {
    Q_OBJECT
    public:
#ifndef SWIG
        ButtonWidget(std::string text, bool checkable, std::function<void(bool)> callback, bool checked = false, QWidget* parent = nullptr);
        void clicked(bool check);
#endif
        ButtonWidget(std::string text, bool checkable, ButtonWidgetCallback* callback, bool checked = false, QWidget* parent = nullptr);
        void setChecked(bool checked);
        bool getChecked() const;
        std::string getText() const;
        void setText(const std::string& text);
        void setCallbackClass(ButtonWidgetCallback* callback);
#ifndef SWIG
    Q_SIGNALS:
        void updateText(const QString& str);
#endif
    private:
        void init(const std::string& text, bool checkable, bool checked);
        std::function<void(float)> m_callbackFunction;
        ButtonWidgetCallback* m_callbackClass = nullptr;

        QPushButton* m_button;
};


}