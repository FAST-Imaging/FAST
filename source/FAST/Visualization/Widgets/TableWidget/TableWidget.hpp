#pragma once

#include <QWidget>
#include <string>
#include <FASTExport.hpp>
#include <iostream>

class QTableWidget;

namespace fast {

class TableWidget;

// The destructor is causing seg faults in python after is has been used in a window
#ifdef SWIG
%nodefaultdtor TableWidget;
%extend TableWidget {
    ~TableWidget() {
    }
};

%feature("director") TableWidgetCallback;
%pythoncode %{
_table_callbacks = [] # Hack to avoid callbacks being deleted
def TableCallback(func):
    global _table_callbacks
    class CB(TableWidgetCallback):
        def __init__(self):
            super().__init__()

        def handle(self, widget, row:int, column:int, doubleClick:bool):
            func(widget, row, column, doubleClick)
    obj = CB()
    _table_callbacks.append(obj)
    return obj
%}
#endif

/**
 * @brief Callback class for TableWidget
 * Used primarily by pyFAST
 */
class TableWidgetCallback {
    public:
        virtual void handle(TableWidget* widget, int row, int column, bool doubleClick) = 0;
        virtual ~TableWidgetCallback() { std::cout << "Destroying TableWidgetCallback" << std::endl;};
};

using TableData = std::vector<std::pair<std::string, std::vector<std::string>>>;

/**
 * @brief A table widget for displaying tabular data
 * @ingroup widgets
 */
class FAST_EXPORT TableWidget : public QWidget {
    Q_OBJECT
    public:
#ifndef SWIG
        TableWidget(
                const TableData& data,
                std::function<void(TableWidget*,int,int,bool)> clickCallback,
                QWidget* parent = nullptr
        );
#endif
        TableWidget(
                const TableData& data,
                TableWidgetCallback* clickCallback = nullptr,
                QWidget* parent = nullptr
        );
        std::map<std::string, std::string> getRow(int i);
        void setRow(int i, std::map<std::string, std::string> row);
        void show(bool maximized = false);
        void close();
#ifndef SWIG
        void closeEvent(QCloseEvent *event) override;
        Q_SIGNALS:
            void closed();
#endif
    private:
        void init(const TableData& data);
        QTableWidget* m_tableWidget;
        TableWidgetCallback* m_clickCallbackClass;
        std::function<void(TableWidget*, int, int, bool)> m_clickCallbackFunction;
};
}