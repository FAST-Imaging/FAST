#include <FAST/Testing.hpp>
#include "TextRenderer.hpp"
#include <FAST/Visualization/SimpleWindow.hpp>
#include <FAST/Visualization/ImageRenderer/ImageRenderer.hpp>
#include <FAST/Importers/ImageFileImporter.hpp>
#include <FAST/Data/Text.hpp>

using namespace fast;

TEST_CASE("Text renderer", "[fast][TextRenderer][visual]") {

    auto text = Text::create("This is a test!");

    auto renderer = TextRenderer::New();
    renderer->addInputData(text);
    renderer->setFontSize(64);
    //renderer->setStyle(TextRenderer::STYLE_ITALIC);


    auto window = SimpleWindow::New();
    window->addRenderer(renderer);
    window->getView()->setBackgroundColor(Color::Black());
    window->set2DMode();
    window->setTimeout(1000);
    window->start();
}

TEST_CASE("Text renderer positioning", "[fast][TextRenderer][visual]") {

    auto text = Text::create("This is a test!");

    auto importer = ImageFileImporter::New();
    importer->setFilename(Config::getTestDataPath() + "/US/Axillary/US-2D_0.mhd");

    auto imageRenderer = ImageRenderer::New();
    imageRenderer->addInputConnection(importer->getOutputPort());

    auto renderer = TextRenderer::New();
    renderer->addInputData(text);
    renderer->setWorldPosition(Vector2f(0.1,0.1));
    renderer->setFontSize(64);
    //renderer->setStyle(TextRenderer::STYLE_ITALIC);


    auto window = SimpleWindow::New();
    window->addRenderer(imageRenderer);
    window->addRenderer(renderer);
    window->set2DMode();
    window->setTimeout(1000);
    window->start();
}
