#include <FAST/Data/ImagePyramid.hpp>
#include "TissueSegmentation.hpp"
#include <FAST/Algorithms/Morphology/Dilation.hpp>
#include <FAST/Algorithms/Morphology/Erosion.hpp>
#include <FAST/Algorithms/GaussianSmoothingFilter/GaussianSmoothingFilter.hpp>

namespace fast {

TissueSegmentation::TissueSegmentation() {
    createInputPort<ImagePyramid>(0);
    createOutputPort<Segmentation>(0);

    createOpenCLProgram(Config::getKernelSourcePath() + "/Algorithms/TissueSegmentation/TissueSegmentation.cl");
}

void TissueSegmentation::setThreshold(int thresh) {
    m_thresh = thresh;
}

void TissueSegmentation::setDilate(int radius) {
    m_dilate = radius;
}

void TissueSegmentation::setErode(int radius) {
    m_erode = radius;
}

int TissueSegmentation::getThreshold() const {
    return m_thresh;
}

int TissueSegmentation::getErode() const {
    return m_erode;
}

int TissueSegmentation::getDilate() const {
    return m_dilate;
}

void TissueSegmentation::execute() {
    auto wsi = getInputData<ImagePyramid>();
    auto access = wsi->getAccess(ACCESS_READ);
    auto input = access->getLevelAsImage(wsi->getNrOfLevels()-1);

    auto output = Segmentation::New();
    output->createFromImage(input);

    {
        auto device = std::dynamic_pointer_cast<OpenCLDevice>(getMainDevice());
        auto program = getOpenCLProgram(device);
        cl::Kernel kernel(program, "segment");

        auto inputAccess = input->getOpenCLImageAccess(ACCESS_READ, device);
        auto outputAccess = output->getOpenCLImageAccess(ACCESS_READ_WRITE, device);
        kernel.setArg(0, *inputAccess->get2DImage());
        kernel.setArg(1, *outputAccess->get2DImage());
        kernel.setArg(2, m_thresh);

        device->getCommandQueue().enqueueNDRangeKernel(
                kernel,
                cl::NullRange,
                cl::NDRange(input->getWidth(), input->getHeight()),
                cl::NullRange
        );
        device->getCommandQueue().finish();
    }

    if ((m_dilate == 0) && (m_erode == 0)) {
        addOutputData(0, output);  // no morphological post-processing

    } else if ((m_dilate > 0) && (m_erode == 0)){
        auto dilation = Dilation::New();
        dilation->setInputData(output);
        dilation->setStructuringElementSize(m_dilate);

        auto newOutput = dilation->updateAndGetOutputData<Image>();
        addOutputData(0, newOutput);

    } else if ((m_dilate == 0) && (m_erode > 0)){
        auto erosion = Erosion::New();
        erosion->setInputData(output);
        erosion->setStructuringElementSize(m_erode);

        auto newOutput = erosion->updateAndGetOutputData<Image>();
        addOutputData(0, newOutput);

    } else {
        auto dilation = Dilation::New();
        dilation->setInputData(output);
        dilation->setStructuringElementSize(m_dilate);

        auto erosion = Erosion::New();
        erosion->setInputConnection(dilation->getOutputPort());
        erosion->setStructuringElementSize(m_erode);

        auto newOutput = erosion->updateAndGetOutputData<Image>();
        addOutputData(0, newOutput);  // closing (instead of opening) to increase sensitivity in detection
    }

}

}