#include "InputTextWidget.hpp"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>

namespace fast {

InputTextWidget::InputTextWidget(std::string title, std::string text, bool singleLine, QWidget *parent) : QWidget(parent) {
    m_singleLine = singleLine;
    m_text = text;
    m_title = title;
    // Use QueuedConnection to assure slot is called in main thread
    if(singleLine) {
        m_inputWidget = new QLineEdit(text.c_str());
        connect(this, &InputTextWidget::updateTextSignal, (QLineEdit*)m_inputWidget, &QLineEdit::setText, Qt::QueuedConnection);
        connect((QLineEdit*)m_inputWidget, &QLineEdit::textChanged, [=](QString str) { setText(str.toStdString()); });
    } else {
        m_inputWidget = new QPlainTextEdit(text.c_str());
        connect(this, &InputTextWidget::updateTextSignal, (QPlainTextEdit*)m_inputWidget, &QPlainTextEdit::setPlainText, Qt::QueuedConnection);
        connect((QPlainTextEdit*)m_inputWidget, &QPlainTextEdit::textChanged, [=]() { setText(((QPlainTextEdit*)m_inputWidget)->toPlainText().toStdString()); });
    }
    auto layout = new QVBoxLayout;
    if(!m_title.empty()) {
        m_label = new QLabel(m_title.c_str());
        layout->addWidget(m_label);
    }
    layout->addWidget(m_inputWidget);
    setLayout(layout);
}

void InputTextWidget::setText(std::string text) {
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

}
