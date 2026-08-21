#include <FAST/Importers/ImageFileImporter.hpp>
#include <FAST/Visualization/ImageRenderer/ImageRenderer.hpp>
#include <FAST/Visualization/Shortcuts.hpp>
#include "FAST/Testing.hpp"
#include "FAST/Importers/VTKMeshFileImporter.hpp"
#include "LineRenderer.hpp"
#include "FAST/Visualization/SimpleWindow.hpp"
#include <FAST/Data/Mesh.hpp>

using namespace fast;

TEST_CASE("LineRenderer", "[fast][LineRenderer][visual]") {
    VTKMeshFileImporter::pointer importer = VTKMeshFileImporter::New();
    importer->setFilename(Config::getTestDataPath() + "centerline.vtk");

    LineRenderer::pointer renderer = LineRenderer::New();
    renderer->addInputConnection(importer->getOutputPort());
    renderer->setColor(0, Color::Red());
    SimpleWindow::pointer window = SimpleWindow::New();
    window->addRenderer(renderer);
    window->setTimeout(500);
    window->start();
}

TEST_CASE("LineRenderer 2D", "[fast][LineRenderer][visual]") {

    auto importer = ImageFileImporter::New();
    importer->setFilename(Config::getTestDataPath()+"US/CarotidArtery/Right/US-2D_0.mhd");
    auto imageRenderer = ImageRenderer::New();
    imageRenderer->addInputConnection(importer->getOutputPort());

    std::vector<MeshVertex> vertices = {
            MeshVertex(Vector3f(1, 5, 0)),
            MeshVertex(Vector3f(10, 10, 0)),
            MeshVertex(Vector3f(20, 20, 0)),
    };
    std::vector<MeshLine> lines = {
            MeshLine(0, 1),
            MeshLine(1, 2),
    };
    auto mesh = Mesh::create(vertices, lines);

    auto renderer = LineRenderer::create(0.5, Color::Red())->connect(mesh);

    auto window = SimpleWindow2D::create()->connect(imageRenderer)->connect(renderer);
    window->setTimeout(500);
    window->start();
}

TEST_CASE("LineRenderer 2D empty", "[fast][LineRenderer][visual]") {
    auto importer = ImageFileImporter::New();
    importer->setFilename(Config::getTestDataPath()+"US/CarotidArtery/Right/US-2D_0.mhd");
    auto imageRenderer = ImageRenderer::New();
    imageRenderer->addInputConnection(importer->getOutputPort());

    std::vector<MeshVertex> vertices = {
    };
    std::vector<MeshLine> lines = {
    };
    auto mesh = Mesh::create(vertices);

    auto renderer = LineRenderer::create()->connect(mesh);
    renderer->setDefaultLineWidth(0.5);
    renderer->setColor(0, Color::Red());

    auto window = SimpleWindow::New();
    window->addRenderer(imageRenderer);
    window->addRenderer(renderer);
    window->setTimeout(500);
    window->set2DMode();
    window->start();
}

TEST_CASE("LineRenderer label colors", "[fast][LineRenderer]") {
    auto image = Image::create(32, 32, TYPE_UINT8, 1);
    image->fill(0);

    auto vertices = {
            MeshVertex({2, 2, 0}, {}, {}, 1),
            MeshVertex({2, 30, 0}, {}, {}, 2),
            MeshVertex({30, 30, 0}, {}, {}, 3),
            MeshVertex({30, 2, 0}, {}, {}, 4),
    };
    auto lines = {
            MeshLine(0, 1),
            MeshLine(1, 2),
            MeshLine(2, 3),
            MeshLine(3, 0)
    };
    //auto mesh = Mesh::create(vertices, lines);
    auto mesh = Mesh::create({}, {});
    auto access = mesh->getMeshAccess(ACCESS_READ_WRITE);
    access->addVertices(vertices);
    access->addLines(lines);
    access->release();
    Display2DArgs args;
    args.image = image;
    args.lines = mesh;
    args.lineOpacity = 0.5f;
    args.timeout = 500;
    display2D(args);
}
