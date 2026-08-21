/**
 * @example airwaySegmentation.cpp
 */
#include <FAST/Exporters/MetaImageExporter.hpp>
#include <FAST/Exporters/VTKMeshFileExporter.hpp>
#include "FAST/Algorithms/AirwaySegmentation/AirwaySegmentation.hpp"
#include "FAST/Importers/ImageFileImporter.hpp"
#include "FAST/Algorithms/SurfaceExtraction/SurfaceExtraction.hpp"
#include "FAST/Algorithms/CenterlineExtraction/CenterlineExtraction.hpp"
#include "FAST/Visualization/TriangleRenderer/TriangleRenderer.hpp"
#include "FAST/Visualization/LineRenderer/LineRenderer.hpp"
#include "FAST/Visualization/SimpleWindow.hpp"
#include "FAST/Tools/CommandLineParser.hpp"

using namespace fast;

int main(int argc, char** argv) {
    Reporter::setGlobalReportMethod(Reporter::COUT); // TODO remove
	CommandLineParser parser("Airway segmentation");
    parser.addPositionVariable(1, "filename", Config::getTestDataPath() + "CT/CT-Thorax.mhd");
    parser.addVariable("smoothing", "0.5", "How much smoothing to apply before segmentation");
    parser.addVariable("seed", false, "Manual seed point coordinate (--seed x,y,z)");
    parser.addVariable("export_segmentation", false, "Filename to export segmentation volume to. Example: segmentation.mhd");
    parser.addVariable("export_centerline", false, "Filename to export centerline to. Example: centerline.vtk");
	parser.parse(argc, argv);

	// Import CT data
	auto importer = ImageFileImporter::create(parser.get(1));

	// Do airway segmentation
	auto segmentation = AirwaySegmentation::create(parser.get<float>("smoothing"))->connect(importer);
	if(parser.gotValue("seed"))
        segmentation->setSeedPoint(parser.get<Vector3i>("seed"));

	// Extract centerline from segmentation
	auto centerline = CenterlineExtraction::create()->connect(segmentation);

    // Export data if user has specified to do so
    if(parser.gotValue("export_segmentation")) {
        auto exporter = MetaImageExporter::create(parser.get("export_segmentation"))
                ->connect(segmentation);
        exporter->run();
    }
    if(parser.gotValue("export_centerline")) {
        auto exporter = VTKMeshFileExporter::create(parser.get("export_centerline"))
                ->connect(centerline);
        exporter->run();
    }
	if(parser.gotValue("export_segmentation") || parser.gotValue("export_centerline"))
		return 0; // Dont render if exporting..

	// Extract surface from segmentation
	auto extraction = SurfaceExtraction::create()->connect(segmentation);

	// Set up renderers and window
	auto renderer = TriangleRenderer::create()->connect(extraction);

	auto lineRenderer = LineRenderer::create(1.0f, Color::Blue())->connect(centerline);
	lineRenderer->setDefaultDrawOnTop(true);

	auto window = SimpleWindow3D::create()->connect({renderer, lineRenderer});
#ifdef FAST_CONTINUOUS_INTEGRATION
	// This will automatically close the window after 5 seconds, used for CI testing
    window->setTimeout(5*1000);
#endif
	window->run();


}
