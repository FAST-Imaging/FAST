#pragma once

#include <QWidget>
#include <FASTExport.hpp>
#include <iostream>

class QLabel;

namespace fast {

class ComboBox;
// The destructor is causing seg faults in python after is has been used in a window
#ifdef SWIG
%nodefaultdtor OptionsWidget;
%extend OptionsWidget {
    ~OptionsWidget() {
    }
};

%feature("director") OptionsWidgetCallback;
%pythoncode %{
_options_callbacks = [] # Hack to avoid callbacks being deleted
def OptionsCallback(func):
    global _options_callbacks
    class CB(OptionsWidgetCallback):
        def __init__(self):
            super().__init__()

        def handle(self, index, value):
            func(index, value)
    obj = CB()
    _options_callbacks.append(obj)
    return obj
%}
#endif

/**
 * @brief Callback class for OptionsWidget
 * Used primarily by pyFAST
 */
class OptionsWidgetCallback {
    public:
        virtual void handle(int index, std::string value) = 0;
        virtual ~OptionsWidgetCallback() { std::cout << "Destroying OptionsWidgetCallback" << std::endl;};
};

/**
 * @brief A widget for selecting one of several options in a dropdown box
 * @ingroup widgets
 */
class FAST_EXPORT OptionsWidget : public QWidget {
    Q_OBJECT
    public:
#ifndef SWIG
        OptionsWidget(const std::vector<std::string>& options, const std::string& name = "", const std::string& placeholder = "", int selected = -1, std::function<void(int, std::string)> callback = nullptr, QWidget* parent = nullptr);
#endif
        OptionsWidget(const std::vector<std::string>& options, const std::string& name = "", const std::string& placeholder = "", int selected = -1, OptionsWidgetCallback* callback = nullptr, QWidget* parent = nullptr);
        void setSelected(int index);
        void setSelected(std::string value);
        int getSelected();
        std::string getOption(int index) const;
    private:
        void init(const std::string& name, const std::string& placeholder, const std::vector<std::string>& options, int selected);
        std::function<void(int, std::string)> m_callbackFunction;
        OptionsWidgetCallback* m_callbackClass = nullptr;

        QLabel* m_label;
        ComboBox* m_comboBox;
        std::string m_name;
        std::vector<std::string> m_options;
};


}