#include "ImageSharpening.hpp"

namespace fast {

void ImageSharpening::loadAttributes() {
	setGain(getFloatAttribute("gain"));
	GaussianSmoothingFilter::loadAttributes();
}

ImageSharpening::ImageSharpening() {
	createInputPort<Image>(0);
	createOutputPort<Image>(0);

    createOpenCLProgram(Config::getKernelSourcePath() + "Algorithms/ImageSharpening/ImageSharpening.cl");
    createFloatAttribute("gain", "Unsharp masking gain", "Unsharp masking gain", m_gain);
}

void ImageSharpening::setGain(float gain) {
    m_gain = gain;
    setModified(true);
}

void ImageSharpening::execute() {
    auto input = getInputData<Image>(0);

    if(input->getDimensions() != 2)
        throw Exception("ImageSharpening only supports 2D images");

    auto output = getOutputData<Image>(0);

    char maskSize = mMaskSize;
    if(maskSize <= 0) // If mask size is not set calculate it instead
        maskSize = ceil(2*mStdDev)*2+1;

    if(maskSize > 19)
        maskSize = 19;

    // Initialize output image
    if(mOutputTypeSet) {
        output->create(input->getSize(), mOutputType, input->getNrOfChannels());
        output->setSpacing(input->getSpacing());
    } else {
        output->createFromImage(input);
    }
    mOutputType = output->getDataType();
    SceneGraph::setParentNode(output, input);

	auto clDevice = std::static_pointer_cast<OpenCLDevice>(getMainDevice());

    const auto halfSize = (maskSize-1)/2;
    cl::Kernel kernel(getOpenCLProgram(clDevice, "", "-DHALF_SIZE=" + std::to_string(halfSize)), "sharpen");

	auto inputAccess = input->getOpenCLImageAccess(ACCESS_READ, clDevice);
	//createMask(input, maskSize, false);
	kernel.setArg(2, mStdDev);
    kernel.setArg(3, m_gain);
	auto globalSize = cl::NDRange(input->getWidth(),input->getHeight());

	auto outputAccess = output->getOpenCLImageAccess(ACCESS_READ_WRITE, clDevice);
	kernel.setArg(0, *inputAccess->get2DImage());
	kernel.setArg(1, *outputAccess->get2DImage());
    clDevice->getCommandQueue().enqueueNDRangeKernel(
        kernel,
        cl::NullRange,
        globalSize,
        cl::NullRange
	);
}

}