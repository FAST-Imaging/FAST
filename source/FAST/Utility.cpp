#include "FAST/Utility.hpp"

#ifdef FAST_MODULE_VISUALIZATION
// For download and extract zip file
#include <iomanip>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QStandardPaths>
#endif

#include "FAST/Config.hpp"
#define _USE_MATH_DEFINES
#include <cmath>
#ifdef _WIN32
#include <windows.h>
#include <direct.h> // Needed for _mkdir
#include <io.h> // needed for _access_s
#include <sys/types.h>
#include <sys/stat.h> // _stat
#else
// Needed for making directory
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h> // needed for DIR
#include <unistd.h>
#include <sys/ioctl.h>
#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif
#include <zip/zip.h>
#include <random>
#include <iomanip>

namespace fast {

double log2(double n) {
    return log( n ) / log( 2.0 );
}

double round(double n) {
	return (n - floor(n) > 0.5) ? ceil(n) : floor(n);
}

double round(double n, int decimals) {
    int factor = decimals*10;
    return decimals == 0 ? round(n) : round(n*factor)/factor;
}

void* allocateDataArray(unsigned int voxels, DataType type, unsigned int nrOfComponents) {
    unsigned int size = voxels*nrOfComponents;
    void * data;
    switch(type) {
        fastSwitchTypeMacro(data = new FAST_TYPE[size])
    }

    return data;
}

template <class T>
inline void getMaxAndMinFromOpenCLImageResult(void* voidData, unsigned int size, unsigned int nrOfComponents, float* min, float* max) {
    T* data = (T*)voidData;
    *min = data[0];
    *max = data[1];
    for(unsigned int i = nrOfComponents; i < size*nrOfComponents; i += nrOfComponents) {
        if(data[i] < *min) {
            *min = data[i];
        }
        if(data[i+1] > *max) {
            *max = data[i+1];
        }
    }
}

unsigned int getPowerOfTwoSize(unsigned int size) {
    int i = 1;
    while(pow(2, i) < size)
        i++;

    return (unsigned int)pow(2,i);
}

void getIntensitySumFromOpenCLImage(OpenCLDevice::pointer device, cl::Image2D image, DataType type, float* sum) {
    // Get power of two size
    unsigned int powerOfTwoSize = getPowerOfTwoSize(std::max(image.getImageInfo<CL_IMAGE_WIDTH>(), image.getImageInfo<CL_IMAGE_HEIGHT>()));

    // Create image levels
    unsigned int size = powerOfTwoSize;
    size /= 2;
    std::vector<cl::Image2D> levels;
    while(size >= 4) {
        cl::Image2D level = cl::Image2D(device->getContext(), CL_MEM_READ_WRITE, getOpenCLImageFormat(device, CL_MEM_OBJECT_IMAGE2D, TYPE_FLOAT, 1), size, size);
        levels.push_back(level);
        size /= 2;
    }

    // Compile OpenCL code
    std::string buildOptions = "";
    switch(type) {
    case TYPE_FLOAT:
        buildOptions = "-DTYPE_FLOAT";
        break;
    case TYPE_UINT8:
        buildOptions = "-DTYPE_UINT8";
        break;
    case TYPE_INT8:
        buildOptions = "-DTYPE_INT8";
        break;
    case TYPE_UINT16:
        buildOptions = "-DTYPE_UINT16";
        break;
    case TYPE_INT16:
        buildOptions = "-DTYPE_INT16";
        break;
    case TYPE_UINT32:
        buildOptions = "-DTYPE_UINT32";
        break;
    case TYPE_INT32:
        buildOptions = "-DTYPE_INT32";
            break;
    }
    std::string sourceFilename = Config::getKernelSourcePath() + "/ImageSum.cl";
    std::string programName = sourceFilename + buildOptions;
    // Only create program if it doesn't exist for this device from before
    if(!device->hasProgram(programName))
        device->createProgramFromSourceWithName(programName, sourceFilename, buildOptions);
    cl::Program program = device->getProgram(programName);
    cl::CommandQueue queue = device->getCommandQueue();

    // Fill first level
    size = powerOfTwoSize/2;
    cl::Kernel firstLevel(program, "createFirstSumImage2DLevel");
    firstLevel.setArg(0, image);
    firstLevel.setArg(1, levels[0]);

    queue.enqueueNDRangeKernel(
            firstLevel,
            cl::NullRange,
            cl::NDRange(size,size),
            cl::NullRange
    );

    // Fill all other levels
    cl::Kernel createLevel(program, "createSumImage2DLevel");
    int i = 0;
    size /= 2;
    while(size >= 4) {
        createLevel.setArg(0, levels[i]);
        createLevel.setArg(1, levels[i+1]);
        queue.enqueueNDRangeKernel(
                createLevel,
                cl::NullRange,
                cl::NDRange(size,size),
                cl::NullRange
        );
        i++;
        size /= 2;
    }

    // Get result from the last level
    unsigned int nrOfElements = 4*4;
    unsigned int nrOfComponents = getOpenCLImageFormat(device, CL_MEM_OBJECT_IMAGE2D, TYPE_FLOAT, 1).image_channel_order == CL_RGBA ? 4 : 1;
    float* result = (float*)allocateDataArray(nrOfElements,TYPE_FLOAT,nrOfComponents);
    queue.enqueueReadImage(levels[levels.size()-1],CL_TRUE,createOrigoRegion(),createRegion(4,4,1),0,0,result);
    *sum = getSumFromOpenCLImageResult<float>(result, nrOfElements, nrOfComponents);
    delete[] result;
}


void getIntensitySumFromOpenCLImage(OpenCLDevice::pointer device, cl::Image3D image, DataType type, float* sum) {
    // Get power of two size
    unsigned int powerOfTwoSize = getPowerOfTwoSize(
            std::max(std::max(image.getImageInfo<CL_IMAGE_WIDTH>(), image.getImageInfo<CL_IMAGE_HEIGHT>()), image.getImageInfo<CL_IMAGE_DEPTH>()));

    // Create image levels
    unsigned int size = powerOfTwoSize;
    size /= 2;
    std::vector<cl::Buffer> levels;
    while(size >= 4) {
        cl::Buffer level = cl::Buffer(device->getContext(), CL_MEM_READ_WRITE, size*size*size*sizeof(float));
        levels.push_back(level);
        size /= 2;
    }

    // Compile OpenCL code
    std::string buildOptions = "";
    switch(type) {
        case TYPE_FLOAT:
            buildOptions = "-DTYPE_FLOAT";
            break;
        case TYPE_UINT8:
            buildOptions = "-DTYPE_UINT8";
            break;
        case TYPE_INT8:
            buildOptions = "-DTYPE_INT8";
            break;
        case TYPE_UINT16:
            buildOptions = "-DTYPE_UINT16";
            break;
        case TYPE_INT16:
            buildOptions = "-DTYPE_INT16";
            break;
        case TYPE_UINT32:
            buildOptions = "-DTYPE_UINT32";
            break;
        case TYPE_INT32:
            buildOptions = "-DTYPE_INT32";
            break;
    }
    std::string sourceFilename = Config::getKernelSourcePath() + "/ImageSum.cl";
    std::string programName = sourceFilename + buildOptions;
    // Only create program if it doesn't exist for this device from before
    if(!device->hasProgram(programName))
        device->createProgramFromSourceWithName(programName, sourceFilename, buildOptions);
    cl::Program program = device->getProgram(programName);
    cl::CommandQueue queue = device->getCommandQueue();

    // Fill first level
    size = powerOfTwoSize/2;
    cl::Kernel firstLevel(program, "createFirstSumImage3DLevel");
    firstLevel.setArg(0, image);
    firstLevel.setArg(1, levels[0]);

    queue.enqueueNDRangeKernel(
            firstLevel,
            cl::NullRange,
            cl::NDRange(size,size,size),
            cl::NullRange
    );

    // Fill all other levels
    cl::Kernel createLevel(program, "createSumImage3DLevel");
    int i = 0;
    size /= 2;
    while(size >= 4) {
        createLevel.setArg(0, levels[i]);
        createLevel.setArg(1, levels[i+1]);
        queue.enqueueNDRangeKernel(
                createLevel,
                cl::NullRange,
                cl::NDRange(size,size,size),
                cl::NullRange
        );
        i++;
        size /= 2;
    }

    // Get result from the last level
    unsigned int nrOfElements = 4*4*4;
    unsigned int nrOfComponents = 1;
    float* result = (float*)allocateDataArray(nrOfElements,TYPE_FLOAT,nrOfComponents);
    queue.enqueueReadBuffer(levels[levels.size()-1],CL_TRUE, 0, nrOfElements*sizeof(float), result);
    *sum = getSumFromOpenCLImageResult<float>(result, nrOfElements, nrOfComponents);
    delete[] result;
}

void getIntensityStdDevFromOpenCLImage(OpenCLDevice::pointer device, cl::Image3D image, DataType type, float average, float* stddev) {
    // Get power of two size
    unsigned int powerOfTwoSize = getPowerOfTwoSize(
            std::max(std::max(image.getImageInfo<CL_IMAGE_WIDTH>(), image.getImageInfo<CL_IMAGE_HEIGHT>()), image.getImageInfo<CL_IMAGE_DEPTH>()));
    int totalNrOfElements = image.getImageInfo<CL_IMAGE_WIDTH>()*image.getImageInfo<CL_IMAGE_HEIGHT>()*image.getImageInfo<CL_IMAGE_DEPTH>();

    // Create image levels
    unsigned int size = powerOfTwoSize;
    size /= 2;
    std::vector<cl::Buffer> levels;
    while(size >= 4) {
        cl::Buffer level = cl::Buffer(device->getContext(), CL_MEM_READ_WRITE, size*size*size*sizeof(float));
        levels.push_back(level);
        size /= 2;
    }

    // Compile OpenCL code
    std::string buildOptions = "";
    switch(type) {
        case TYPE_FLOAT:
            buildOptions = "-DTYPE_FLOAT";
            break;
        case TYPE_UINT8:
            buildOptions = "-DTYPE_UINT8";
            break;
        case TYPE_INT8:
            buildOptions = "-DTYPE_INT8";
            break;
        case TYPE_UINT16:
            buildOptions = "-DTYPE_UINT16";
            break;
        case TYPE_INT16:
            buildOptions = "-DTYPE_INT16";
            break;
        case TYPE_UINT32:
            buildOptions = "-DTYPE_UINT32";
            break;
        case TYPE_INT32:
            buildOptions = "-DTYPE_INT32";
            break;
    }
    std::string sourceFilename = Config::getKernelSourcePath() + "/ImageSum.cl";
    std::string programName = sourceFilename + buildOptions;
    // Only create program if it doesn't exist for this device from before
    if(!device->hasProgram(programName))
        device->createProgramFromSourceWithName(programName, sourceFilename, buildOptions);
    cl::Program program = device->getProgram(programName);
    cl::CommandQueue queue = device->getCommandQueue();

    // Fill first level
    size = powerOfTwoSize/2;
    cl::Kernel firstLevel(program, "createFirstStdDevImage3DLevel");
    firstLevel.setArg(0, image);
    firstLevel.setArg(1, levels[0]);
    firstLevel.setArg(2, average);

    queue.enqueueNDRangeKernel(
            firstLevel,
            cl::NullRange,
            cl::NDRange(size,size,size),
            cl::NullRange
            );

    // Fill all other levels
    cl::Kernel createLevel(program, "createSumImage3DLevel");
    int i = 0;
    size /= 2;
    while(size >= 4) {
        createLevel.setArg(0, levels[i]);
        createLevel.setArg(1, levels[i+1]);
        queue.enqueueNDRangeKernel(
                createLevel,
                cl::NullRange,
                cl::NDRange(size,size,size),
                cl::NullRange
                );
        i++;
        size /= 2;
    }

    // Get result from the last level
    unsigned int nrOfElements = 4*4*4;
    unsigned int nrOfComponents = 1;
    float* result = (float*)allocateDataArray(nrOfElements,TYPE_FLOAT,nrOfComponents);
    queue.enqueueReadBuffer(levels[levels.size()-1],CL_TRUE, 0, nrOfElements*sizeof(float), result);
    *stddev = std::sqrt(getSumFromOpenCLImageResult<float>(result, nrOfElements, nrOfComponents)/totalNrOfElements);
    delete[] result;
}

void getIntensityStdDevFromOpenCLImage(OpenCLDevice::pointer device, cl::Image2D image, DataType type, float average, float* stddev) {
    // Get power of two size
    unsigned int powerOfTwoSize = getPowerOfTwoSize(std::max(image.getImageInfo<CL_IMAGE_WIDTH>(), image.getImageInfo<CL_IMAGE_HEIGHT>()));
    int totalNrOfElements = image.getImageInfo<CL_IMAGE_WIDTH>()*image.getImageInfo<CL_IMAGE_HEIGHT>();

    // Create image levels
    unsigned int size = powerOfTwoSize;
    size /= 2;
    std::vector<cl::Image2D> levels;
    while(size >= 4) {
        cl::Image2D level = cl::Image2D(device->getContext(), CL_MEM_READ_WRITE, getOpenCLImageFormat(device, CL_MEM_OBJECT_IMAGE2D, TYPE_FLOAT, 1), size, size);
        levels.push_back(level);
        size /= 2;
    }

    // Compile OpenCL code
    std::string buildOptions = "";
    switch(type) {
    case TYPE_FLOAT:
        buildOptions = "-DTYPE_FLOAT";
        break;
    case TYPE_UINT8:
        buildOptions = "-DTYPE_UINT8";
        break;
    case TYPE_INT8:
        buildOptions = "-DTYPE_INT8";
        break;
    case TYPE_UINT16:
        buildOptions = "-DTYPE_UINT16";
        break;
    case TYPE_INT16:
        buildOptions = "-DTYPE_INT16";
        break;
    case TYPE_UINT32:
        buildOptions = "-DTYPE_UINT32";
        break;
    case TYPE_INT32:
        buildOptions = "-DTYPE_INT32";
        break;
    }
    std::string sourceFilename = Config::getKernelSourcePath() + "/ImageSum.cl";
    std::string programName = sourceFilename + buildOptions;
    // Only create program if it doesn't exist for this device from before
    if(!device->hasProgram(programName))
        device->createProgramFromSourceWithName(programName, sourceFilename, buildOptions);
    cl::Program program = device->getProgram(programName);
    cl::CommandQueue queue = device->getCommandQueue();

    // Fill first level
    size = powerOfTwoSize/2;
    cl::Kernel firstLevel(program, "createFirstStdDevImage2DLevel");
    firstLevel.setArg(0, image);
    firstLevel.setArg(1, levels[0]);
    firstLevel.setArg(2, average);

    queue.enqueueNDRangeKernel(
            firstLevel,
            cl::NullRange,
            cl::NDRange(size,size),
            cl::NullRange
    );

    // Fill all other levels
    cl::Kernel createLevel(program, "createSumImage2DLevel");
    int i = 0;
    size /= 2;
    while(size >= 4) {
        createLevel.setArg(0, levels[i]);
        createLevel.setArg(1, levels[i+1]);
        queue.enqueueNDRangeKernel(
                createLevel,
                cl::NullRange,
                cl::NDRange(size,size),
                cl::NullRange
        );
        i++;
        size /= 2;
    }

    // Get result from the last level
    unsigned int nrOfElements = 4*4;
    unsigned int nrOfComponents = getOpenCLImageFormat(device, CL_MEM_OBJECT_IMAGE2D, TYPE_FLOAT, 1).image_channel_order == CL_RGBA ? 4 : 1;
    float* result = (float*)allocateDataArray(nrOfElements,TYPE_FLOAT,nrOfComponents);
    queue.enqueueReadImage(levels[levels.size()-1],CL_TRUE,createOrigoRegion(),createRegion(4,4,1),0,0,result);
    *stddev = std::sqrt(getSumFromOpenCLImageResult<float>(result, nrOfElements, nrOfComponents)/totalNrOfElements);
    delete[] result;
}



void getMaxAndMinFromOpenCLImage(OpenCLDevice::pointer device, cl::Image2D image, DataType type, float* min, float* max) {
    // Get power of two size
    unsigned int powerOfTwoSize = getPowerOfTwoSize(std::max(image.getImageInfo<CL_IMAGE_WIDTH>(), image.getImageInfo<CL_IMAGE_HEIGHT>()));

    // Create image levels
    unsigned int size = powerOfTwoSize;
    size /= 2;
    std::vector<cl::Image2D> levels;
    while(size >= 4) {
        cl::Image2D level = cl::Image2D(device->getContext(), CL_MEM_READ_WRITE, getOpenCLImageFormat(device, CL_MEM_OBJECT_IMAGE2D, type, 2), size, size);
        levels.push_back(level);
        size /= 2;
    }

    // Compile OpenCL code
    std::string buildOptions = "";
    switch(type) {
    case TYPE_FLOAT:
        buildOptions = "-DTYPE_FLOAT";
        break;
    case TYPE_UINT8:
        buildOptions = "-DTYPE_UINT8";
        break;
    case TYPE_INT8:
        buildOptions = "-DTYPE_INT8";
        break;
    case TYPE_UINT16:
        buildOptions = "-DTYPE_UINT16";
        break;
    case TYPE_INT16:
        buildOptions = "-DTYPE_INT16";
        break;
    case TYPE_UINT32:
        buildOptions = "-DTYPE_UINT32";
        break;
    case TYPE_INT32:
        buildOptions = "-DTYPE_INT32";
        break;
    }
    std::string sourceFilename = Config::getKernelSourcePath() + "/ImageMinMax.cl";
    std::string programName = sourceFilename + buildOptions;
    // Only create program if it doesn't exist for this device from before
    if(!device->hasProgram(programName))
        device->createProgramFromSourceWithName(programName, sourceFilename, buildOptions);
    cl::Program program = device->getProgram(programName);
    cl::CommandQueue queue = device->getCommandQueue();

    // Fill first level
    size = powerOfTwoSize/2;
    cl::Kernel firstLevel(program, "createFirstMinMaxImage2DLevel");
    firstLevel.setArg(0, image);
    firstLevel.setArg(1, levels[0]);

    queue.enqueueNDRangeKernel(
            firstLevel,
            cl::NullRange,
            cl::NDRange(size,size),
            cl::NullRange
    );

    // Fill all other levels
    cl::Kernel createLevel(program, "createMinMaxImage2DLevel");
    int i = 0;
    size /= 2;
    while(size >= 4) {
        createLevel.setArg(0, levels[i]);
        createLevel.setArg(1, levels[i+1]);
        queue.enqueueNDRangeKernel(
                createLevel,
                cl::NullRange,
                cl::NDRange(size,size),
                cl::NullRange
        );
        i++;
        size /= 2;
    }

    // Get result from the last level
    unsigned int nrOfElements = 4*4;
    unsigned int nrOfComponents = getOpenCLImageFormat(device, CL_MEM_OBJECT_IMAGE2D, type, 2).image_channel_order == CL_RGBA ? 4 : 2;
    void* result = allocateDataArray(nrOfElements,type,nrOfComponents);
    queue.enqueueReadImage(levels[levels.size()-1],CL_TRUE,createOrigoRegion(),createRegion(4,4,1),0,0,result);
    switch(type) {
    case TYPE_FLOAT:
        getMaxAndMinFromOpenCLImageResult<float>(result, nrOfElements, nrOfComponents, min, max);
        break;
    case TYPE_INT8:
        getMaxAndMinFromOpenCLImageResult<char>(result, nrOfElements, nrOfComponents, min, max);
        break;
    case TYPE_UINT8:
        getMaxAndMinFromOpenCLImageResult<uchar>(result, nrOfElements, nrOfComponents, min, max);
        break;
    case TYPE_INT16:
        getMaxAndMinFromOpenCLImageResult<short>(result, nrOfElements, nrOfComponents, min, max);
        break;
    case TYPE_UINT16:
        getMaxAndMinFromOpenCLImageResult<ushort>(result, nrOfElements, nrOfComponents, min, max);
        break;
    case TYPE_INT32:
        getMaxAndMinFromOpenCLImageResult<int>(result, nrOfElements, nrOfComponents, min, max);
        break;
    case TYPE_UINT32:
        getMaxAndMinFromOpenCLImageResult<uint>(result, nrOfElements, nrOfComponents, min, max);
        break;
    }
    deleteArray(result, type);
}

void getMaxAndMinFromOpenCLImage(OpenCLDevice::pointer device, cl::Image3D image, DataType type, float* min, float* max) {



   // Get power of two size
    unsigned int powerOfTwoSize = getPowerOfTwoSize(std::max(image.getImageInfo<CL_IMAGE_DEPTH>(), std::max(
            image.getImageInfo<CL_IMAGE_WIDTH>(),
            image.getImageInfo<CL_IMAGE_HEIGHT>())));

    // Create image levels
    unsigned int size = powerOfTwoSize;
    size /= 2;
    std::vector<cl::Image3D> levels;
    while(size >= 4) {
        cl::Image3D level = cl::Image3D(device->getContext(), CL_MEM_READ_WRITE, getOpenCLImageFormat(device, CL_MEM_OBJECT_IMAGE3D, type, 2), size, size, size);
        levels.push_back(level);
        size /= 2;
    }

    // Compile OpenCL code
    std::string buildOptions = "";
    switch(type) {
    case TYPE_FLOAT:
        buildOptions = "-DTYPE_FLOAT";
        break;
    case TYPE_UINT8:
        buildOptions = "-DTYPE_UINT8";
        break;
    case TYPE_INT8:
        buildOptions = "-DTYPE_INT8";
        break;
    case TYPE_UINT16:
        buildOptions = "-DTYPE_UINT16";
        break;
    case TYPE_INT16:
        buildOptions = "-DTYPE_INT16";
        break;
    case TYPE_UINT32:
        buildOptions = "-DTYPE_UINT32";
        break;
    case TYPE_INT32:
        buildOptions = "-DTYPE_INT32";
        break;
    }
    // Add fast_3d_image_writes flag if it is supported
    if(device->isWritingTo3DTexturesSupported()) {
        if(buildOptions.size() > 0)
            buildOptions += " ";
        buildOptions += "-Dfast_3d_image_writes";
    }
    std::string sourceFilename = Config::getKernelSourcePath() + "/ImageMinMax.cl";
    std::string programName = sourceFilename + buildOptions;
    // Only create program if it doesn't exist for this device from before
    if(!device->hasProgram(programName))
        device->createProgramFromSourceWithName(programName, sourceFilename, buildOptions);
    cl::Program program = device->getProgram(programName);
    cl::CommandQueue queue = device->getCommandQueue();

    // Fill first level
    size = powerOfTwoSize/2;
    cl::Kernel firstLevel(program, "createFirstMinMaxImage3DLevel");
    firstLevel.setArg(0, image);
    firstLevel.setArg(1, levels[0]);

    queue.enqueueNDRangeKernel(
            firstLevel,
            cl::NullRange,
            cl::NDRange(size,size,size),
            cl::NullRange
    );

    // Fill all other levels
    cl::Kernel createLevel(program, "createMinMaxImage3DLevel");
    int i = 0;
    size /= 2;
    while(size >= 4) {
        createLevel.setArg(0, levels[i]);
        createLevel.setArg(1, levels[i+1]);
        queue.enqueueNDRangeKernel(
                createLevel,
                cl::NullRange,
                cl::NDRange(size,size,size),
                cl::NullRange
        );
        i++;
        size /= 2;
    }

    // Get result from the last level
    unsigned int nrOfElements = 4*4*4;
    unsigned int nrOfComponents = getOpenCLImageFormat(device, CL_MEM_OBJECT_IMAGE3D, type, 2).image_channel_order == CL_RGBA ? 4 : 2;
    void* result = allocateDataArray(nrOfElements,type,nrOfComponents);
    queue.enqueueReadImage(levels[levels.size()-1],CL_TRUE,createOrigoRegion(),createRegion(4,4,4),0,0,result);
    switch(type) {
    case TYPE_FLOAT:
        getMaxAndMinFromOpenCLImageResult<float>(result, nrOfElements, nrOfComponents, min, max);
        break;
    case TYPE_INT8:
        getMaxAndMinFromOpenCLImageResult<char>(result, nrOfElements, nrOfComponents, min, max);
        break;
    case TYPE_UINT8:
        getMaxAndMinFromOpenCLImageResult<uchar>(result, nrOfElements, nrOfComponents, min, max);
        break;
    case TYPE_INT16:
        getMaxAndMinFromOpenCLImageResult<short>(result, nrOfElements, nrOfComponents, min, max);
        break;
    case TYPE_UINT16:
        getMaxAndMinFromOpenCLImageResult<ushort>(result, nrOfElements, nrOfComponents, min, max);
        break;
    case TYPE_INT32:
        getMaxAndMinFromOpenCLImageResult<int>(result, nrOfElements, nrOfComponents, min, max);
        break;
    case TYPE_UINT32:
        getMaxAndMinFromOpenCLImageResult<uint>(result, nrOfElements, nrOfComponents, min, max);
        break;
    }
    deleteArray(result, type);

}

void getMaxAndMinFromOpenCLBuffer(OpenCLDevice::pointer device, cl::Buffer buffer, unsigned int size, DataType type, float* min, float* max) {
    // Compile OpenCL code
    std::string buildOptions = "";
    switch(type) {
    case TYPE_FLOAT:
        buildOptions = "-DTYPE_FLOAT";
        break;
    case TYPE_UINT8:
        buildOptions = "-DTYPE_UINT8";
        break;
    case TYPE_INT8:
        buildOptions = "-DTYPE_INT8";
        break;
    case TYPE_UINT16:
        buildOptions = "-DTYPE_UINT16";
        break;
    case TYPE_INT16:
        buildOptions = "-DTYPE_INT16";
        break;
    case TYPE_UINT32:
        buildOptions = "-DTYPE_UINT32";
        break;
    case TYPE_INT32:
        buildOptions = "-DTYPE_INT32";
            break;
    }
    std::string sourceFilename = Config::getKernelSourcePath() + "/ImageMinMax.cl";
    std::string programName = sourceFilename + buildOptions;
    // Only create program if it doesn't exist for this device from before
    if(!device->hasProgram(programName))
        device->createProgramFromSourceWithName(programName, sourceFilename, buildOptions);
    cl::Program program = device->getProgram(programName);
    cl::CommandQueue queue = device->getCommandQueue();

    // Nr of work groups must be set so that work-group size does not exceed max work-group size (256 on AMD)
    int length = size;
    cl::Kernel reduce(program, "reduce");

    cl::Buffer current = buffer;
    cl::Buffer clResult;
    int workGroupSize = 256;
    int workGroups = 256;
    int X = ceil((float)length / (workGroups*workGroupSize));

    clResult = cl::Buffer(device->getContext(), CL_MEM_READ_WRITE, getSizeOfDataType(type,1)*workGroups*2);
    reduce.setArg(0, current);
    reduce.setArg(1, workGroupSize * getSizeOfDataType(type,1), NULL);
    reduce.setArg(2, workGroupSize * getSizeOfDataType(type,1), NULL);
    reduce.setArg(3, size);
    reduce.setArg(4, X);
    reduce.setArg(5, clResult);

    queue.enqueueNDRangeKernel(
            reduce,
            cl::NullRange,
            cl::NDRange(workGroups*workGroupSize),
            cl::NDRange(workGroupSize)
    );

    length = workGroups;

    void* result = allocateDataArray(length, type, 2);
    unsigned int nrOfElements = length;
    queue.enqueueReadBuffer(clResult,CL_TRUE,0,getSizeOfDataType(type,1)*workGroups*2,result);
    switch(type) {
    case TYPE_FLOAT:
        getMaxAndMinFromOpenCLImageResult<float>(result, nrOfElements, 2, min, max);
        break;
    case TYPE_INT8:
        getMaxAndMinFromOpenCLImageResult<char>(result, nrOfElements, 2, min, max);
        break;
    case TYPE_UINT8:
        getMaxAndMinFromOpenCLImageResult<uchar>(result, nrOfElements, 2, min, max);
        break;
    case TYPE_INT16:
        getMaxAndMinFromOpenCLImageResult<short>(result, nrOfElements, 2, min, max);
        break;
    case TYPE_UINT16:
        getMaxAndMinFromOpenCLImageResult<ushort>(result, nrOfElements, 2, min, max);
        break;
    case TYPE_INT32:
        getMaxAndMinFromOpenCLImageResult<int>(result, nrOfElements, 2, min, max);
        break;
    case TYPE_UINT32:
        getMaxAndMinFromOpenCLImageResult<uint>(result, nrOfElements, 2, min, max);
        break;
    }
    deleteArray(result, type);
}

cl::size_t<3> createRegion(unsigned int x, unsigned int y, unsigned int z) {
    cl::size_t<3> region;
    region[0] = x;
    region[1] = y;
    region[2] = z;
    return region;
}
cl::size_t<3> createRegion(Vector3ui size) {
    return createRegion(size.x(), size.y(), size.z());
}

cl::size_t<3> createOrigoRegion() {
    cl::size_t<3> region;
    region[0] = 0;
    region[1] = 0;
    region[2] = 0;
    return region;
}

std::string getCLErrorString(cl_int err) {
    switch (err) {
    case CL_SUCCESS:
        return std::string("Success!");
    case CL_DEVICE_NOT_FOUND:
        return std::string("Device not found.");
    case CL_DEVICE_NOT_AVAILABLE:
        return std::string("Device not available");
    case CL_COMPILER_NOT_AVAILABLE:
        return std::string("Compiler not available");
    case CL_MEM_OBJECT_ALLOCATION_FAILURE:
        return std::string("Memory object allocation failure");
    case CL_OUT_OF_RESOURCES:
        return std::string("Out of resources");
    case CL_OUT_OF_HOST_MEMORY:
        return std::string("Out of host memory");
    case CL_PROFILING_INFO_NOT_AVAILABLE:
        return std::string("Profiling information not available");
    case CL_MEM_COPY_OVERLAP:
        return std::string("Memory copy overlap");
    case CL_IMAGE_FORMAT_MISMATCH:
        return std::string("Image format mismatch");
    case CL_IMAGE_FORMAT_NOT_SUPPORTED:
        return std::string("Image format not supported");
    case CL_BUILD_PROGRAM_FAILURE:
        return std::string("Program build failure");
    case CL_MAP_FAILURE:
        return std::string("Map failure");
    case CL_INVALID_VALUE:
        return std::string("Invalid value");
    case CL_INVALID_DEVICE_TYPE:
        return std::string("Invalid device type");
    case CL_INVALID_PLATFORM:
        return std::string("Invalid platform");
    case CL_INVALID_DEVICE:
        return std::string("Invalid device");
    case CL_INVALID_CONTEXT:
        return std::string("Invalid context");
    case CL_INVALID_QUEUE_PROPERTIES:
        return std::string("Invalid queue properties");
    case CL_INVALID_COMMAND_QUEUE:
        return std::string("Invalid command queue");
    case CL_INVALID_HOST_PTR:
        return std::string("Invalid host pointer");
    case CL_INVALID_MEM_OBJECT:
        return std::string("Invalid memory object");
    case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
        return std::string("Invalid image format descriptor");
    case CL_INVALID_IMAGE_SIZE:
        return std::string("Invalid image size");
    case CL_INVALID_SAMPLER:
        return std::string("Invalid sampler");
    case CL_INVALID_BINARY:
        return std::string("Invalid binary");
    case CL_INVALID_BUILD_OPTIONS:
        return std::string("Invalid build options");
    case CL_INVALID_PROGRAM:
        return std::string("Invalid program");
    case CL_INVALID_PROGRAM_EXECUTABLE:
        return std::string("Invalid program executable");
    case CL_INVALID_KERNEL_NAME:
        return std::string("Invalid kernel name");
    case CL_INVALID_KERNEL_DEFINITION:
        return std::string("Invalid kernel definition");
    case CL_INVALID_KERNEL:
        return std::string("Invalid kernel");
    case CL_INVALID_ARG_INDEX:
        return std::string("Invalid argument index");
    case CL_INVALID_ARG_VALUE:
        return std::string("Invalid argument value");
    case CL_INVALID_ARG_SIZE:
        return std::string("Invalid argument size");
    case CL_INVALID_KERNEL_ARGS:
        return std::string("Invalid kernel arguments");
    case CL_INVALID_WORK_DIMENSION:
        return std::string("Invalid work dimension");
    case CL_INVALID_WORK_GROUP_SIZE:
        return std::string("Invalid work group size");
    case CL_INVALID_WORK_ITEM_SIZE:
        return std::string("Invalid work item size");
    case CL_INVALID_GLOBAL_OFFSET:
        return std::string("Invalid global offset");
    case CL_INVALID_EVENT_WAIT_LIST:
        return std::string("Invalid event wait list");
    case CL_INVALID_EVENT:
        return std::string("Invalid event");
    case CL_INVALID_OPERATION:
        return std::string("Invalid operation");
    case CL_INVALID_GL_OBJECT:
        return std::string("Invalid OpenGL object");
    case CL_INVALID_BUFFER_SIZE:
        return std::string("Invalid buffer size");
    case CL_INVALID_MIP_LEVEL:
        return std::string("Invalid mip-map level");
    case CL_KERNEL_ARG_INFO_NOT_AVAILABLE:
        return std::string("Kernel Arg info not available");
    default:
        return std::string("Unknown");
    }
}

std::string replace(std::string str, std::string find, std::string replacement) {
    while(true) {
        int pos = str.find(find);
        if(pos == std::string::npos)
            break;
        str.replace(pos, find.size(), replacement);
    }

    return str;
}

std::vector<std::string> split(const std::string input, const std::string& delimiter, float removeEmpty) {
    std::vector<std::string> parts;
    int startPos = 0;
    while(true) {
        int pos = input.find(delimiter, startPos);
        if(pos == std::string::npos) {
            parts.push_back(input.substr(startPos));
            break;
        }

        if(pos - startPos > 0) {
            parts.push_back(input.substr(startPos, pos - startPos));
        } else if(!removeEmpty) {
            parts.push_back("");
        }
        startPos = pos + delimiter.length();
    }

    return parts;
}

Matrix4f loadPerspectiveMatrix(float fovy, float aspect, float zNear, float zFar) {

    float theta = fovy*0.5f;
    float range = zFar - zNear;
    float invtan = 1.0f/(float)tan(theta);

    Matrix4f mProjectionMatrix = Matrix4f::Zero();
    mProjectionMatrix(0,0) = invtan / aspect;
    mProjectionMatrix(1,1) = invtan;
    mProjectionMatrix(2,2) = -(zNear + zFar) / range;
    mProjectionMatrix(3,2) = -1;
    mProjectionMatrix(2,3) = -2 * zNear * zFar / range;
    mProjectionMatrix(3,3) = 0;

    return mProjectionMatrix;
}


Matrix4f loadOrthographicMatrix(float left, float right, float bottom, float top, float zNear, float zFar) {
    // https://en.wikipedia.org/wiki/Orthographic_projection
    Matrix4f mProjectionMatrix = Matrix4f::Zero();
    mProjectionMatrix(0,0) = 2.0f / (right - left);
    mProjectionMatrix(1,1) = 2.0f / (top - bottom);
    mProjectionMatrix(2,2) = -2.0f / (zFar - zNear);
    mProjectionMatrix(0,3) = -(right + left) / (right - left);
    mProjectionMatrix(1,3) = -(top + bottom) / (top - bottom);
    mProjectionMatrix(2,3) = -(zFar + zNear) / (zFar - zNear);
    mProjectionMatrix(3,3) = 1;

    return mProjectionMatrix;
}

void createDirectory(std::string path) {
    int error = 0;
#if defined(_WIN32)
    error = _mkdir(path.c_str()); // can be used on Windows
#else
    mode_t nMode = 0733; // UNIX style permissions
    error = mkdir(path.c_str(), nMode); // can be used on non-Windows
    error = errno;
#endif
    if (error != 0) {
        if(fileExists(path)) {
            throw ExistException("Unable to create directory at " + path + ": Directory already exists.");
        } else if(error == ENOENT) {
            throw DoesNotExistException("Unable to create directory at " + path + ": Path was not found.");
        } else {
            throw Exception("Unable to create directory at " + path + ": Unknown error." + std::to_string(error));
        }
    }
}


void createDirectories(std::string path) {
    if(path.empty())
        return;
    // Replace \ with / so that this will work on windows
    path = replace(path, "\\", "/");
    std::vector<std::string> directories = split(path, "/");
    std::vector<std::string> filteredDirectories;

    // Fix any path with /../ in path
    for(int i = 0; i < directories.size(); ++i) {
        trim(directories[i]);
        if(directories[i] == "..") {
            // Pop previous
            filteredDirectories.pop_back();
        } else if(directories[i].size() > 0) {
            filteredDirectories.push_back(directories[i]);
        }
    }

    directories = filteredDirectories;
    std::string currentPath = "";
#ifdef WIN32
#else
    if(path[0] == '/')
        currentPath = "/";
#endif
    // Create each directory needed
    for(int i = 0; i < directories.size(); ++i) {
        currentPath += directories[i] + "/";
        try {
            createDirectory(currentPath);
        } catch(ExistException &e) {
            continue;
        } catch(Exception &e) {
            Reporter::warning() << e.what() << Reporter::end();
            continue;
        }
    }
}

bool fileExists(std::string filename) {
#ifdef _WIN32
    //return _access_s(filename.c_str(), 0) == 0;
    struct __stat64 stat_buf;
    return (_stat64(filename.c_str(), &stat_buf) == 0);
#else
    struct stat buffer;
    return (stat (filename.c_str(), &buffer) == 0);
#endif
}

bool isFile(const std::string& path) {
#ifdef _WIN32
    auto dwAttrib = GetFileAttributesA(path.c_str());

    return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
           !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat buf;
    stat(path.c_str(), &buf);
    return S_ISREG(buf.st_mode);
#endif
}

bool isDir(const std::string& path) {
#ifdef _WIN32
	auto ftyp = GetFileAttributesA(path.c_str());
	if(ftyp == INVALID_FILE_ATTRIBUTES)
		return false; 

	if(ftyp & FILE_ATTRIBUTE_DIRECTORY)
		return true;

	return false; 
#else
    struct stat buf;
    auto res = stat(path.c_str(), &buf);
    return res == 0 && S_ISDIR(buf.st_mode); // Exists and is directory
#endif
}

std::string join(std::string path) {
    // Remove all trailing /
    while(path[path.size()-1] == '/')
        path.substr(0, path.size()-1);
    return path;
}

static bool compare_nocase (const std::string& first, const std::string& second)
{
    unsigned int i=0;
    while((i<first.length()) && (i<second.length()) ) {
        if (tolower(first[i])<tolower(second[i])) return true;
        else if (tolower(first[i])>tolower(second[i])) return false;
        ++i;
    }
    return ( first.length() < second.length() );
}

std::vector<std::string> getDirectoryList(std::string path, bool getFiles, bool getDirectories) {
    if(path[path.size() - 1] != '/')
        path += '/';
    if(!getFiles && !getDirectories)
        throw Exception("getFiles and getDirectories set to false in getDirectoryList");
    std::vector<std::string> list;
#ifdef _WIN32
	HANDLE hFind;
	WIN32_FIND_DATA data;
	hFind = FindFirstFile((path + "*").c_str(), &data);
	if(hFind != INVALID_HANDLE_VALUE) {
		do {
			const std::string name = data.cFileName;
            if (name == "." || name == "..")
                continue;
            if(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if(getDirectories)
					list.push_back(name);
			} else if(getFiles) {
                list.push_back(name);
			}
		} while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
#else
    DIR *dir = opendir(path.c_str());
    if(dir != NULL) {
        struct dirent *ent;
        while ((ent = readdir (dir)) != NULL) {
            const std::string name = ent->d_name;
            if(getFiles && isFile(join(path, name))) // Regular file
                list.push_back(name);
            if(getDirectories && isDir(join(path, name))) { // Directory
                if(name == "." || name == "..")
                    continue;
                list.push_back(name);
            }
        }
        closedir(dir);
    } else {
        throw Exception("Unable to open directory " + path);
    }
#endif

    // Sort list
    std::sort(list.begin(), list.end(), compare_nocase);

    return list;
}

std::string getDirName(std::string path) {
    // Replace \ with / so that this will work on windows
    path = replace(path, "\\", "/");
    path.erase(std::find(path.rbegin(), path.rend(), '/').base(), path.end());
    return path;
}

std::string getFileName(std::string path) {
    // Replace \ with / so that this will work on windows
    path = replace(path, "\\", "/");
    auto pos = path.rfind('/');
    if(pos == std::string::npos)
        return path;
    return path.substr(pos+1);
}

std::string currentDateTime(std::string format) {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), format.c_str(), &tstruct);

    return buf;
}

std::string getModifiedDate(std::string filename) {
    std::string timeStr;
    #ifdef WIN32
        HANDLE hFile = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        FILETIME ftCreate, ftAccess, ftWrite;
        SYSTEMTIME sysTime;
        GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite);
        FileTimeToSystemTime(&ftWrite, &sysTime);
        char* buffer = new char[255];
        sprintf(buffer, "%hu%hu%hu%hu%hu%hu", sysTime.wYear, sysTime.wMonth, sysTime.wDay, sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
        timeStr = buffer;
        delete[] buffer;
    #else
        struct stat attrib; // create a file attribute structure
        stat(filename.c_str(), &attrib);
        timeStr = ctime(&(attrib.st_mtime));
    #endif
    trim(timeStr);
    return timeStr;
}

void extractZipFile(std::string zipFilepath, std::string destination) {
	int result = zip_extract(zipFilepath.c_str(), destination.c_str(), nullptr, nullptr);
    if(result != 0)
        throw Exception("Zip extraction failed");
}

std::string stringToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), 
                   [](unsigned char c){ return std::tolower(c); }
	);
    return s;
}
std::string stringToUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), 
                   [](unsigned char c){ return std::toupper(c); }
	);
    return s;
}

uint64_t fileSize(std::string filename) {
#ifdef WIN32
    struct __stat64 stat_buf;
    int rc = _stat64(filename.c_str(), &stat_buf);
#else
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
#endif
    if(rc != 0)
        throw Exception("Unable to get file size of " + filename);
    return stat_buf.st_size;
}

std::string generateRandomString(int length) {
    static auto& chrs = "0123456789"
                        "abcdefghijklmnopqrstuvwxyz"
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    thread_local static std::mt19937 rg{std::random_device{}()};
    thread_local static std::uniform_int_distribution<std::string::size_type> pick(0, sizeof(chrs) - 2);

    std::string s;

    s.reserve(length);

    while(length--)
        s += chrs[pick(rg)];

    return s;
}

std::string roundToString(double value, int decimals) {
    std::stringstream stream;
    stream << std::fixed << std::setprecision(decimals) << value;
    return stream.str();
}
std::string roundToString(float value, int decimals) {
    std::stringstream stream;
    stream << std::fixed << std::setprecision(decimals) << value;
    return stream.str();
}

Progress::Progress(uint64_t max, bool print, bool autoStart) {
    m_max = max;
    m_print = print;
    if(autoStart)
        start();
}

void Progress::start() {
    m_startTime = std::chrono::high_resolution_clock::now();
    m_started = true;
}

int getConsoleWidth() {
#ifdef WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int columns, rows;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return columns;
#else
        struct winsize w;
        int res = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        if(res < 0)
            return 80;
        if(w.ws_col == 0)
            return 80;
        if(w.ws_col > 512)
            return 80;
        return w.ws_col;
#endif
    }

void Progress::update(uint64_t current) {
    if(!m_started)
        start();
    m_current = current;

    const float percent = getPercent();
    std::chrono::duration<float, std::milli> duration = std::chrono::high_resolution_clock::now() - m_startTime;
    const float speed = (duration.count() / 1000.0f)/percent;
    const float ETA = (speed * (1.0f - percent) / 60.0f);
    if(m_print) {
        std::cout << "\r"; // Replace line
        std::stringstream ss;
        if(m_unitPrecision >= 0) {
            ss << std::setprecision(m_unitPrecision);
            ss << std::fixed;
        }
        ss << "] " << (int)(percent * 100.0f) << "%";
        if(!m_unit.empty()) {
            ss << " " << round(m_current*m_unitScale, m_unitPrecision) << m_unit << " / " << round(m_max*m_unitScale, m_unitPrecision) << m_unit;
        }
        if(ETA > 60) {
            ss << " | ETA " << round(ETA / 60.0f, 1) << " hours";
        } else if(ETA > 1) {
            ss << " | ETA " << round(ETA) << " mins";
        } else {
            ss << " | ETA " << std::floor(ETA * 60.0f) << " seconds";
        }
        std::string startString = m_text + (m_text.empty() ? "" : " ") + "[";
        const int totalWidth = getConsoleWidth();
        const int progressbarWidth = totalWidth - ss.str().size() - startString.size() - 1; // have to leave last line open to avoid jumping to next line (windows)
        std::cout << startString;
        int pos = progressbarWidth * percent;
        std::string bar;
        for (int i = 0; i < progressbarWidth; ++i) {
            if (i < pos) bar += "=";
            else if (i == pos) bar += ">";
            else bar += " ";
        }
        print(bar, ConsoleColor::GREEN, false, false);
        std::cout << ss.str();
        std::cout.flush();
        if(percent == 1.0f) {
            std::cout << "\n";
            std::cout.flush();
        }
    }
}

float Progress::getPercent() const {
    return (float)m_current / m_max;
}

uint64_t Progress::getCurrent() const {
    return m_current;
}

void Progress::setText(std::string text) {
    m_text = text;
}

void Progress::setMax(uint64_t max) {
    m_max = max;
}

void Progress::setUnit(std::string unit, float scale, int precision) {
    m_unit = unit;
    m_unitScale = scale;
    m_unitPrecision = precision;
}


#ifdef FAST_MODULE_VISUALIZATION
void downloadAndExtractZipFile(const std::string& URL, const std::string& destination, const std::string& name) {
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(QString::fromStdString(URL)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto timer = new QElapsedTimer;
    timer->start();
    auto reply = manager.get(request);
    Progress progress(100);
    progress.setText("Downloading");
    progress.setUnit("MB", 1.0/(1024*1024), 0);
    const int totalWidth = getConsoleWidth();
    std::string blank;
    for(int i = 0; i < totalWidth-1; ++i)
        blank += " ";
    QObject::connect(reply, &QNetworkReply::downloadProgress, [&](quint64 current, quint64 max) {
        progress.setMax(max);
        progress.update(current);
    });
    auto tempLocation = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + name.c_str() + ".zip";
    QFile file(tempLocation);
    if(!file.open(QIODevice::WriteOnly)) {
        throw Exception("Could not write to " + tempLocation.toStdString());
    }
    QObject::connect(reply, &QNetworkReply::readyRead, [&reply, &file]() {
        file.write(reply->read(reply->bytesAvailable()));
    });
    QObject::connect(&manager, &QNetworkAccessManager::finished, [blank, reply, &file, destination]() {
        std::cout << "\n";
        if(reply->error() != QNetworkReply::NoError) {
            std::cout << "\r" << blank;
            std::cout << "\rERROR: Download failed! : " << reply->errorString().toStdString() << std::endl;
            file.close();
            file.remove();
            return;
        }
        file.close();
        std::cout << "\r" << blank;
        std::cout << "\rExtracting zip file...";
        std::cout.flush();
        try {
            extractZipFile(file.fileName().toStdString(), destination);
        } catch(Exception& e) {
            std::cout << "\r" << blank;
            std::cout << "\rERROR: Zip extraction failed!" << std::endl;
            file.remove();
            return;
        }

        file.remove();
        std::cout << "\r" << blank;
        std::cout << "\rComplete." << std::endl;
    });

    auto eventLoop = new QEventLoop(&manager);

    // Make sure to quit the event loop when download is finished
    QObject::connect(&manager, &QNetworkAccessManager::finished, eventLoop, &QEventLoop::quit);

    // Wait for it to finish
    eventLoop->exec();
}

std::string getAbsolutePath(std::string path) {
#ifdef WIN32
    constexpr int BUFFER_SIZE = 4096;
    char pathBuffer[BUFFER_SIZE];
    char* file_part_ptr = NULL;
    DWORD length = GetFullPathNameA(
            path.c_str(),
            BUFFER_SIZE,
            pathBuffer,
            NULL
            );
    if(length == 0) {
        throw Exception("Unable to get absolute path of file " + path);
    } else if(length > BUFFER_SIZE) {
        throw Exception("Buffer too small, when getting absolute path of file " + path);
    }
    std::string result = path;
#else
    char pathBuffer[PATH_MAX];
    char* realPath = realpath(path.c_str(), pathBuffer);
    if(realPath == nullptr)
        throw Exception("Unable to get absolute path of file " + path);
    std::string result = pathBuffer;
#endif
    return result;
}

#else
static void downloadAndExtractZipFile(const std::string& URL, const std::string& destination) {
    throw NotImplementedException();
}
#endif

std::mutex& getGlobalPrintMutex() {
    static std::mutex mutex;
    return mutex;
}

struct PrintState {
    bool ansiSupported;
#ifdef WIN32
    HANDLE handle;
    WORD defaultAttributes;
#endif
};

PrintState initializePrint() {
    static bool initialized = false;
    static PrintState state;
    state.ansiSupported = false;
    if(!initialized) {
#ifdef WIN32
        state.handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if(!GetConsoleMode(state.handle, &mode)) {
            state.ansiSupported = false;
        } else {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            if(!SetConsoleMode(state.handle, mode)) {
                state.ansiSupported = false;
            } else {
                state.ansiSupported = true;
            }
        }

        if(!state.ansiSupported) {
            // Fallback
            CONSOLE_SCREEN_BUFFER_INFO Info;
            GetConsoleScreenBufferInfo(state.handle, &Info);
            state.defaultAttributes = Info.wAttributes & 0x00F0;
        }
#else
        state.ansiSupported = true;
#endif
    }
    return state;
}

void print(const std::string& text, ConsoleColor color, bool bold, bool addNewline) {
    std::lock_guard<std::mutex> lock(getGlobalPrintMutex());
    const auto state = initializePrint();
    static std::map<ConsoleColor, std::string> ANSIcolorCodes = {
            {ConsoleColor::RED, "31"},
            {ConsoleColor::GREEN, "32"},
            {ConsoleColor::YELLOW, "33"},
            {ConsoleColor::BLUE, "34"},
            {ConsoleColor::MAGENTA, "35"},
            {ConsoleColor::CYAN, "36"}
    };
#ifdef WIN32
    static std::map<ConsoleColor, WORD> fallbackColorCodes = {
            {ConsoleColor::RED, FOREGROUND_RED},
            {ConsoleColor::GREEN, FOREGROUND_GREEN},
            {ConsoleColor::YELLOW, FOREGROUND_RED | FOREGROUND_GREEN},
            {ConsoleColor::BLUE, FOREGROUND_BLUE},
            {ConsoleColor::MAGENTA, FOREGROUND_RED | FOREGROUND_BLUE},
            {ConsoleColor::CYAN, FOREGROUND_BLUE | FOREGROUND_GREEN}
    };
#endif

    // Set color and bold
    if(color != ConsoleColor::DEFAULT || bold) {
        if(state.ansiSupported) {
            std::string code = "\033[";
            if(color != ConsoleColor::DEFAULT)
                code += ANSIcolorCodes[color];
            if(bold) {
                if(color != ConsoleColor::DEFAULT)
                    code += ";";
                code += "1";
            }
            code += "m";
            std::cout << code;
        } else {
#ifdef WIN32
            auto currentAttributes = state.defaultAttributes | fallbackColorCodes[color];
            if(bold)
                currentAttributes |= FOREGROUND_INTENSITY;
            SetConsoleTextAttribute(state.handle, currentAttributes);
#endif
        }
    }

    // Print text
    std::cout << text;

    // Reset (if needed)
    if(color != ConsoleColor::DEFAULT || bold) {
        if(state.ansiSupported) {
            std::cout << "\033[0m";
        } else {
#ifdef WIN32
            SetConsoleTextAttribute(state.handle, state.defaultAttributes);
#endif
        }
    }

    if(addNewline)
        std::cout << std::endl;
}

} // end namespace fast
