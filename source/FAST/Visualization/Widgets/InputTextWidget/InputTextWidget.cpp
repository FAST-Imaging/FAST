#include "InputTextWidget.hpp"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>

namespace fast {

void InputTextWidget::setText(const std::string& text) {
    emit updateTextSignal(QString(text.c_str()));
    std::lock_guard<std::mutex> lock(m_mutex);
    m_text = text;
}

std::string InputTextWidget::getText() {
    std::string text;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        text = m_text;
    }

    return text;
}

void InputTextWidget::init(const std::string& title, const std::string& text, bool singleLine) {
    m_singleLine = singleLine;
    m_text = text;
    m_title = title;
    // Use QueuedConnection to assure slot is called in main thread
    if(singleLine) {
        m_inputWidget = new QLineEdit(text.c_str());
        // When text has changed in FAST, update GUI
        connect(this, &InputTextWidget::updateTextSignal, (QLineEdit*)m_inputWidget, &QLineEdit::setText, Qt::QueuedConnection);
        // When text has changed in GUI, update the FAST string, and do callback
        connect((QLineEdit*)m_inputWidget, &QLineEdit::textChanged, [=](const QString& str) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_text = str.toStdString();
            }
            if(m_callbackClass != nullptr) {
                m_callbackClass->handle(str.toStdString());
            } else if(m_callbackFunction) {
                m_callbackFunction(str.toStdString());
            }
        });
    } else {
        m_inputWidget = new QPlainTextEdit(text.c_str());
        ((QPlainTextEdit*)m_inputWidget)->moveCursor(QTextCursor::End);
        // When text has changed in FAST, update GUI
        connect(this, &InputTextWidget::updateTextSignal, this, [=](const QString& str) {
            auto widget = (QPlainTextEdit*)m_inputWidget;
            widget->setPlainText(str);
            widget->moveCursor(QTextCursor::End);
        }, Qt::QueuedConnection);
        // When text has changed in GUI, update the FAST string, and do callback
        connect((QPlainTextEdit*)m_inputWidget, &QPlainTextEdit::textChanged, [=]() {
            QString str =((QPlainTextEdit*)m_inputWidget)->toPlainText();
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_text = str.toStdString();
            }
            if(m_callbackClass != nullptr) {
                m_callbackClass->handle(str.toStdString());
            } else if(m_callbackFunction) {
                m_callbackFunction(str.toStdString());
            }
        });
    }
    auto layout = new QVBoxLayout;
    if(!m_title.empty()) {
        m_label = new QLabel(m_title.c_str());
        layout->addWidget(m_label);
    }
    layout->addWidget(m_inputWidget);
    setLayout(layout);
}

void InputTextWidget::setCallbackClass(InputTextWidgetCallback* callback) {
    m_callbackClass = callback;
}

}
