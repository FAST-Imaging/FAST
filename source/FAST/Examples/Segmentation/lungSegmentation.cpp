/**
 * @example lungSegmentation.cpp
 */
#include <FAST/Tools/CommandLineParser.hpp>
#include <FAST/Exporters/MetaImageExporter.hpp>
#include <FAST/Exporters/VTKMeshFileExporter.hpp>
#include <FAST/Algorithms/CenterlineExtraction/CenterlineExtraction.hpp>
#include <FAST/Visualization/LineRenderer/LineRenderer.hpp>
#include "FAST/Importers/ImageFileImporter.hpp"
#include "FAST/Visualization/ImageRenderer/ImageRenderer.hpp"
#include "FAST/Visualization/SegmentationRenderer/SegmentationRenderer.hpp"
#include "FAST/Visualization/TriangleRenderer/TriangleRenderer.hpp"
#include "FAST/Visualization/SimpleWindow.hpp"
#include "FAST/Algorithms/SurfaceExtraction/SurfaceExtraction.hpp"
#include "FAST/Algorithms/LungSegmentation/LungSegmentation.hpp"

using namespace fast;

int main(int argc, char** argv) {
    CommandLineParser parser("Lung segmentation", "Segments the lungs, airways and blood vessels");
    parser.addPositionVariable(1, "input-filename", Config::getTestDataPath() + "CT/CT-Thorax.mhd");
    parser.addVariable("airway-seed", false, "Manual seed point coordinate for airway segmentation --airway-seed x,y,z");
    parser.addVariable("lung-seed", false, "Manual seed point coordinate for lung segmentation --lung-seed x,y,z");
    parser.addVariable("output-path", false, "Save data to this path in mhd and vtk format");
    parser.addOption("blood-vessel-centerline", "Extract centerline from blood vessels");
    parser.parse(argc, argv);

    // Import image from file using the ImageFileImporter
    auto importer = ImageFileImporter::create(parser.get("input-filename"));

    // Perform lung segmentation (this will also extract the airways using AirwaySegmentation)
    Vector3i airwaySeedPoint = Vector3i::Zero();
    Vector3i lungSeedPoint = Vector3i::Zero();
    if(parser.gotValue("airway-seed"))
        airwaySeedPoint = parser.get<Vector3i>("airway-seed");
    if(parser.gotValue("lung-seed"))
        lungSeedPoint = parser.get<Vector3i>("lung-seed");
    auto segmentation = LungSegmentation::create(airwaySeedPoint, lungSeedPoint, true)
            ->connect(importer);

    auto centerline = CenterlineExtraction::create();
    if(parser.getOption("blood-vessel-centerline")) {
        centerline->connect(segmentation, 2);
        centerline->getReporter().setGlobalReportMethod(Reporter::COUT);
    }

    if(parser.gotValue("output-path")) {
        auto exporter = MetaImageExporter::create(join(parser.get("output-path"), "vessel_segmentation.mhd"));
        exporter->connect(segmentation, 2);

        auto exporter2 = MetaImageExporter::create(join(parser.get("output-path"), "lung_segmentation.mhd"));
        exporter2->connect(segmentation, 0);

        auto exporter3 = MetaImageExporter::create(join(parser.get("output-path"), "airway_segmentation.mhd"));
        exporter3->connect(segmentation, 1);

        if(parser.getOption("blood-vessel-centerline")) {
            auto exporter2 = VTKMeshFileExporter::create(join(parser.get("output-path"), "centerline.vtk"));
            exporter2->connect(centerline);
            exporter2->run();
        }
        exporter->run();
        exporter2->run();
        exporter3->run();
    } else {

        // Extract lung surface
        auto extraction = SurfaceExtraction::create()
                ->connect(segmentation);

        // Extract airway surface
        auto extraction2 = SurfaceExtraction::create()
                ->connect(segmentation, 1);

        // Extract blood vessel
        auto extraction3 = SurfaceExtraction::create()
                ->connect(segmentation, 2);

        // Render both surfaces with different color
        auto renderer = TriangleRenderer::New();
        renderer->addInputConnection(extraction->getOutputPort(), Color::Green(), 0.6f);
        renderer->addInputConnection(extraction2->getOutputPort(), Color::Red(), 1.0f);
        renderer->addInputConnection(extraction3->getOutputPort(), Color::Blue(), 1.0f);

        auto window = SimpleWindow3D::create()->connect(renderer);

        if(parser.getOption("blood-vessel-centerline")) {
            auto lineRenderer = LineRenderer::create(1.0f, Color::Green(), {}, 1.0f, true)
                    ->connect(centerline);
            window->addRenderer(lineRenderer);
        }

#ifdef FAST_CONTINUOUS_INTEGRATION
        // This will automatically close the window after 5 seconds, used for CI testing
        window->setTimeout(5*1000);
#endif
        window->run();
    }
}
