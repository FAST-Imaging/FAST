#include <FAST/Testing.hpp>
#include <FAST/Visualization/Shortcuts.hpp>
#include <FAST/Importers/ImageFileImporter.hpp>
#include <FAST/Importers/WholeSlideImageImporter.hpp>
#include <FAST/Algorithms/Thresholding/BinaryThresholding.hpp>
#include <FAST/Visualization/Widgets/TextWidget/TextWidget.hpp>
#include <FAST/Data/Mesh.hpp>

using namespace fast;

TEST_CASE("display2D image importer only", "[fast][shortcuts][display2D][visual]") {
    auto importer = ImageFileImporter::create(Config::getTestDataPath() + "US/US-2D.jpg");

    Display2DArgs args;
    args.image = importer;
    args.bgcolor = Color::Black();
    args.timeout = 1000;
    display2D(args);
    //display2D({.image = importer}); // C++ 20 needed
}

TEST_CASE("display2D no data throws", "[fast][shortcuts][display2D][visual]") {
    Display2DArgs args;
    args.bgcolor = Color::Black();
    args.timeout = 1000;
    CHECK_THROWS(display2D(args));
}

TEST_CASE("display2D image and segmentation", "[fast][shortcuts][display2D][visual]") {
    auto importer = ImageFileImporter::create(Config::getTestDataPath() + "US/US-2D.jpg");
    auto segmentation = BinaryThresholding::create(100)->connect(importer);

    Display2DArgs args;
    args.image = importer;
    args.segmentation = segmentation;
    args.segmentationColors = {{1, Color::Red()}};
    args.bgcolor = Color::Black();
    args.timeout = 1000;
    display2D(args);
}

TEST_CASE("display2D image only", "[fast][shortcuts][display2D][visual]") {
    auto image = ImageFileImporter::create(Config::getTestDataPath() + "US/US-2D.jpg")->runAndGetOutputData<Image>();

    Display2DArgs args;
    args.image = image;
    args.timeout = 1000;
    display2D(args);
    //display2D({.image = importer}); // C++ 20 needed
}

TEST_CASE("display2D wsi", "[fast][shortcuts][display2D][visual]") {
    auto importer = WholeSlideImageImporter::create(Config::getTestDataPath() + "WSI/CMU-1.svs");

    Display2DArgs args;
    args.imagePyramid = importer;
    args.width = 1024;
    args.height = 512;
    args.timeout = 1000;
    display2D(args);
}

TEST_CASE("display2D render to image", "[fast][shortcuts][display2D][visual]") {
    auto importer = ImageFileImporter::create(Config::getTestDataPath() + "US/US-2D.jpg");
    auto segmentation = BinaryThresholding::create(100)->connect(importer);

    Display2DArgs args;
    args.image = importer;
    args.segmentation = segmentation;
    args.segmentationColors = {{1, Color::Blue()}};
    args.renderToImage = true;
    auto image = display2D(args);

    Display2DArgs args2;
    args2.image = std::get<Image::pointer>(image);
    args2.timeout = 1000;
    display2D(args2);
}

TEST_CASE("display2D with widgets", "[fast][shortcuts][display2D][visual]") {
    auto importer = ImageFileImporter::create(Config::getTestDataPath() + "US/US-2D.jpg");

    auto widget1 = new TextWidget("Bottom");
    Display2DArgs args;
    args.image = importer;
    args.timeout = 1000;
    args.widgets = std::vector<QWidget*>{widget1};
    display2D(args);

    auto widget2 = new TextWidget("Bottom");
    auto widget3 = new TextWidget("Top");
    auto widget4 = new TextWidget("Right");
    auto widget5 = new TextWidget("Left");

    args.widgets = std::map<WidgetPosition, std::vector<QWidget*>>{
            {WidgetPosition::BOTTOM, {widget2}},
            {WidgetPosition::TOP, {widget3}},
            {WidgetPosition::RIGHT, {widget4}},
            {WidgetPosition::LEFT, {widget5}},
    };
    display2D(args);
}

TEST_CASE("display2D with mesh data", "[fast][shortcuts][display2D][visual][what]") {
    auto importer = ImageFileImporter::create(Config::getTestDataPath() + "US/JugularVein/US-2D_100.mhd");
    auto image = importer->runAndGetOutputData<Image>();
    auto spacing = image->getSpacing();

    std::vector<MeshVertex> vertices = {
        MeshVertex({10*spacing[0], 10*spacing[1], 0}),
        MeshVertex({10*spacing[0], 50*spacing[1], 0}),
        MeshVertex({200*spacing[0], 200*spacing[1], 0}),
    };
    std::vector<MeshLine> lines = {
        MeshLine(0, 1),
        MeshLine(1, 2),
    };

    auto mesh = Mesh::create(vertices, lines);

    Display2DArgs args;
    args.image = importer;
    args.lines = mesh;
    args.lineColor = Color::Red();
    args.lineWidth = 1;
    args.timeout = 1000;
    display2D(args);

    Display2DArgs args2;
    args2.image = importer;
    args2.vertices = mesh;
    args2.vertexSize = 2;
    args2.vertexSizeIsInPixels = false;
    args2.vertexMinSize = 1;
    args2.vertexOpacity = 0.5;
    args2.timeout = 1000;
    display2D(args2);
}

TEST_CASE("display3D image importer only", "[fast][shortcuts][display2D][visual]") {
    auto importer = ImageFileImporter::create(Config::getTestDataPath() + "CT/CT-Thorax.mhd");

    Display3DArgs args;
    args.image = importer;
    args.bgcolor = Color::Black();
    args.timeout = 1000;
    display3D(args);
}

TEST_CASE("display3D image and segmentation slicer", "[fast][shortcuts][display3D][visual]") {
    auto importer = ImageFileImporter::create(Config::getTestDataPath() + "CT/CT-Thorax.mhd");
    auto segmentation = BinaryThresholding::create(300)->connect(importer);

    Display3DArgs args;
    args.image = importer;
    args.segmentation = segmentation;
    args.displayType = DisplayType::SLICER;
    args.bgcolor = Color::Black();
    args.timeout = 1000;
    display3D(args);
}

TEST_CASE("display3D no data throws", "[fast][shortcuts][display3D][visual]") {
    Display3DArgs args;
    args.bgcolor = Color::Black();
    args.timeout = 1000;
    CHECK_THROWS(display3D(args));
}

TEST_CASE("display3D alpha blending ", "[fast][shortcuts][display3D][visual]") {
    auto importer = ImageFileImporter::create(Config::getTestDataPath() + "CT/CT-Thorax.mhd");

    Display3DArgs args;
    args.image = importer;
    args.displayType = DisplayType::ALPHA_BLENDING;
    args.timeout = 1000;
    display3D(args);
}

TEST_CASE("display3D MIP ", "[fast][shortcuts][display2D][visual]") {
    auto importer = ImageFileImporter::create(Config::getTestDataPath() + "CT/CT-Thorax.mhd");

    Display3DArgs args;
    args.image = importer;
    args.displayType = DisplayType::MAXIMUM_INTENSITY_PROJECTION;
    args.timeout = 1000;
    display3D(args);
}

TEST_CASE("display3D with widgets", "[fast][shortcuts][display3D][visual]") {
    auto importer = ImageFileImporter::create(Config::getTestDataPath() + "CT/CT-Thorax.mhd");

    auto widget1 = new TextWidget("Bottom");
    Display3DArgs args;
    args.image = importer;
    args.displayType = DisplayType::MAXIMUM_INTENSITY_PROJECTION;
    args.timeout = 1000;
    args.widgets = std::vector<QWidget*>{widget1};
    display3D(args);

    auto widget2 = new TextWidget("Bottom");
    auto widget3 = new TextWidget("Top");
    auto widget4 = new TextWidget("Right");
    auto widget5 = new TextWidget("Left");

    args.widgets = std::map<WidgetPosition, std::vector<QWidget*>>{
            {WidgetPosition::BOTTOM, {widget2}},
            {WidgetPosition::TOP, {widget3}},
            {WidgetPosition::RIGHT, {widget4}},
            {WidgetPosition::LEFT, {widget5}},
    };
    display3D(args);
}
