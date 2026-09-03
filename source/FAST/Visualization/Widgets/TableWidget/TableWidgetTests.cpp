#include "TableWidget.hpp"
#include <FAST/Testing.hpp>
#include <FAST/Visualization/Window.hpp>
#include <FAST/Importers/ImageFileImporter.hpp>
#include <FAST/Visualization/Shortcuts.hpp>
#include <FAST/Visualization/ImageRenderer/ImageRenderer.hpp>
#include <FAST/Visualization/SimpleWindow.hpp>

using namespace fast;

TEST_CASE("Table widget", "[fast][TableWidget][visual]") {
    Window::initializeQtApp(); // TODO is there a better way to do this?
    TableData data = {
        {"Column 1", {"1", "a", "c"}},
        {"Column 2", {"wee", "sdasd", " asdahosdass dh"}},
        {"Column 3", {"1.23", "aaa", "ccc ccc"}},
    };
    auto widget = new TableWidget(data);
    widget->show(true);
    widget->close();
}

TEST_CASE("Table widget in Window", "[fast][TableWidget][visual]") {
    auto importer = ImageFileImporter::create(Config::getTestDataPath() + "US/Heart/ApicalFourChamber/US-2D_0.mhd");

    TableData data = {
            {"Column 1", {"1", "a", "c"}},
            {"Column 2", {"wee", "sdasd", " asdahosdass dh"}},
            {"Column 3", {"1.23", "aaa", "ccc ccc"}},
    };
    auto widget = new TableWidget(data, [=](TableWidget* self, int row, int column, bool doubleClick) {
        auto data = self->getRow(row);
        std::cout << "Clicked on " << row << " " << column << "" << std::endl;
        if(doubleClick)
            std::cout << "It was a double click" << std::endl;
        std::cout << "Row data:" << std::endl;
        for(auto item : data) {
            std::cout << item.first << ": " << item.second << std::endl;
        }
    });
    auto renderer = ImageRenderer::create()->connect(importer);
    auto window = SimpleWindow2D::create()
            ->connect(renderer)
            ->connect(widget);
    //window->setTimeout(2000);
    window->run();
}
