#include "TableWidget.hpp"
#include <QTableWidget>
#include <QVBoxLayout>
#include <FAST/Exception.hpp>
#include <QThread>
#include <QEventLoop>
#include <FAST/Utility.hpp>
#include <QCloseEvent>

namespace fast {

void TableWidget::init(const TableData& data) {
    if(data.size() > 0) {
        int columns = data.size();
        int rows = data[0].second.size();
        QStringList labels;
        for(const auto& item : data) {
            if(item.second.size() != rows) {
                throw Exception("Inconsistent data given to TableWidget");
            }
            labels.append(QString::fromStdString(item.first));
        }
        m_tableWidget = new QTableWidget(rows, columns);
        m_tableWidget->setHorizontalHeaderLabels(labels);
        for(int c = 0; c < columns; ++c) {
            for(int r = 0; r < rows; ++r) {
                auto item = new QTableWidgetItem(QString::fromStdString(data[c].second[r]));
                m_tableWidget->setItem(r, c, item);
            }
        }
        m_tableWidget->resizeColumnsToContents();
        m_tableWidget->resizeRowsToContents();
    } else {
        m_tableWidget = new QTableWidget();
    }
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto layout = new QHBoxLayout();
    setLayout(layout);
    layout->addWidget(m_tableWidget);
}

TableWidget::TableWidget(const TableData& data,
                         TableWidgetCallback *clickCallback,
                         QWidget *parent) : QWidget(parent) {
    init(data);
    if(clickCallback != nullptr) {
        m_clickCallbackClass = clickCallback;
        QObject::connect(m_tableWidget, &QTableWidget::cellClicked, [=](int row, int column) {
            m_clickCallbackClass->handle(this, row, column, false);
        });
        QObject::connect(m_tableWidget, &QTableWidget::cellDoubleClicked, [=](int row, int column) {
            m_clickCallbackClass->handle(this, row, column, true);
        });
    }
}

TableWidget::TableWidget(const TableData& data,
                         std::function<void(TableWidget*,int,int,bool)> clickCallback,
                         QWidget *parent) : QWidget(parent) {
    init(data);
    m_clickCallbackFunction = clickCallback;
    QObject::connect(m_tableWidget, &QTableWidget::cellClicked, [=](int row, int column) {
        m_clickCallbackFunction(this, row, column, false);
    });
    QObject::connect(m_tableWidget, &QTableWidget::cellDoubleClicked, [=](int row, int column) {
        m_clickCallbackFunction(this, row, column, true);
    });
}

void TableWidget::show(bool maximized) {
    if(maximized) {
        showMaximized();
    } else {
        QWidget::show();
    }
    if(QThread::currentThread()->loopLevel() <= 0) {
        auto loop = new QEventLoop;
        QObject::connect(this, &TableWidget::closed, loop, &QEventLoop::quit);
        loop->exec();
    }
}

void TableWidget::closeEvent(QCloseEvent *event) {
    emit closed();
    event->accept();
}

std::map<std::string, std::string> TableWidget::getRow(int i) {
    // TODO thread safety?
    std::map<std::string, std::string> data;
    for(int c = 0; c < m_tableWidget->columnCount(); ++c) {
        auto label = m_tableWidget->horizontalHeaderItem(c);
        auto value = m_tableWidget->item(i, c);
        data[label->text().toStdString()] = value->text().toStdString();
    }
    return data;
}

void TableWidget::setRow(int i, std::map<std::string, std::string> row) {
    // TODO thread safety?
    for(int c = 0; c < m_tableWidget->rowCount(); ++c) {
        auto label = m_tableWidget->horizontalHeaderItem(c)->text().toStdString();
        if(row.count(label) == 0)
            throw Exception("Label " + label + " not found in row data given to TableWidget::setRow");
        m_tableWidget->setItem(i, c, new QTableWidgetItem(QString::fromStdString(row[label])));
    }
}

void TableWidget::close() {
    QWidget::close();
}

}