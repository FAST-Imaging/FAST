#include "ImageImporter.hpp"
#include <memory>
#include <QImage>
#include "FAST/Data/DataTypes.hpp"
#include "FAST/DeviceManager.hpp"
#include "FAST/Exception.hpp"
#include "FAST/Data/Image.hpp"
#include <cctype>
#include <algorithm>
#include <utility>
#include <FAST/Algorithms/Compression/JPEGXLCompression.hpp>
#include <fstream>
#include <FAST/Algorithms/Compression/JPEGCompression.hpp>

namespace fast {

void ImageImporter::execute() {
    if(m_filename.empty())
        throw Exception("No filename was supplied to the ImageImporter");

    size_t pos = m_filename.rfind(".", -5);
    if(pos == std::string::npos)
        throw Exception("ImageImporter filename had no extension");

    std::string ext = stringToLower(m_filename.substr(pos + 1));
    if(ext == "jxl") {
        std::ifstream file(m_filename, std::ios::in | std::ios::binary);
        std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(file), {});
        JPEGXLCompression jxl;
        int width, height;
        mRuntimeManager->startRegularTimer("jxl_decompression");
        void* data = jxl.decompress(buffer.data(), buffer.size()*sizeof(uchar), &width, &height);
        mRuntimeManager->stopRegularTimer("jxl_decompression");
        reportInfo() << "Decompressed JPEGXL image for size " << width << " " << height << Reporter::end();
        std::unique_ptr<uchar[]> decompressedData((uchar*)data); // Use unique_ptr to avoid copy
        auto image = Image::create(width, height, TYPE_UINT8, 3, std::move(decompressedData));
        addOutputData(0, image);
    } else if(ext == "jpg" || ext == "jpeg") {
        std::ifstream file(m_filename, std::ios::in | std::ios::binary);
        std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(file), {});
        JPEGCompression jpg;
        int width, height;
        mRuntimeManager->startRegularTimer("jpeg_decompression");
        void* data = jpg.decompress(buffer.data(), buffer.size()*sizeof(uchar), &width, &height);
        mRuntimeManager->stopRegularTimer("jpeg_decompression");
        std::unique_ptr<uchar[]> decompressedData((uchar*)data); // Use unique_ptr to avoid copy
        auto image = Image::create(width, height, TYPE_UINT8, 3, std::move(decompressedData));
        addOutputData(0, image);
    } else {
        uchar* convertedPixelData;
        // Load image from disk using Qt
        QImage image;
        reportInfo() << "Trying to load image in ImageImporter" << Reporter::end();
        if(!image.load(m_filename.c_str())) {
            throw FileNotFoundException(m_filename);
        }
        reportInfo() << "Loaded image with size " << image.width() << " "  << image.height() << Reporter::end();

        QImage::Format format;
        if(mGrayscale) {
            format = QImage::Format_Grayscale8;
        } else {
            format = QImage::Format_RGB888;
        }
        QImage convertedImage = image.convertToFormat(format);

        // Get pixel data
        convertedPixelData = convertedImage.bits();

        if(convertedImage.width()*convertedImage.depth()/8 != convertedImage.bytesPerLine()) {
            const int bytesPerPixel = (convertedImage.depth()/8);
            std::unique_ptr<uchar[]> fixedPixelData = std::make_unique<uchar[]>(image.width()*image.height()*bytesPerPixel);
            // Misalignment
            for(int scanline = 0; scanline < image.height(); ++scanline) {
                std::memcpy(
                        &fixedPixelData[scanline*image.width()*bytesPerPixel],
                        &convertedPixelData[scanline*convertedImage.bytesPerLine()],
                        image.width()*bytesPerPixel
                        );
            }
            auto output = Image::create(
                    image.width(),
                    image.height(),
                    TYPE_UINT8,
                    mGrayscale ? 1 : 3,
                    getMainDevice(),
                    fixedPixelData.get()
                    );
            addOutputData(0, output);
        } else {
            auto output = Image::create(
                    image.width(),
                    image.height(),
                    TYPE_UINT8,
                    mGrayscale ? 1 : 3,
                    getMainDevice(),
                    convertedPixelData
                    );
            addOutputData(0, output);
        }
    }
}

void ImageImporter::loadAttributes() {
    setFilename(getStringAttribute("filename"));
    setGrayscale(getBooleanAttribute("grayscale"));
}

ImageImporter::ImageImporter() {
    mGrayscale = false;
    mIsModified = true;
    createOutputPort(0, "Image");
    createBooleanAttribute("grayscale", "Grayscale", "Whether to convert image to grayscale or not", mGrayscale);
}

ImageImporter::ImageImporter(std::string filename, bool convertToGrayscale) : FileImporter(std::move(filename)) {
    createOutputPort(0, "Image");
    createBooleanAttribute("grayscale", "Grayscale", "Whether to convert image to grayscale or not", mGrayscale);
    setGrayscale(convertToGrayscale);
}

void ImageImporter::setGrayscale(bool grayscale) {
    mGrayscale = grayscale;
    setModified(true);
}

}
