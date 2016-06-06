#include "ImageCropper.hpp"
#include "FAST/Data/Image.hpp"

namespace fast {


void ImageCropper::setOffset(VectorXui offset) {
    mOffset.x() = offset.x();
    mOffset.y() = offset.y();
    if(offset.size() > 2) {
        mOffset.z() = offset.z();
    } else {
        mOffset.z() = 0;
    }
}

void ImageCropper::setSize(VectorXui size) {
    mSize.x() = size.x();
    mSize.y() = size.y();
    if(size.size() > 2) {
        mSize.z() = size.z();
    } else {
        mSize.z() = 1;
    }
}

ImageCropper::ImageCropper() {
    createInputPort<Image>(0);
    createOutputPort<Image>(0, OUTPUT_DEPENDS_ON_INPUT, 0);

    mSize = Vector3ui::Zero();
    mOffset = Vector3ui::Zero();
}

void ImageCropper::execute() {
    if(mSize == Vector3ui::Zero())
        throw Exception("Size must be given to ImageCropper");

    Image::pointer input = getStaticInputData<Image>();
    Image::pointer output = input->crop(mOffset, mSize);
    setStaticOutputData<Image>(0, output);
}

}
