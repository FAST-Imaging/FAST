#include "ButtonWidget.hpp"
#include <QHBoxLayout>
#include <QPushButton>
#include <sstream>
#include <iomanip>

namespace fast {

void ButtonWidget::init(const std::string& text, bool checkable, bool checked) {
    auto layout = new QHBoxLayout();
    setLayout(layout);
    m_button = new QPushButton();
    m_button->setText(QString::fromStdString(text));
    connect(this, &ButtonWidget::updateText, this, [=](const QString& str) {
        m_button->setText(str);
    }, Qt::QueuedConnection);
    m_button->setCheckable(checkable);
    if(checkable)
        m_button->setChecked(checked);
    layout->addWidget(m_button);
    QObject::connect(m_button, &QPushButton::clicked, [=](bool checked) {
        clicked(checked);
    });
}

void ButtonWidget::clicked(bool checked) {
    m_button->setChecked(checked);
    if(m_callbackClass != nullptr) {
        m_callbackClass->handle(checked);
    } else {
        m_callbackFunction(checked);
    }
}

ButtonWidget::ButtonWidget(std::string text, bool checkable,
                           std::function<void(bool)> callback, bool checked, QWidget *parent) : QWidget(parent) {
    m_callbackFunction = callback;
    init(text, checkable, checked);
}

ButtonWidget::ButtonWidget(std::string text, bool checkable,
                           ButtonWidgetCallback *callback, bool checked, QWidget *parent) {
    m_callbackClass = callback;
    init(text, checkable, checked);
}

std::string ButtonWidget::getText() const {
    return m_button->text().toStdString();
}

bool ButtonWidget::getChecked() const {
    return m_button->isChecked();
}

void ButtonWidget::setChecked(bool checked) {
    m_button->setChecked(checked);
}

void ButtonWidget::setCallbackClass(ButtonWidgetCallback *callback) {
    m_callbackClass = callback;
}

void ButtonWidget::setText(const std::string& text) {
    emit updateText(QString::fromStdString(text));
}

}