#include "OptionsWidget.hpp"
#include <FAST/Testing.hpp>
#include <FAST/Importers/ImageFileImporter.hpp>
#include <FAST/Visualization/Shortcuts.hpp>

using namespace fast;

TEST_CASE("OptionsWidget", "[fast][OptionsWidget][visual]") {
    auto importer = ImageFileImporter::create(Config::getTestDataPath() + "US/US-2D.jpg");
    auto widget = new OptionsWidget({"Option 1", "Option 2", "Option 3"}, "Cool options:", "Select a value ...", -1, [](int index, std::string value){
        std::cout << "Selected: " << index << " " << value << std::endl;
    });
    Display2DArgs args;
    args.image = importer;
    args.widgets = std::vector<QWidget*>{widget};
    args.timeout = 1000;
    display2D(args);
}