/**
 * @example blockMatching.cpp
 */
#include <FAST/Algorithms/BlockMatching/BlockMatching.hpp>
#include <FAST/Streamers/ImageFileStreamer.hpp>
#include <FAST/Visualization/SimpleWindow.hpp>
#include <FAST/Visualization/ImageRenderer/ImageRenderer.hpp>
#include <FAST/Visualization/VectorFieldRenderer/VectorFieldRenderer.hpp>
#include <FAST/Visualization/VectorFieldRenderer/VectorFieldColorRenderer.hpp>
#include <FAST/Tools/CommandLineParser.hpp>
#include <FAST/Algorithms/VectorMedianFilter/VectorMedianFilter.hpp>
#include <FAST/Exporters/StreamToFileExporter.hpp>
#include <FAST/Exporters/ImageFileExporter.hpp>

using namespace fast;

int main(int argc, char** argv) {
    CommandLineParser parser("Block matching");
    parser.addPositionVariable(1, "path", Config::getTestDataPath() + "/US/Heart/ApicalFourChamber/US-2D_#.mhd", 
        "Path to the recording to stream, e.g. /path/to/frames/frame_#.mhd");
    parser.addVariable("block-size", "5", "The size in pixels of the blocks to match. Has to be odd.");
    parser.addVariable("search-size", "9", "The size in pixels of the grid to search for a match. Has to be odd");
    parser.addVariable("intensity-threshold", "75.0f", "Lower intensity of a threshold to search for a block. If a block has lower intensity that this, it will not look for a match.");
    parser.addVariable("median-filter-size", "7", "Size of vector median filter window to run after block matching. Must be odd and larger than 3.");
    parser.addVariable("export-to", "", "Specify a path to export the vector field for each frame. E.g. /path/to/somewhere/");
    parser.addChoice("export-format", {"mhd"}, "mhd", "Specify export format");
    parser.addChoice("matching-metric", {"SAD", "SSD", "NCC"}, "SAD", "Matching metric used for calculating how similar two blocks are.");
    parser.addOption("display-lines", "Display vector field as lines instead of color overlay");
    parser.addOption("disable-compression", "Disable compression when saving as mhd (.zraw)");
    parser.parse(argc, argv);

    auto streamer = ImageFileStreamer::create(parser.get("path"), true, true, 40);

    auto blockMatching = BlockMatching::create(
            parser.get<int>("block-size"),
            parser.get<int>("search-size"),
            BlockMatching::stringToMetric(parser.get("matching-metric"))
            )->connect(streamer);
    blockMatching->setIntensityThreshold(parser.get<float>("intensity-threshold"));
    blockMatching->enableRuntimeMeasurements();

    ProcessObject::pointer source = blockMatching;
    if(parser.get<int>("median-filter-size") > 0) {
        auto filter = VectorMedianFilter::create(parser.get<int>("median-filter-size"))->connect(blockMatching);
        source = filter;
    }

    if(!parser.gotValue("export-to")) {
        streamer->enableLooping();
        // Visualize
        auto renderer = ImageRenderer::create()->connect(streamer);

        auto window = SimpleWindow2D::create(Color::Black())->connect(renderer);
        if (parser.getOption("display-lines")) {
            window->connect(VectorFieldRenderer::create(1.0f, Color::Red())->connect(source));
        } else {
            auto vectorRenderer = VectorFieldColorRenderer::create();
            vectorRenderer->setIntensityWindow(1.0f);
            vectorRenderer->setIntensityLevel(0.5f);
            vectorRenderer->connect(source);
            window->connect(vectorRenderer);
        }

        window->run();
    } else {
        // TODO Use StreamToFileExporter instead..
        // Export vector fields to disk
        std::string exportPath = parser.get("export-to");
        int timestep = 0;
        auto dataStream = DataStream(source);
        while(!dataStream.isDone()) {
            auto frame = dataStream.getNextFrame<Image>();
            std::cout << "Processing frame: " << timestep << " " << frame.get() << std::endl;
            const std::string path = join(exportPath, "frame_" + std::to_string(timestep) + "." + parser.get("export-format"));
            auto exporter = ImageFileExporter::create(path);
            exporter->setInputData(frame);
            exporter->setCompression(!parser.getOption("disable-compression"));
            exporter->run();
            ++timestep;
        }
    }
    blockMatching->getRuntime()->print();
}