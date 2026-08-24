#include "VectorFieldRenderer.hpp"
#include <FAST/Data/Image.hpp>
#include <FAST/Data/Mesh.hpp>

namespace fast {

VectorFieldRenderer::VectorFieldRenderer(float lineWidth, Color color, LabelColors labelColors, float opacity,
                                         bool drawOnTop) : LineRenderer(lineWidth, color, labelColors, opacity, drawOnTop) {

}

void VectorFieldRenderer::execute() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if(mStop) {
            return;
        }
    }

    std::unordered_map<uint, SpatialDataObject::pointer> vectorImages;
    // This simply gets the input data for each connection and puts it into a data structure
    for(uint inputNr = 0; inputNr < getNrOfInputConnections(); inputNr++) {
        if(hasNewInputData(inputNr)) {
            SpatialDataObject::pointer input = getInputData<SpatialDataObject>(inputNr);

            vectorImages[inputNr] = input;
        }
    }

    // Convert data to mesh
    for(uint inputNr = 0; inputNr < getNrOfInputConnections(); inputNr++) {
        Image::pointer image = std::dynamic_pointer_cast<Image>(vectorImages[inputNr]);
        auto imageAccess = image->getImageAccess(ACCESS_READ);
        std::vector<MeshVertex> vertices;
        std::vector<MeshLine> lines;
        Vector3f spacing = image->getSpacing();
        int counter = 0;
        int step = 3;
        for(int y = 0; y < image->getHeight(); y += step) {
            for(int x = 0; x < image->getWidth(); x += step) {
                VectorXf vector = imageAccess->getVector(Vector2i(x,y));
                float length = vector.norm();
                vector.x() *= spacing.x();
                vector.y() *= spacing.y();
                vector *= 1; // scale vector
                MeshVertex vertex1(Vector3f(x*spacing.x(), y*spacing.y(), 0));
                MeshVertex vertex2(Vector3f(x*spacing.x() + vector.x(), y*spacing.y() + vector.y(), 0));
                vertices.push_back(vertex1);
                vertices.push_back(vertex2);
                MeshLine line(counter, counter+1);
                lines.push_back(line);
                counter += 2;
            }
        }
        auto mesh = Mesh::create(vertices, lines);
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if(mHasRendered) {
                mHasRendered = false;
                mDataToRender[inputNr] = mesh;
            }
        }
    }
}

}