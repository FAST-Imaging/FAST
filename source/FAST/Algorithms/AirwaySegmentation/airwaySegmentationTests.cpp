#include "FAST/Testing.hpp"
#include "AirwaySegmentation.hpp"
#include "FAST/Importers/ImageFileImporter.hpp"
#include "FAST/Algorithms/SurfaceExtraction/SurfaceExtraction.hpp"
#include "FAST/Algorithms/CenterlineExtraction/CenterlineExtraction.hpp"
#include "FAST/Visualization/TriangleRenderer/TriangleRenderer.hpp"
#include "FAST/Visualization/LineRenderer/LineRenderer.hpp"
#include "FAST/Visualization/SimpleWindow.hpp"

using namespace fast;

/*
TEST_CASE("Airway segmentation ALL", "[fast][AirwaySegmentation]") {
	Reporter::setGlobalReportMethod(Reporter::COUT);
	for(int i = 1; i < 26; ++i) {
        ImageFileImporter::pointer importer = ImageFileImporter::New();
        //importer->setFilename(Config::getTestDataPath() + "/CT/CT-Thorax.mhd");
        std::string nr = std::to_string(i);
        if(nr.size() == 1) {
        	nr = "0" + nr;
        }
        importer->setFilename("/home/smistad/Data/lunge_datasett/pasient" + nr + ".mhd");

        AirwaySegmentation::pointer segmentation = AirwaySegmentation::New();
        segmentation->setInputConnection(importer->getOutputPort());

        CenterlineExtraction::pointer centerline = CenterlineExtraction::New();
        centerline->setInputConnection(segmentation->getOutputPort());
        centerline->update();

        SurfaceExtraction::pointer extraction = SurfaceExtraction::create();
        extraction->setInputConnection(segmentation->getOutputPort());

        TriangleRenderer::pointer renderer = TriangleRenderer::New();
        renderer->addInputConnection(extraction->getOutputPort());

        LineRenderer::pointer lineRenderer = LineRenderer::New();
        lineRenderer->addInputConnection(centerline->getOutputPort());
        lineRenderer->setDefaultDrawOnTop(true);

        SimpleWindow::pointer window = SimpleWindow::New();
        window->addRenderer(renderer);
        window->addRenderer(lineRenderer);
        window->start();
	}
}
*/

TEST_CASE("Airway segmentation", "[fast][AirwaySegmentation][visual]") {
	auto importer = ImageFileImporter::create(Config::getTestDataPath() + "CT/CT-Thorax.mhd");

	auto segmentation = AirwaySegmentation::create()->connect(importer);
	segmentation->enableRuntimeMeasurements();

	auto centerline = CenterlineExtraction::New();
	centerline->setInputConnection(segmentation->getOutputPort());

	auto extraction = SurfaceExtraction::create()->connect(segmentation);

	auto renderer = TriangleRenderer::create()->connect(extraction);

	auto lineRenderer = LineRenderer::create(1.0f, Color::Blue())->connect(centerline);
	lineRenderer->setDefaultDrawOnTop(true);

	auto window = SimpleWindow3D::create()->connect({renderer, lineRenderer});
	window->setTimeout(1000);
	window->run();
	//segmentation->getRuntime()->print();
}
