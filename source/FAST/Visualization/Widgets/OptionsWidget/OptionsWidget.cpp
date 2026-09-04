#include "OptionsWidget.hpp"
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <FAST/Visualization/Window.hpp>
#include <QStylePainter>
#include <utility>

namespace fast {

class ComboBox : public QComboBox {
    protected:
        void paintEvent(QPaintEvent *e) override {
            if(placeholderText().isEmpty()) {
                // If placeholder is empty just use QComboBox paint
                QComboBox::paintEvent(e);
            } else {
                // Override paintEvent for QComboBox due to a bug not show placeholder text
                auto painter = new QStylePainter(this);
                painter->setPen(palette().color(QPalette::Text));
                QStyleOptionComboBox opt;
                initStyleOption(&opt);
                painter->drawComplexControl(QStyle::CC_ComboBox, opt);
                if(currentIndex() < 0) { // Invalid index
                    opt.palette.setBrush(QPalette::ButtonText,opt.palette.brush(QPalette::ButtonText).color().lighter());
                    opt.currentText = placeholderText();
                }
                painter->drawControl(QStyle::CE_ComboBoxLabel, opt);
                painter->end();
            }
        };
};
OptionsWidget::OptionsWidget(const std::vector<std::string> &options, const std::string &name, const std::string& placeholder, int selected,
                           OptionsWidgetCallback *callback, QWidget *parent) : QWidget(parent) {
    init(name, placeholder, options, selected);
    m_callbackClass = callback;
}

OptionsWidget::OptionsWidget(const std::vector<std::string> &options, const std::string &name, const std::string& placeholder, int selected,
                           std::function<void(int, std::string)> callback, QWidget *parent) : QWidget(parent) {
    init(name, placeholder, options, selected);
    m_callbackFunction = std::move(callback);
}

void OptionsWidget::init(const std::string& name, const std::string& placeholder, const std::vector<std::string>& options, int selected) {
    m_name = name;
    m_options = options;
    auto layout = new QVBoxLayout();
    setLayout(layout);
    if(!name.empty()) {
        m_label = new QLabel();
        m_label->setText(QString::fromStdString(name));
        layout->addWidget(m_label);
    }
    m_comboBox = new ComboBox();
    layout->addWidget(m_comboBox);
    if(!placeholder.empty()) {
        m_comboBox->setPlaceholderText(QString::fromStdString(placeholder));
    }
    for(auto& option : options)
        m_comboBox->addItem(QString::fromStdString(option));
    if(!placeholder.empty()) {
        setSelected(selected);
    } else {
        if(selected < 0) {
            setSelected(0);
        } else {
            setSelected(selected);
        }
    }
    QObject::connect(m_comboBox, &QComboBox::currentTextChanged, [=](const QString& value) {
        auto text = value.toStdString();
        auto index = m_comboBox->findText(value);
        if(m_callbackClass != nullptr) {
            m_callbackClass->handle(index, text);
        } else {
            m_callbackFunction(index, text);
        }
    });
}

void OptionsWidget::setSelected(std::string value) {
    // TODO Thread safety?
    m_comboBox->setCurrentText(QString::fromStdString(value));
}

void OptionsWidget::setSelected(int index) {
    // TODO Thread safety?
    m_comboBox->setCurrentIndex(index);
}

int OptionsWidget::getSelected() {
    return m_comboBox->currentIndex();
}

std::string OptionsWidget::getOption(int index) const {
    if(index < 0 || index >= m_options.size())
        throw Exception("Index out of range in OptionsWidget::getOption()");
    return m_options.at(index);
}

}