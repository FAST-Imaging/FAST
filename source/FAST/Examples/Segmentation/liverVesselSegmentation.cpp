#include <FAST/Algorithms/HounsefieldConverter/HounsefieldConverter.hpp>
#include "FAST/Algorithms/TubeSegmentationAndCenterlineExtraction/TubeSegmentationAndCenterlineExtraction.hpp"
#include "FAST/Importers/ImageFileImporter.hpp"
#include "FAST/Visualization/SliceRenderer/SliceRenderer.hpp"
#include "FAST/Visualization/LineRenderer/LineRenderer.hpp"
#include "FAST/Algorithms/SurfaceExtraction/SurfaceExtraction.hpp"
#include "FAST/Visualization/TriangleRenderer/TriangleRenderer.hpp"
#include "FAST/Visualization/SimpleWindow.hpp"
#include <FAST/Tools/CommandLineParser.hpp>

using namespace fast;

int main(int argc, char** argv) {
    CommandLineParser parser("Liver vessel segmentation");
    parser.addPositionVariable(1, "filename", true, "Path to CT file");
    parser.parse(argc, argv);

    auto importer = ImageFileImporter::create(parser.get("filename"));

    auto converter = HounsefieldConverter::create()->connect(importer);

    auto tubeExtraction = TubeSegmentationAndCenterlineExtraction::create()->connect(converter);

    // Parameters
    tubeExtraction->extractBrightTubes();
    tubeExtraction->setMinimumRadius(1);
    tubeExtraction->setMaximumRadius(25);
    tubeExtraction->setRadiusStep(0.5);
    tubeExtraction->setSensitivity(0.95);
    tubeExtraction->setMinimumIntensity(100);
    tubeExtraction->setMaximumIntensity(200);
    tubeExtraction->setMinimumTreeSize(500);
    tubeExtraction->setKeepLargestTree(true);
    tubeExtraction->enableAutomaticCropping();

    auto renderer = SliceRenderer::create(PLANE_Z)->connect(importer);

    auto lineRenderer = LineRenderer::create(1.0f, Color::Blue(), {}, 1.0, true)
        ->connect(tubeExtraction, 1);

    auto surfaceExtraction = SurfaceExtraction::create()
        ->connect(tubeExtraction);

    auto triangleRenderer = TriangleRenderer::create()->connect(surfaceExtraction);

    auto window = SimpleWindow3D::create()->connect({
        renderer,
        triangleRenderer,
        lineRenderer
	});
    window->run();
}
