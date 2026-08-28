#pragma once

#include <string>
#include <QWidget>
#include <FASTExport.hpp>
#include <mutex>

class QLabel;

namespace fast {

// The destructor is causing seg faults in python after is has been used in a window
#ifdef SWIG
%nodefaultdtor InputTextWidget;
%extend InputTextWidget {
    ~InputTextWidget() {
    }
};
#endif

/**
 * @brief A input text widget
 * @ingroup widgets
 */
class FAST_EXPORT InputTextWidget : public QWidget {
    Q_OBJECT
    public:
        /**
         * @brief Create an input text widget
         * @param title Title of input text widget (can contain HTML)
         * @param text Initial text of input text widget
         * @param singleLine Whether to use a single line input text widget or a multi-line widget
         * @param parent
         */
        InputTextWidget(std::string title = "", std::string text = "", bool singleLine = true, QWidget* parent = nullptr);
        /**
         * @brief Set text
         * @param text
         */
        void setText(std::string text);
        /**
         * @brief Get current text
         * @return text
         */
        std::string getText();
#ifndef SWIG
    Q_SIGNALS:
        void updateTextSignal(QString text);
#endif
    private:
        QWidget* m_inputWidget;
        QLabel* m_label;
        bool m_singleLine = true;
        std::string m_text;
        std::string m_title;
        std::mutex m_mutex;
};


}
