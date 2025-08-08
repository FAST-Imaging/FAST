#pragma once

#include <FAST/Streamers/FileStreamer.hpp>

namespace fast {

class Image;

/**
 * @brief Stream a sequence of image files from disk
 *
 * Images has to be stored with an index before the file extension.
 * E.g. some_name_0.mhd, some_name_1.mhd, some_name_2.mhd ...
 * or some_name_0.png, some_name_1.png, some_name_2.png ...
 *
 * This streamer uses the ImageFileImporter to read the images from disk.
 *
 * <h3>Output ports</h3>
 * - 0: Image
 *
 * @ingroup streamers
 */
class FAST_EXPORT ImageFileStreamer : public FileStreamer {
    FAST_PROCESS_OBJECT(ImageFileStreamer)
    public:
        /**
         * @brief Create a ImageFileStreamer instance
         *
         * @param filenameFormat String of path and format of images. E.g. /path/to/files/frame_#.mhd. The hash sign #
         * will be replaced by an index
         * @param loop Whether to loop the recording or not
         * @param useTimestamps Whether to use timestamps in image files (if available) when streaming, or just stream as fast as possible
         * @param framerate If framerate is > 0, this framerate will be used for streaming the images
         * @param grayscale Convert images to grayscale if the source image is in color.
         * @return instance
         */
        FAST_CONSTRUCTOR(ImageFileStreamer,
                         std::string, filenameFormat,,
                         bool, loop, = false,
                         bool, useTimestamps, = true,
                         int, framerate, = -1,
                         bool, grayscale, = false
        );
#ifndef SWIG
        /**
         * @brief Create a ImageFileStreamer instance
         *
         * This is only available in C++ for now.
         *
         * @param filenameFormats List of strings of paths and format of images. E.g. /path/to/files/frame_#.mhd. The hash sign #
         * will be replaced by an index
         * @param loop Whether to loop the recording or not
         * @param useTimestamps Whether to use timestamps in image files (if available) when streaming, or just stream as fast as possible
         * @param framerate If framerate is > 0, this framerate will be used for streaming the images
         * @param grayscale Convert images to grayscale if the source image is in color.
         * @return instance
         */
        FAST_CONSTRUCTOR(ImageFileStreamer,
                         std::vector<std::string>, filenameFormats,,
                         bool, loop, = false,
                         bool, useTimestamps, = true,
                         int, framerate, = -1,
                         bool, grayscale, = false
        )
#endif
        /**
         * @brief Convert images to grayscale if the source image is in color
         * @param grayscale Convert or not
         */
        void setGrayscale(bool grayscale);
        void loadAttributes() override;
        ~ImageFileStreamer() { stop(); }; // FileStreamer derived classes must call stop() in destructor or you will get a pure virtual method called error
    protected:
        DataObject::pointer getDataFrame(std::string filename) override;

        ImageFileStreamer();

        bool m_grayscale = false;

};

} // end namespace fast
