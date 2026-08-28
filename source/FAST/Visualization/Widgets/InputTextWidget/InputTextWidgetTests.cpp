#include <FAST/Testing.hpp>
#include <FAST/Visualization/ImageRenderer/ImageRenderer.hpp>
#include <FAST/Visualization/SimpleWindow.hpp>
#include "InputTextWidget.hpp"

using namespace fast;

TEST_CASE("Input text widget single line", "[fast][InputTextWidget][visual]") {
    auto renderer = ImageRenderer::create();

    auto widget = new InputTextWidget("Single line input text:", "Test", true);

    CHECK(widget->getText() == "Test");

    widget->setText("Hello world!");

    CHECK(widget->getText() == "Hello world!");

    auto window = SimpleWindow2D::create()
        ->connect(renderer)
        ->connect(widget);
    window->setTimeout(1000);
    window->run();

    CHECK(widget->getText() == "Hello world!");
}

TEST_CASE("Input text widget multi-line", "[fast][InputTextWidget][visual]") {
    auto renderer = ImageRenderer::create();

    auto widget = new InputTextWidget("Multi-line input text:", "Test", false);

    CHECK(widget->getText() == "Test");

    widget->setText("Hello world!");

    CHECK(widget->getText() == "Hello world!");

    auto window = SimpleWindow2D::create()
            ->connect(renderer)
            ->connect(widget);
    window->setTimeout(1000);
    window->run();

    CHECK(widget->getText() == "Hello world!");
}
