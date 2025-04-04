#include "Image.hpp"
#include "FAST/Data/Access/ImageAccess.hpp"
#include "FAST/Utility.hpp"
#include "FAST/Exception.hpp"
#include "FAST/Utility.hpp"
#include "FAST/SceneGraph.hpp"
#include "FAST/Config.hpp"
#include <eigen3/unsupported/Eigen/CXX11/Tensor>
#ifdef FAST_MODULE_VISUALIZATION
#include <FAST/Visualization/Window.hpp>
#endif
#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenGL/gl.h>
#include <OpenCL/cl_gl.h>
#include <OpenGL/OpenGL.h>
#elif _WIN32
#include <GL/gl.h>
#include <CL/cl_gl.h>
#else
#include <GL/gl.h>
#include <CL/cl_gl.h>
#endif

namespace fast {

unique_pixel_ptr allocatePixelArray(std::size_t size, DataType type) {
    unique_pixel_ptr ptr;
    switch(type) {
        fastSwitchTypeMacro(ptr = make_unique_pixel<FAST_TYPE>(new FAST_TYPE[size]))
    }

    return ptr;
}

// Pad data with 1, 2 or 3 channels to 4 channels with 0
template <class T>
void * padData(T * data, unsigned int size, unsigned int nrOfChannels) {
    T * newData = new T[size*4]();
    for(unsigned int i = 0; i < size; i++) {
    	if(nrOfChannels == 1) {
            newData[i*4] = data[i];
    	} else if(nrOfChannels == 2) {
            newData[i*4] = data[i*2];
            newData[i*4+1] = data[i*2+1];
    	} else {
            newData[i*4] = data[i*3];
            newData[i*4+1] = data[i*3+1];
            newData[i*4+2] = data[i*3+2];
    	}
    }
    return (void *)newData;
}

const void * const adaptDataToImage(const void* const data, cl_channel_order order, unsigned int size, DataType type, unsigned int nrOfChannels) {
    // Because no OpenCL images support 3 channels,
    // the data has to be padded to 4 channels if the nr of channels is 3
    // Also, not all CL platforms support CL_R and CL_RG images
    if(order == CL_RGBA && nrOfChannels != 4) {
        switch(type) {
            fastSwitchTypeMacro(return padData<FAST_TYPE>((FAST_TYPE*)data, size, nrOfChannels))
        }
    }

    return data;
}

// Remove padding from a data array created by padData
template <class T>
void * removePadding(T * data, unsigned int size, unsigned int nrOfChannels) {
     T * newData = new T[size*nrOfChannels];
    for(unsigned int i = 0; i < size; i++) {
    	if(nrOfChannels == 1) {
            newData[i] = data[i*4];
    	} else if(nrOfChannels == 2) {
            newData[i*2] = data[i*4];
            newData[i*2+1] = data[i*4+1];
    	} else {
            newData[i*3] = data[i*4];
            newData[i*3+1] = data[i*4+1];
            newData[i*3+2] = data[i*4+2];
    	}
    }
    return (void *)newData;
}

unique_pixel_ptr adaptImageDataToHostData(unique_pixel_ptr data, cl_channel_order order, unsigned int size, DataType type, unsigned int nrOfChannels) {
    // Because no OpenCL images support 3 channels,
    // the data has to be padded to 4 channels if the nr of channels is 3.
    // Also, not all CL platforms support CL_R and CL_RG images
    // This function removes that padding
    if(order == CL_RGBA && nrOfChannels != 4) {
        switch(type) {
            fastSwitchTypeMacro(return unique_pixel_ptr(removePadding<FAST_TYPE>((FAST_TYPE*)data.get(), size, nrOfChannels), &pixel_deleter<FAST_TYPE>))
        }
    }

    return data;
}



void Image::transferCLImageFromHost(OpenCLDevice::pointer device) {

    // Special treatment for images with 3 channels because an OpenCL image can only have 1, 2 or 4 channels
	// And if the device does not support 1 or 2 channels
    cl::ImageFormat format = getOpenCLImageFormat(device, mDimensions == 2 ? CL_MEM_OBJECT_IMAGE2D : CL_MEM_OBJECT_IMAGE3D, mType, mChannels);
    if(format.image_channel_order == CL_RGBA && mChannels != 4) {
        auto tempData = adaptDataToImage(mHostData.get(), CL_RGBA, mWidth*mHeight*mDepth, mType, mChannels);
        device->getCommandQueue().enqueueWriteImage(*(cl::Image*)mCLImages[device],
        CL_TRUE, createOrigoRegion(), createRegion(mWidth, mHeight, mDepth), 0,
                0, (void*)tempData);
        deleteArray((void*)tempData, mType);
    } else {
        device->getCommandQueue().enqueueWriteImage(*(cl::Image*)mCLImages[device],
        CL_TRUE, createOrigoRegion(), createRegion(mWidth, mHeight, mDepth), 0,
                0, mHostData.get());
    }
}

void Image::transferCLImageToHost(OpenCLDevice::pointer device) {
    // Special treatment for images with 3 channels because an OpenCL image can only have 1, 2 or 4 channels
	// And if the device does not support 1 or 2 channels
    cl::ImageFormat format = getOpenCLImageFormat(device, mDimensions == 2 ? CL_MEM_OBJECT_IMAGE2D : CL_MEM_OBJECT_IMAGE3D, mType, mChannels);
    if(format.image_channel_order == CL_RGBA && mChannels != 4) {
        auto tempData = allocatePixelArray(mWidth*mHeight*mDepth*4, mType);
        device->getCommandQueue().enqueueReadImage(*(cl::Image*)mCLImages[device],
        CL_TRUE, createOrigoRegion(), createRegion(mWidth, mHeight, mDepth), 0,
                0, tempData.get());
        mHostData = adaptImageDataToHostData(std::move(tempData), CL_RGBA, mWidth*mHeight*mDepth,mType,mChannels);
    } else {
        if(!mHostHasData) {
            // Must allocate memory for host data
            mHostData = allocatePixelArray(mWidth*mHeight*mDepth*mChannels,mType);
			mHostHasData = true;
        }
        device->getCommandQueue().enqueueReadImage(*(cl::Image*)mCLImages[device],
        CL_TRUE, createOrigoRegion(), createRegion(mWidth, mHeight, mDepth), 0,
                0, mHostData.get());
    }
}

bool Image::hasAnyData() {
    return mHostHasData || mCLImages.size() > 0 || mCLBuffers.size() > 0;
}

void Image::updateOpenCLImageData(OpenCLDevice::pointer device) {

    // If data exist on device and is up to date do nothing
    if (mCLImagesIsUpToDate.count(device) > 0 && mCLImagesIsUpToDate[device]
            == true)
        return;

    bool updated = false;
    if (mCLImagesIsUpToDate.count(device) == 0) {
        // Data is not on device, create it
        cl::Image * newImage;
        if(mDimensions == 2) {
            newImage = new cl::Image2D(device->getContext(),
            CL_MEM_READ_WRITE, getOpenCLImageFormat(device, CL_MEM_OBJECT_IMAGE2D, mType,mChannels), mWidth, mHeight);
        } else {
            newImage = new cl::Image3D(device->getContext(),
            CL_MEM_READ_WRITE, getOpenCLImageFormat(device, CL_MEM_OBJECT_IMAGE3D, mType,mChannels), mWidth, mHeight, mDepth);
        }

        if(hasAnyData()) {
            mCLImagesIsUpToDate[device] = false;
        } else {
            mCLImagesIsUpToDate[device] = true;
            updated = true;
        }
        mCLImages[device] = newImage;
    }

    // Find which data is up to date
	if (!mCLImagesIsUpToDate[device]) {
		if (mHostDataIsUpToDate) {
			// Transfer host data to this device
			transferCLImageFromHost(device);
			updated = true;
		} else {
			std::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
			for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end();
				it++) {
				if (it->second == true) {
					// Transfer from this device(it->first) to device
					// TODO should use copy image to image here, if possible
					transferCLImageToHost(it->first);
					transferCLImageFromHost(device);
					mHostDataIsUpToDate = true;
					updated = true;
					break;
				}
			}
			for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end();
				it++) {
				if (it->second == true) {
					// Transfer from this device(it->first) to device
					// TODO should use copy buffer to image here, if possible
					transferCLBufferToHost(it->first);
					transferCLImageFromHost(device);
					mHostDataIsUpToDate = true;
					updated = true;
					break;
				}
			}
		}
	}

    if (!updated)
        throw Exception(
                "Data was not updated because no data was marked as up to date");
}

OpenCLBufferAccess::pointer Image::getOpenCLBufferAccess(
        accessType type,
        OpenCLDevice::pointer device) {

    if(!isInitialized())
        throw Exception("Image has not been initialized.");

    blockIfBeingWrittenTo();

    if(type == ACCESS_READ_WRITE) {
    	blockIfBeingAccessed();
        std::unique_lock<std::mutex> lock(mDataIsBeingWrittenToMutex);
        mDataIsBeingWrittenTo = true;
    }
    updateOpenCLBufferData(device);
    if(type == ACCESS_READ_WRITE) {
        setAllDataToOutOfDate();
        updateModifiedTimestamp();
    }
    mCLBuffersIsUpToDate[device] = true;
    {
        std::unique_lock<std::mutex> lock(mDataIsBeingAccessedMutex);
        mDataIsBeingAccessed = true;
    }

    // Now it is guaranteed that the data is on the device and that it is up to date
	OpenCLBufferAccess::pointer accessObject(new OpenCLBufferAccess(mCLBuffers[device],  std::static_pointer_cast<Image>(mPtr.lock())));
	return std::move(accessObject);
}

unsigned int Image::getBufferSize() const {
    unsigned int bufferSize = mWidth*mHeight;
    if(mDimensions == 3) {
        bufferSize *= mDepth;
    }
    bufferSize *= getSizeOfDataType(mType,mChannels);

    return bufferSize;
}

void Image::updateOpenCLBufferData(OpenCLDevice::pointer device) {

    // If data exist on device and is up to date do nothing
    if (mCLBuffersIsUpToDate.count(device) > 0 && mCLBuffersIsUpToDate[device]
            == true)
        return;

    bool updated = false;
    if (mCLBuffers.count(device) == 0) {
        // Data is not on device, create it
        unsigned int bufferSize = getBufferSize();
        cl::Buffer * newBuffer = new cl::Buffer(device->getContext(),
        CL_MEM_READ_WRITE, bufferSize);

        if(hasAnyData()) {
            mCLBuffersIsUpToDate[device] = false;
        } else {
            mCLBuffersIsUpToDate[device] = true;
            updated = true;
        }
        mCLBuffers[device] = newBuffer;
    }


    // Find which data is up to date
    if(!mCLBuffersIsUpToDate[device]) {
        if (mHostDataIsUpToDate) {
            // Transfer host data to this device
            transferCLBufferFromHost(device);
            updated = true;
        } else {
            std::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
            for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end();
                    it++) {
                if (it->second == true) {
                    // Transfer from this device(it->first) to device
                    transferCLImageToHost(it->first);
                    transferCLBufferFromHost(device);
                    mHostDataIsUpToDate = true;
                    updated = true;
                    break;
                }
            }
            for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end();
                    it++) {
                if (it->second == true) {
                    // Transfer from this device(it->first) to device
                    transferCLBufferToHost(it->first);
                    transferCLBufferFromHost(device);
                    mHostDataIsUpToDate = true;
                    updated = true;
                    break;
                }
            }
        }
    }

    if (!updated)
        throw Exception(
                "Data was not updated because no data was marked as up to date");

    mCLBuffersIsUpToDate[device] = true;
}

void Image::transferCLBufferFromHost(OpenCLDevice::pointer device) {
    unsigned int bufferSize = getBufferSize();
    device->getCommandQueue().enqueueWriteBuffer(*mCLBuffers[device],
        CL_TRUE, 0, bufferSize, mHostData.get());
}

void Image::transferCLBufferToHost(OpenCLDevice::pointer device) {
	if (!mHostHasData) {
		// Must allocate memory for host data
		mHostData = allocatePixelArray(mWidth*mHeight*mDepth*mChannels, mType);
		mHostHasData = true;
	}
    unsigned int bufferSize = getBufferSize();
    device->getCommandQueue().enqueueReadBuffer(*mCLBuffers[device],
        CL_TRUE, 0, bufferSize, mHostData.get());
}

void Image::updateHostData() {
    // It is the host data that has been modified, no need to update
    if (mHostDataIsUpToDate)
        return;

    bool updated = false;
    if (!mHostHasData) {
        // Data is not initialized, do that first
        unsigned int size = mWidth*mHeight*mChannels;
        if(mDimensions == 3)
            size *= mDepth;
        mHostData = allocatePixelArray(mWidth*mHeight*mDepth*mChannels,mType);
        if(hasAnyData()) {
            mHostDataIsUpToDate = false;
        } else {
            mHostDataIsUpToDate = true;
            updated = true;
        }
        mHostHasData = true;
    }

    // Find which data is up to date
    std::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
    for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end();
            it++) {
        if (it->second == true) {
            // transfer from this device to host
            transferCLImageToHost(it->first);
            updated = true;
            break;
        }
    }
    for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end();
            it++) {
        if (it->second == true) {
            // transfer from this device to host
            transferCLBufferToHost(it->first);
            updated = true;
            break;
        }
    }
    if (!updated)
        throw Exception(
                "Data was not updated because no data was marked as up to date");
}

void Image::setAllDataToOutOfDate() {
    mHostDataIsUpToDate = false;
    m_GLtextureUpToDate = false;
    std::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
    for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end();
            it++) {
        it->second = false;
    }
    for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end();
            it++) {
        it->second = false;
    }
}

OpenCLImageAccess::pointer Image::getOpenCLImageAccess(
        accessType type,
        OpenCLDevice::pointer device) {

    if(!isInitialized())
        throw Exception("Image has not been initialized.");

    blockIfBeingWrittenTo();

    // Check for write access
    if(type == ACCESS_READ_WRITE) {
    	blockIfBeingAccessed();
    	std::lock_guard<std::mutex> lock(mDataIsBeingWrittenToMutex);
        mDataIsBeingWrittenTo = true;
    }
    updateOpenCLImageData(device);
    if (type == ACCESS_READ_WRITE) {
        setAllDataToOutOfDate();
        updateModifiedTimestamp();
    }
    {
        std::lock_guard<std::mutex> lock(mDataIsBeingAccessedMutex);
        mDataIsBeingAccessed = true;
    }
    mCLImagesIsUpToDate[device] = true;

    // Now it is guaranteed that the data is on the device and that it is up to date
    if(mDimensions == 2) {
        OpenCLImageAccess::pointer accessObject(new OpenCLImageAccess((cl::Image2D*)mCLImages[device], std::static_pointer_cast<Image>(mPtr.lock())));
        return accessObject;
    } else {
        OpenCLImageAccess::pointer accessObject(new OpenCLImageAccess((cl::Image3D*)mCLImages[device], std::static_pointer_cast<Image>(mPtr.lock())));
        return accessObject;
    }
}

Image::Image() {
    mHostHasData = false;
    mHostDataIsUpToDate = false;
    mSpacing = Vector3f(1,1,1);
    mMaxMinInitialized = false;
    mSumInitialized = false;
    mIsInitialized = false;
}

ImageAccess::pointer Image::getImageAccess(accessType type) {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");

    blockIfBeingWrittenTo();

    if(type == ACCESS_READ_WRITE) {
    	blockIfBeingAccessed();
        std::unique_lock<std::mutex> lock(mDataIsBeingWrittenToMutex);
        mDataIsBeingWrittenTo = true;
    }
    updateHostData();
    if(type == ACCESS_READ_WRITE) {
        setAllDataToOutOfDate();
        updateModifiedTimestamp();
    }
    mHostDataIsUpToDate = true;
    {
        std::unique_lock<std::mutex> lock(mDataIsBeingAccessedMutex);
        mDataIsBeingAccessed = true;
    }

	ImageAccess::pointer accessObject(new ImageAccess(mHostData.get(), std::static_pointer_cast<Image>(mPtr.lock())));
	return std::move(accessObject);
}

Image::Image(
        VectorXui size,
        DataType type,
        unsigned int nrOfChannels) : Image() {

    if(size.rows() > 2 && size.z() > 1) {
        // 3D
        init(size.x(), size.y(), size.z(), type, nrOfChannels);
    } else {
        // 2D
        init(size.x(), size.y(), type, nrOfChannels);
    }
}

Image::Image(
        VectorXui size,
        DataType type,
        unsigned int nrOfChannels,
        ExecutionDevice::pointer device,
        const void* const data) : Image() {

    if(size.rows() > 2 && size.z() > 1) {
        // 3D
        init(size.x(), size.y(), size.z(), type, nrOfChannels);
    } else {
        // 2D
        init(size.x(), size.y(), type, nrOfChannels);
    }
    copyData(device, data);
}

Image::Image(
        VectorXui size,
        DataType type,
        unsigned int nrOfChannels,
        const void* const data) : Image() {

    if(size.rows() > 2 && size.z() > 1) {
        // 3D
        init(size.x(), size.y(), size.z(), type, nrOfChannels);
    } else {
        // 2D
        init(size.x(), size.y(), type, nrOfChannels);
    }
    copyData(Host::getInstance(), data);
}


Image::Image(
        unsigned int width,
        unsigned int height,
        unsigned int depth,
        DataType type,
        unsigned int nrOfChannels) : Image() {
    init(width, height, depth, type, nrOfChannels);
}

void Image::init(
        unsigned int width,
        unsigned int height,
        unsigned int depth,
        DataType type,
        unsigned int nrOfChannels) {
    getSceneGraphNode()->reset(); // reset scene graph node
    freeAll(); // delete any old data

    mWidth = width;
    mHeight = height;
    mDepth = depth;
    mBoundingBox = DataBoundingBox(Vector3f(width, height, depth));
    mDimensions = 3;
    mType = type;
    mChannels = nrOfChannels;
    updateModifiedTimestamp();
    mIsInitialized = true;
}


Image::Image(
        unsigned int width,
        unsigned int height,
        unsigned int depth,
        DataType type,
        unsigned int nrOfChannels,
        ExecutionDevice::pointer device,
        const void* const data) : Image() {

    init(width, height, depth, type, nrOfChannels);
    copyData(device, data);
}

Image::Image(
        unsigned int width,
        unsigned int height,
        unsigned int depth,
        DataType type,
        unsigned int nrOfChannels,
        const void* const data) : Image() {

	init(width, height, depth, type, nrOfChannels);
	copyData(Host::getInstance(), data);
}

Image::Image(
        unsigned int width,
        unsigned int height,
        DataType type,
        unsigned int nrOfChannels
        ) : Image() {
    init(width, height, type, nrOfChannels);
}

void Image::init(
        unsigned int width,
        unsigned int height,
        DataType type,
        unsigned int nrOfChannels
        ) {
    getSceneGraphNode()->reset(); // reset scene graph node
    freeAll(); // delete any old data

    mWidth = width;
    mHeight = height;
    mBoundingBox = DataBoundingBox(Vector3f(width, height, 0));
    mDepth = 1;
    mDimensions = 2;
    mType = type;
    mChannels = nrOfChannels;
    updateModifiedTimestamp();
    mIsInitialized = true;
}

void Image::init(VectorXui size, DataType type, uint nrOfChannels) {
    if(size.size() == 2 || size.z() == 1) {
        init(size.x(), size.y(), type, nrOfChannels);
    } else {
        init(size.x(), size.y(), size.z(), type, nrOfChannels);
    }
}

Image::Image(
        unsigned int width,
        unsigned int height,
        DataType type,
        unsigned int nrOfChannels,
        ExecutionDevice::pointer device,
        const void* const data) : Image() {

    init(width, height, type, nrOfChannels);

    copyData(device, data);

}

Image::Image(
        unsigned int width,
        unsigned int height,
        DataType type,
        unsigned int nrOfChannels,
        const void* const data) : Image() {

	init(width, height, type, nrOfChannels);
	copyData(Host::getInstance(), data);
}

void Image::copyData(ExecutionDevice::pointer device, const void* const data) {
    if(!mIsInitialized)
        throw Exception("Image must be initialized");
    // We do not own this pointer, have to copy it
    if(device->isHost()) {
        mHostData = allocatePixelArray(mWidth*mHeight*mDepth*mChannels, mType);
        std::memcpy(mHostData.get(), data, getSizeOfDataType(mType, mChannels) * mWidth * mHeight * mDepth);
        mHostHasData = true;
        mHostDataIsUpToDate = true;
        updateModifiedTimestamp();
        mIsInitialized = true;
    } else {
        OpenCLDevice::pointer clDevice = std::static_pointer_cast<OpenCLDevice>(device);
        cl::Image* clImage;
        void* tempData;
        if(mDimensions == 2) {
            tempData = (void*)adaptDataToImage(data, getOpenCLImageFormat(clDevice, CL_MEM_OBJECT_IMAGE2D, mType,
                                                                         mChannels).image_channel_order,
                                              mWidth * mHeight, mType, mChannels);
            clImage = new cl::Image2D(
                    clDevice->getContext(),
                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                    getOpenCLImageFormat(clDevice, CL_MEM_OBJECT_IMAGE2D, mType, mChannels),
                    mWidth, mHeight,
                    0,
                    tempData
            );
        } else {
            tempData = (void*)adaptDataToImage(data, getOpenCLImageFormat(clDevice, CL_MEM_OBJECT_IMAGE3D, mType, mChannels).image_channel_order, mWidth*mHeight*mDepth, mType, mChannels);
            clImage = new cl::Image3D(
                clDevice->getContext(),
                CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                getOpenCLImageFormat(clDevice, CL_MEM_OBJECT_IMAGE3D, mType, mChannels),
                mWidth, mHeight, mDepth,
                0, 0,
                tempData
            );
        }
        mCLImages[clDevice] = clImage;
        mCLImagesIsUpToDate[clDevice] = true;
        if(tempData != data) // If a new copy was made, delete it
            deleteArray(tempData, mType);
    }
}

void Image::setData(ExecutionDevice::pointer device, void* data) {
    // We own data pointer, do what we want
    if(!mIsInitialized)
        throw Exception("Image must be initialized");

    if(device->isHost()) {
        // Since we own the data pointer, we can put it in an unique_ptr:
        switch(mType) {
            fastSwitchTypeMacro(mHostData = make_unique_pixel<FAST_TYPE>((FAST_TYPE*)data))
        }
        mHostHasData = true;
        mHostDataIsUpToDate = true;
    } else {
        // For OpenCL we have to do a copy
        copyData(device, data);
        deleteArray(data, mType); // data was copied, have to delete it
    }
    updateModifiedTimestamp();
    mIsInitialized = true;
}

bool Image::isInitialized() const {
    return mIsInitialized;
}

void Image::free(ExecutionDevice::pointer device) {
    // Delete data on a specific device
    if(device->isHost()) {
        mHostData.reset();
        mHostHasData = false;
    } else {
        OpenCLDevice::pointer clDevice = std::static_pointer_cast<OpenCLDevice>(device);
        // Delete any OpenCL images
        delete mCLImages[clDevice];
        mCLImages.erase(clDevice);
        mCLImagesIsUpToDate.erase(clDevice);
        // Delete any OpenCL buffers
        delete mCLBuffers[clDevice];
        mCLBuffers.erase(clDevice);
        mCLBuffersIsUpToDate.erase(clDevice);
    }
}

void Image::freeAll() {
    // Delete OpenCL Images
    std::unordered_map<OpenCLDevice::pointer, cl::Image*>::iterator it;
    for (it = mCLImages.begin(); it != mCLImages.end(); it++) {
        delete it->second;
    }
    mCLImages.clear();
    mCLImagesIsUpToDate.clear();

    // Delete OpenCL buffers
    std::unordered_map<OpenCLDevice::pointer, cl::Buffer*>::iterator it2;
    for (it2 = mCLBuffers.begin(); it2 != mCLBuffers.end(); it2++) {
        delete it2->second;
    }
    mCLBuffers.clear();
    mCLBuffersIsUpToDate.clear();

    // Delete host data
    if(mHostHasData) {
        this->free(Host::getInstance());
    }
    if(m_GLtextureID > 0) {
        // We have to have current GL context to delete it
        if(QOpenGLContext::currentContext() == nullptr) {
            reportWarning() << "Unable to delete texture because no OpenGL context was current" << reportEnd();
        } else {
            glDeleteTextures(1, &m_GLtextureID);
        }
        m_GLtextureID = 0;
        m_GLtextureUpToDate = false;
    }
}

int Image::getWidth() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return mWidth;
}

int Image::getHeight() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return mHeight;
}

int Image::getDepth() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return mDepth;
}

Vector3ui Image::getSize() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return Vector3ui(mWidth, mHeight, mDepth);
}

unsigned char Image::getDimensions() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return mDimensions;
}

DataType Image::getDataType() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return mType;
}

int Image::getNrOfChannels() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return mChannels;
}

Vector3f fast::Image::getSpacing() const {
    return mSpacing;
}

void fast::Image::setSpacing(Vector3f spacing) {
    mSpacing = spacing;
}

void fast::Image::setSpacing(float x, float y, float z) {
	setSpacing(Vector3f(x, y, z));
}

void Image::calculateMaxAndMinIntensity() {
    // Calculate max and min if image has changed or it is the first time
    if(!mMaxMinInitialized || mMaxMinTimestamp != getTimestamp()) {

        unsigned int nrOfElements = mWidth*mHeight*mDepth*mChannels;
        if(mHostHasData && mHostDataIsUpToDate) {
            // Host data is up to date, calculate min and max on host
            ImageAccess::pointer access = getImageAccess(ACCESS_READ);
            void* data = access->get();
            switch(mType) {
            case TYPE_FLOAT:
            case TYPE_SNORM_INT16:
            case TYPE_UNORM_INT16:
                getMaxAndMinFromData<float>(data,nrOfElements,&mMinimumIntensity,&mMaximumIntensity);
                break;
            case TYPE_INT8:
                getMaxAndMinFromData<char>(data,nrOfElements,&mMinimumIntensity,&mMaximumIntensity);
                break;
            case TYPE_UINT8:
                getMaxAndMinFromData<uchar>(data,nrOfElements,&mMinimumIntensity,&mMaximumIntensity);
                break;
            case TYPE_INT16:
                getMaxAndMinFromData<short>(data,nrOfElements,&mMinimumIntensity,&mMaximumIntensity);
                break;
            case TYPE_UINT16:
                getMaxAndMinFromData<ushort>(data,nrOfElements,&mMinimumIntensity,&mMaximumIntensity);
                break;
            case TYPE_INT32:
                getMaxAndMinFromData<int>(data,nrOfElements,&mMinimumIntensity,&mMaximumIntensity);
                break;
            case TYPE_UINT32:
                getMaxAndMinFromData<uint>(data,nrOfElements,&mMinimumIntensity,&mMaximumIntensity);
                break;
            }
        } else {
            // TODO the logic here can be improved. For instance choose the best device
            // Find some OpenCL image data or buffer data that is up to date
            bool found = false;
            std::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
            for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end(); it++) {
                if(it->second == true) {
                    OpenCLDevice::pointer device = it->first;
                    if(mDimensions == 2) {
                        OpenCLImageAccess::pointer access = getOpenCLImageAccess(ACCESS_READ, device);
                        cl::Image2D* clImage = access->get2DImage();
                        getMaxAndMinFromOpenCLImage(device, *clImage, mType, &mMinimumIntensity, &mMaximumIntensity);
                    } else {
                        if(!device->isWritingTo3DTexturesSupported()) {
                            // Writing to 3D images is not supported on this device
                            // Copy data to buffer instead and do the max min calculation on the buffer instead
                            OpenCLBufferAccess::pointer access = getOpenCLBufferAccess(ACCESS_READ, device);
                            cl::Buffer* buffer = access->get();
                            getMaxAndMinFromOpenCLBuffer(device, *buffer, nrOfElements, mType, &mMinimumIntensity, &mMaximumIntensity);
                        } else {
                            OpenCLImageAccess::pointer access = getOpenCLImageAccess(ACCESS_READ, device);
                            cl::Image3D* clImage = access->get3DImage();
                            getMaxAndMinFromOpenCLImage(device, *clImage, mType, &mMinimumIntensity, &mMaximumIntensity);
                        }
                    }
                    found = true;
                }
            }

            if(!found) {
                for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end(); it++) {
                    if(it->second == true) {
                        OpenCLDevice::pointer device = it->first;
                        OpenCLBufferAccess::pointer access = getOpenCLBufferAccess(ACCESS_READ, device);
                        cl::Buffer* buffer = access->get();
                        getMaxAndMinFromOpenCLBuffer(device, *buffer, nrOfElements, mType, &mMinimumIntensity, &mMaximumIntensity);
                        found = true;
                    }
                }
            }
            if(!found)
                throw Exception(("Can't calculate max/min intensity of image because pixel data has not been initialized."));
        }

        // Update timestamp
        mMaxMinTimestamp = getTimestamp();
        mMaxMinInitialized = true;
    }
}

float Image::calculateSumIntensity() {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");

    // Calculate sum if image has changed or it is the first time
    if(!mSumInitialized || mSumIntensityTimestamp != getTimestamp()) {
        unsigned int nrOfElements = mWidth*mHeight*mDepth;
        if((mHostHasData && mHostDataIsUpToDate) || getNrOfVoxels() < 256) {
            // Host data is up to date, or image is very small, calculate min and max on host
            ImageAccess::pointer access = getImageAccess(ACCESS_READ);
            void* data = access->get();
            switch(mType) {
                case TYPE_FLOAT:
                    mSumIntensity = getSumFromData<float>(data,nrOfElements);
                    break;
                case TYPE_INT8:
                    mSumIntensity = getSumFromData<char>(data,nrOfElements);
                    break;
                case TYPE_UINT8:
                    mSumIntensity = getSumFromData<uchar>(data,nrOfElements);
                    break;
                case TYPE_INT16:
                    mSumIntensity = getSumFromData<short>(data,nrOfElements);
                    break;
                case TYPE_UINT16:
                    mSumIntensity = getSumFromData<ushort>(data,nrOfElements);
                    break;
                case TYPE_INT32:
                    mSumIntensity = getSumFromData<int>(data,nrOfElements);
                    break;
                case TYPE_UINT32:
                    mSumIntensity = getSumFromData<uint>(data,nrOfElements);
                    break;
            }
        } else {
            // TODO the logic here can be improved. For instance choose the best device
            // Find some OpenCL image data or buffer data that is up to date
            bool found = false;
            std::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
            for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end(); it++) {
                if(it->second == true) {
                    OpenCLDevice::pointer device = it->first;
                    float sum;
                    auto access = getOpenCLImageAccess(ACCESS_READ, device);
                    if(mDimensions == 2) {
                        auto clImage = access->get2DImage();
                        getIntensitySumFromOpenCLImage(device, *clImage, mType, &sum);
                    } else {
                        auto clImage = access->get3DImage();
                        getIntensitySumFromOpenCLImage(device, *clImage, mType, &sum);
                    }
                    mSumIntensity = sum;
                    found = true;
                }
            }

            if(!found) {
                for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end(); it++) {
                    if(it->second == true) {
                        // TODO FIXME, ideally we should use the Buffers directly here, here we get a copy from image to buffer
                        OpenCLDevice::pointer device = it->first;
                        float sum;
                        auto access = getOpenCLImageAccess(ACCESS_READ, device);
                        if(mDimensions == 2) {
                            auto clImage = access->get2DImage();
                            getIntensitySumFromOpenCLImage(device, *clImage, mType, &sum);
                        } else {
                            auto clImage = access->get3DImage();
                            getIntensitySumFromOpenCLImage(device, *clImage, mType, &sum);
                        }
                        mSumIntensity = sum;
                    }
                }
            }
        }

        // Update timestamp
        mSumIntensityTimestamp = getTimestamp();
        mSumInitialized = true;
    }

    return mSumIntensity;
}

float Image::calculateStandardDeviationIntensity() {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");

    float average = calculateAverageIntensity();

    // Calculate sum if image has changed or it is the first time
    if(!mStdDevInitialized || mStdDevIntensityTimestamp != getTimestamp()) {
        unsigned int nrOfElements = mWidth*mHeight*mDepth;
        if((mHostHasData && mHostDataIsUpToDate) || getNrOfVoxels() < 256) {
            // Host data is up to date, or image is very small, calculate min and max on host
            auto access = getImageAccess(ACCESS_READ);
            void* data = access->get();
            switch(mType) {
                case TYPE_FLOAT:
                    mStdDevIntensity = getStandardDeviationFromData<float>(data,nrOfElements);
                    break;
                case TYPE_INT8:
                    mStdDevIntensity = getStandardDeviationFromData<char>(data,nrOfElements);
                    break;
                case TYPE_UINT8:
                    mStdDevIntensity = getStandardDeviationFromData<uchar>(data,nrOfElements);
                    break;
                case TYPE_INT16:
                    mStdDevIntensity = getStandardDeviationFromData<short>(data,nrOfElements);
                    break;
                case TYPE_UINT16:
                    mStdDevIntensity = getStandardDeviationFromData<ushort>(data,nrOfElements);
                    break;
                case TYPE_INT32:
                    mStdDevIntensity = getStandardDeviationFromData<int>(data,nrOfElements);
                    break;
                case TYPE_UINT32:
                    mStdDevIntensity = getStandardDeviationFromData<uint>(data,nrOfElements);
                    break;
            }
        } else {
            // TODO the logic here can be improved. For instance choose the best device
            // Find some OpenCL image data or buffer data that is up to date
            bool found = false;
            std::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
            for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end(); it++) {
                if(it->second == true) {
                    OpenCLDevice::pointer device = it->first;
                    float stddev;
                    auto access = getOpenCLImageAccess(ACCESS_READ, device);
                    if(mDimensions == 2) {
                        auto clImage = access->get2DImage();
                        getIntensityStdDevFromOpenCLImage(device, *clImage, mType, average, &stddev);
                    } else {
                        auto clImage = access->get3DImage();
                        getIntensityStdDevFromOpenCLImage(device, *clImage, mType, average, &stddev);
                    }
                    mStdDevIntensity = stddev;
                    found = true;
                }
            }

            if(!found) {
                for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end(); it++) {
                    if(it->second == true) {
                        // TODO FIXME, ideally we should use the Buffers directly here, here we get a copy from image to buffer
                        OpenCLDevice::pointer device = it->first;
                        float stddev;
                        auto access = getOpenCLImageAccess(ACCESS_READ, device);
                        if(mDimensions == 2) {
                            auto clImage = access->get2DImage();
                            getIntensityStdDevFromOpenCLImage(device, *clImage, mType, average, &stddev);
                        } else {
                            auto clImage = access->get3DImage();
                            getIntensityStdDevFromOpenCLImage(device, *clImage, mType, average, &stddev);
                        }
                        mStdDevIntensity = stddev;
                        found = true;
                    }
                }
            }
        }

        // Update timestamp
        mStdDevIntensityTimestamp = getTimestamp();
        mStdDevInitialized = true;
    }

    return mStdDevIntensity;
}



float Image::calculateAverageIntensity() {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");

    return calculateSumIntensity() / getNrOfVoxels();
}

float Image::calculateMaximumIntensity() {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    calculateMaxAndMinIntensity();

    return mMaximumIntensity;
}

float Image::calculateMinimumIntensity() {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    calculateMaxAndMinIntensity();

    return mMinimumIntensity;
}

Image::Image(Image::pointer image) : Image() {
    // Create image first
    init(image->getSize(), image->getDataType(), image->getNrOfChannels());

    // Copy metadata
    setSpacing(image->getSpacing());
    updateModifiedTimestamp();
}


Image::pointer Image::copy(ExecutionDevice::pointer device) {
    Image::pointer clone = Image::createFromImage(std::static_pointer_cast<Image>(mPtr.lock()));

    // If device is host, get data from this image to host
    if(device->isHost()) {
        ImageAccess::pointer readAccess = this->getImageAccess(ACCESS_READ);
        ImageAccess::pointer writeAccess = clone->getImageAccess(ACCESS_READ_WRITE);

        void* input = readAccess->get();
        void* output = writeAccess->get();
        switch(getDataType()) {
            fastSwitchTypeMacro(memcpy(output, input, sizeof(FAST_TYPE)*getWidth()*getHeight()*getDepth()*getNrOfChannels()));
        }
    } else {
        // If device is not host
        OpenCLDevice::pointer clDevice = std::static_pointer_cast<OpenCLDevice>(device);
        if(getDimensions() == 2) {
            OpenCLImageAccess::pointer readAccess = this->getOpenCLImageAccess(ACCESS_READ, clDevice);
            OpenCLImageAccess::pointer writeAccess = clone->getOpenCLImageAccess(ACCESS_READ_WRITE, clDevice);
            cl::Image2D* input = readAccess->get2DImage();
            cl::Image2D* output = writeAccess->get2DImage();

            clDevice->getCommandQueue().enqueueCopyImage(
                    *input,
                    *output,
                    createOrigoRegion(),
                    createOrigoRegion(),
                    createRegion(getWidth(), getHeight(), 1)
            );
        } else {
            OpenCLImageAccess::pointer readAccess = this->getOpenCLImageAccess(ACCESS_READ, clDevice);
            OpenCLImageAccess::pointer writeAccess = clone->getOpenCLImageAccess(ACCESS_READ_WRITE, clDevice);
            cl::Image3D* input = readAccess->get3DImage();
            cl::Image3D* output = writeAccess->get3DImage();

            clDevice->getCommandQueue().enqueueCopyImage(
                    *input,
                    *output,
                    createOrigoRegion(),
                    createOrigoRegion(),
                    createRegion(getWidth(), getHeight(), getDepth())
            );
        }
    }

    return clone;
}


void Image::findDeviceWithUptodateData(ExecutionDevice::pointer& device, bool& isOpenCLImage) {
	isOpenCLImage = false;
    // Check first if there are any OpenCL images
    for(auto iterator : mCLImagesIsUpToDate) {
        if(iterator.second) {
            device = iterator.first;
            isOpenCLImage = true;
            return;
        }
    }

    // OpenCL buffers
    for(auto iterator : mCLBuffersIsUpToDate) {
        if(iterator.second) {
            device = iterator.first;
            return;
        }
    }

    // Host data
    if(mHostDataIsUpToDate) {
        device = Host::getInstance();
    } else {
        throw Exception("Image has no up to date data. This should not be possible!");
    }
}

void Image::fill(float value) {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");

	ExecutionDevice::pointer device;
    bool isOpenCLImage;
    try {
		findDeviceWithUptodateData(device, isOpenCLImage);
    } catch(...) {
    	// Has no data
    	// Create an OpenCL image
    	cl::Image* clImage;
        OpenCLDevice::pointer clDevice = std::dynamic_pointer_cast<OpenCLDevice>(
                DeviceManager::getInstance()->getDefaultDevice());
    	if(getDimensions() == 2) {
			clImage = new cl::Image2D(
				clDevice->getContext(),
				CL_MEM_READ_WRITE,
				getOpenCLImageFormat(clDevice, CL_MEM_OBJECT_IMAGE2D, mType, mChannels),
				mWidth, mHeight
			);
    	} else {
			clImage = new cl::Image3D(
				clDevice->getContext(),
				CL_MEM_READ_WRITE,
				getOpenCLImageFormat(clDevice, CL_MEM_OBJECT_IMAGE3D, mType, mChannels),
				mWidth, mHeight, mDepth
			);
    	}
		mCLImages[clDevice] = clImage;
		mCLImagesIsUpToDate[clDevice] = true;
		device = clDevice;
		isOpenCLImage = true;
    }

    if(device->isHost()) {
        throw Exception("Not implemented yet");
    } else {
        OpenCLDevice::pointer clDevice = std::static_pointer_cast<OpenCLDevice>(device);
        cl::CommandQueue queue = clDevice->getCommandQueue();
		std::string sourceFilename = Config::getKernelSourcePath() + "/ImageFill.cl";
		std::string programName = sourceFilename;
		// Only create program if it doesn't exist for this device from before
		if(!clDevice->hasProgram(programName))
			clDevice->createProgramFromSourceWithName(programName, sourceFilename);
		cl::Program program = clDevice->getProgram(programName);
        if(isOpenCLImage) {
            OpenCLImageAccess::pointer access = this->getOpenCLImageAccess(ACCESS_READ_WRITE, clDevice);
            cl_float4 color = {value, value, value, value};
            if(getDimensions() == 2) {
				cl::Kernel kernel(program, "fillImage2D");
				kernel.setArg(0, *access->get2DImage());
				kernel.setArg(1, value);
				queue.enqueueNDRangeKernel(
						kernel,
						cl::NullRange,
						cl::NDRange(mWidth, mHeight),
						cl::NullRange
				);
				// Ideally, we want to use the enqueueFillImage function for this, but it
				// is not working atm on NVIDIA GPUs when visualizing at the same time
            	/*
				queue.enqueueFillImage(
						*access->get2DImage(),
						color,
						createOrigoRegion(),
						createRegion(getSize())
				);
				*/
			} else {
				//throw Exception("Not implemented yet");
                reportWarning() << "Using enqueueFillImage method which may not work while visualizing on NVIDIA GPUs" << reportEnd();
				queue.enqueueFillImage(
						*access->get3DImage(),
						color,
						createOrigoRegion(),
						createRegion(getSize())
				);
			}
        } else {
            OpenCLBufferAccess::pointer access = this->getOpenCLBufferAccess(ACCESS_READ_WRITE, clDevice);
            queue.enqueueFillBuffer(
                    *access->get(),
                    value,
                    0,
                    getBufferSize()
            );
        }
    }
}

Image::pointer Image::crop(VectorXi offset, VectorXi size, bool allowOutOfBoundsCropping, int croppingValue) {
    bool needInitialization = false;
    VectorXi newImageSize = size;
    VectorXi copySourceOffset = offset;
    VectorXi copyDestinationOffset = VectorXi::Zero(offset.size());
    VectorXi copySize = size;
    if(size.x() < 1 || size.y() < 1)
        throw Exception("Invalid crop size (< 1) given to Image::crop");
    if(!allowOutOfBoundsCropping) {
    	// Validate offset
    	if(offset.x() < 0 || offset.y() < 0 || (getDimensions() == 3 && offset.z() < 0)) {
    		throw Exception("Out of bounds cropping not allowed, but offset was below 0.");
    	}
    	if(offset.x() + size.x() > getWidth() || offset.y() + size.y() > getHeight())
    	    throw Exception("Trying to crop an image outside of its border and allow out of bounds cropping is set to false: " +
    	    std::to_string(offset.x()) + " " + std::to_string(size.x()) + " " + std::to_string(getWidth()) + " " +
            std::to_string(offset.y()) + " " + std::to_string(size.y()) + " " + std::to_string(getHeight())
    	    );
    } else {
    	// Calculate offsets and sizes
    	for(int i = 0; i < offset.size(); ++i) {
    		if(offset[i] < 0) { // If offset is below zero, copy source offset should be zero
				copySourceOffset[i] = 0;
				copyDestinationOffset[i] = -offset[i];
				needInitialization = true;
				copySize[i] -= copyDestinationOffset[i];
    		}
    		if(copySourceOffset[i] + copySize[i] > getSize()[i]) { // If cut region is larger than image
    			// Reduce copy size
    			copySize[i] -= (copySourceOffset[i] + copySize[i]) - getSize()[i];
				needInitialization = true;
    		}
    	}
    }

    ExecutionDevice::pointer device;
    bool isOpenCLImage;
    findDeviceWithUptodateData(device, isOpenCLImage);
    // Handle host

    OpenCLDevice::pointer clDevice;
    if(device->isHost()) { // If data is only on host, copy data to GPU first
        // TODO implement cropping on host instead
        clDevice = std::dynamic_pointer_cast<OpenCLDevice>(DeviceManager::getInstance()->getDefaultDevice());
        copyData(clDevice, mHostData.get());
    } else {
        clDevice = std::static_pointer_cast<OpenCLDevice>(device);
    }

    Image::pointer newImage;
    if(getDimensions() == 2) {
        if(offset.size() < 2 || size.size() < 2)
            throw Exception("offset and size vectors given to Image::crop must have at least 2 channels");
        newImage = Image::create(newImageSize.cast<uint>(), getDataType(), getNrOfChannels());
        if(needInitialization)
            newImage->fill(croppingValue);
        OpenCLImageAccess::pointer readAccess = this->getOpenCLImageAccess(ACCESS_READ, clDevice);
        OpenCLImageAccess::pointer writeAccess = newImage->getOpenCLImageAccess(ACCESS_READ_WRITE, clDevice);
        cl::Image2D* input = readAccess->get2DImage();
        cl::Image2D* output = writeAccess->get2DImage();

        /*
        std::cout << "New cropping:" << std::endl;
        std::cout << offset.transpose() << std::endl;
        std::cout << size.transpose() << std::endl;
        std::cout << getSize().transpose() << std::endl;
        std::cout << newImage->getSize().transpose() << std::endl;
        std::cout << copySourceOffset.transpose() << std::endl;
        std::cout << copyDestinationOffset.transpose() << std::endl;
        std::cout << copySize.transpose() << std::endl;
        */
        clDevice->getCommandQueue().enqueueCopyImage(
                *input,
                *output,
                createRegion(copySourceOffset.x(), copySourceOffset.y(), 0),
                createRegion(copyDestinationOffset.x(), copyDestinationOffset.y(), 0),
                createRegion(copySize.x(), copySize.y(), 1)
        );
    } else {
        if(offset.size() < 3 || size.size() < 3)
            throw Exception("offset and size vectors given to Image::crop must have at least 3 channels");
        newImage = Image::create(newImageSize.cast<uint>(), getDataType(), getNrOfChannels());
        if(needInitialization)
            newImage->fill(croppingValue);
        OpenCLImageAccess::pointer readAccess = this->getOpenCLImageAccess(ACCESS_READ, clDevice);
        OpenCLImageAccess::pointer writeAccess = newImage->getOpenCLImageAccess(ACCESS_READ_WRITE, clDevice);
        cl::Image3D* input = readAccess->get3DImage();
        cl::Image* output;
        if(newImage->getDimensions() == 2) {
            output = writeAccess->get2DImage();
        } else {
            output = writeAccess->get3DImage();
        }
        clDevice->getCommandQueue().enqueueCopyImage(
                *input,
                *output,
                createRegion(copySourceOffset.x(), copySourceOffset.y(), copySourceOffset.z()),
                createRegion(copyDestinationOffset.x(), copyDestinationOffset.y(), copyDestinationOffset.z()),
                createRegion(copySize.x(), copySize.y(), copySize.z())
        );
    }

    // Fix placement and spacing of the new cropped image
    newImage->setSpacing(getSpacing());
    // Multiply with spacing here to convert voxel translation to world(mm) translation
    Affine3f T;
    T.translation() = getSpacing().cwiseProduct(getDimensions() == 2 ? Vector3f(offset.x(), offset.y(), 0) : Vector3f(offset.x(), offset.y(), offset.z()));
    newImage->getSceneGraphNode()->setTransform(T);
    SceneGraph::setParentNode(newImage, std::static_pointer_cast<SpatialDataObject>(mPtr.lock()));

    return newImage;
}

DataBoundingBox Image::getTransformedBoundingBox() const {
    auto T = SceneGraph::getEigenTransformFromNode(getSceneGraphNode());

    // Add image spacing
    T.scale(getSpacing());

    return SpatialDataObject::getBoundingBox().getTransformedBoundingBox(T);
}

DataBoundingBox Image::getBoundingBox() const {
    // Add image spacing
    auto T = Affine3f::Identity();
    T.scale(getSpacing());

    return SpatialDataObject::getBoundingBox().getTransformedBoundingBox(T);
}

int Image::getNrOfVoxels() const {
    return mWidth*mHeight*mDepth;
}

Image::~Image() {
    freeAll();
}

OpenGLTextureAccess::pointer Image::getOpenGLTextureAccess(accessType type, OpenCLDevice::pointer device, bool compress, bool getOwnership) {
#ifdef FAST_MODULE_VISUALIZATION
    if(type == ACCESS_READ_WRITE)
        throw Exception("Read-only access to OpenGL texture for now");
    if(mDimensions != 2)
        throw Exception("Only 2D access for OpenGL texture");

    {
        std::lock_guard<std::mutex> lock(mDataIsBeingAccessedMutex);
        mDataIsBeingAccessed = true;
    }
    uint textureID = 0;
    if(!m_GLtextureUpToDate) {
        std::map<int, std::vector<GLint>> mChannelsToSwizzle = {
                {1, {GL_RED, GL_RED, GL_RED, GL_ONE}},
                {2, {GL_RED, GL_GREEN, GL_ZERO, GL_ONE}},
                {3, {GL_RED, GL_GREEN, GL_BLUE, GL_ONE}},
                {4, {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA}}
        };
        std::map<DataType, std::map<int, std::pair<GLenum, GLenum>>> mChannelsToFormat = {
                {TYPE_UINT8,
                    {
                        {1, {GL_R8UI, GL_RED_INTEGER}},
                        {2, {GL_RG8UI, GL_RG_INTEGER}},
                        {3, {GL_RGB8UI, GL_RGB_INTEGER}},
                        {4, {GL_RGBA8UI, GL_RGBA_INTEGER}}
                    }
                },
                {TYPE_INT8,
                    {
                        {1, {GL_R8I, GL_RED_INTEGER}},
                        {2, {GL_RG8I, GL_RG_INTEGER}},
                        {3, {GL_RGB8I, GL_RGB_INTEGER}},
                        {4, {GL_RGBA8I, GL_RGBA_INTEGER}}
                    }
                },

                {TYPE_UINT16,
                        {
                                {1, {GL_R16UI, GL_RED_INTEGER}},
                                {2, {GL_RG16UI, GL_RG_INTEGER}},
                                {3, {GL_RGB16UI, GL_RGB_INTEGER}},
                                {4, {GL_RGBA16UI, GL_RGBA_INTEGER}}
                        }
                },
                {TYPE_INT16,
                        {
                                {1, {GL_R16I, GL_RED_INTEGER}},
                                {2, {GL_RG16I, GL_RG_INTEGER}},
                                {3, {GL_RGB16I, GL_RGB_INTEGER}},
                                {4, {GL_RGBA16I, GL_RGBA_INTEGER}}
                        }
                },
                {TYPE_UINT32,
                 {
                    {1, {GL_R32UI, GL_RED_INTEGER}},
                    {2, {GL_RG32UI, GL_RG_INTEGER}},
                    {3, {GL_RGB32UI, GL_RGB_INTEGER}},
                    {4, {GL_RGBA32UI, GL_RGBA_INTEGER}}
                 }
                 },
                 {TYPE_INT32,
                  {
                     {1, {GL_R32I, GL_RED_INTEGER}},
                     {2, {GL_RG32I, GL_RG_INTEGER}},
                     {3, {GL_RGB32I, GL_RGB_INTEGER}},
                     {4, {GL_RGBA32I, GL_RGBA_INTEGER}}
                  }
                  },
                {TYPE_FLOAT,
                        {
                                {1, {GL_R32F, GL_RED}},
                                {1, {GL_RG32F, GL_RG}},
                                {3, {GL_RGB32F, GL_RGB}},
                                {4, {GL_RGBA32F, GL_RGBA}}
                        }
                },
        };
        std::map<DataType, GLenum> mTypeToType = {
                {TYPE_UINT8, GL_UNSIGNED_BYTE},
                {TYPE_INT8, GL_BYTE},
                {TYPE_UINT16, GL_UNSIGNED_SHORT},
                {TYPE_INT16, GL_SHORT},
                {TYPE_UINT32, GL_UNSIGNED_INT},
                {TYPE_INT32, GL_INT},
                {TYPE_FLOAT, GL_FLOAT},
        };
        GLint internalFormat = mChannelsToFormat[mType][mChannels].first;
        GLenum format = mChannelsToFormat[mType][mChannels].second;
        if(compress) {
            if(mType != TYPE_UINT8)
                throw Exception("OpenGL texture compression only enabled for UINT8 images.");
            switch(mChannels) {
                case 1:
                    internalFormat = GL_COMPRESSED_RED_RGTC1;
                    format = GL_RED;
                    break;
                case 2:
                    internalFormat = GL_COMPRESSED_RG_RGTC2;
                    format = GL_RG;
                    break;
                case 3:
                    internalFormat = GL_COMPRESSED_RGB;
                    format = GL_RGB;
                    break;
                case 4:
                    internalFormat = GL_COMPRESSED_RGBA;
                    format = GL_RGBA;
                    break;
            }
        }
        GLenum GLtype = mTypeToType[mType];
        GLint* swizzleMask = mChannelsToSwizzle[mChannels].data();

        // Create OpenGl texture
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Fix alignment issues with single channel images..
        bool doCPUtransfer = true; // If GPU-GPU transfer is possible do that, but GPU-CPU-GPU transfer is fallback.
        // If OpenGL interop is supported AND OpenCL has the data on the device..
        // GL interop on compressed textures doesn't work
        if(!compress && device->isOpenGLInteropSupported() && (mCLImagesIsUpToDate[device] || mCLBuffersIsUpToDate[device])) {
            auto access = getOpenCLImageAccess(ACCESS_READ, device);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, mWidth, mHeight, 0, format, GLtype, nullptr);
            glFinish();
            // Create OpenCL Image from texture
            try {
                auto imageGL = cl::ImageGL(
                        device->getContext(),
                        CL_MEM_READ_WRITE,
                        GL_TEXTURE_2D,
                        0,
                        textureID
                );

                // Copy OpenCL image to the texture
                std::vector<cl::Memory> v;
                v.push_back(imageGL);
                device->getCommandQueue().enqueueAcquireGLObjects(&v);
#ifdef WIN32
                // This hack fixes a strange bug on some windows machines with the NVIDIA CL platform
                // where without this finish statement, the program just hangs after the next line
                if(device->getPlatformVendor() == PLATFORM_VENDOR_NVIDIA) {
                    device->getCommandQueue().finish();
                }
#endif
                device->getCommandQueue().enqueueCopyImage(*access->get(), imageGL, createOrigoRegion(), createOrigoRegion(), createRegion(getSize()));
                device->getCommandQueue().enqueueReleaseGLObjects(&v);
                doCPUtransfer = false;
            } catch(cl::Error &e) {
                // Most likely the format was not supported
                reportWarning() << "OpenGL interop was supported, but failed to transfer data. Error was: " << e.what() << reportEnd();
            }
        }
        if(doCPUtransfer) {
            // Copy data from CPU to GL texture
            auto access = getImageAccess(ACCESS_READ);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, mWidth, mHeight, 0, format, GLtype, access->get());
            glFinish();
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        if(!getOwnership) {
            // This Image object will own this texture, so we need to keep a record of it
            m_GLtextureID = textureID;
            m_GLtextureUpToDate = true;
        }
    } else {
        textureID = m_GLtextureID;
        if(getOwnership) {
            m_GLtextureID = 0;
            m_GLtextureUpToDate = false;
        }
    }

    return std::make_unique<OpenGLTextureAccess>(textureID, std::dynamic_pointer_cast<Image>(mPtr.lock()));
#else
    throw Exception("Image::getOpenGLTextureAccess() is only available when FAST is built with visualization/Qt");
#endif
}

bool Image::isSegmentationType() const {
    if(!isInitialized())
        throw Exception("Image was not initialized");
    return mType == TYPE_UINT8 && mChannels == 1;
}

bool Image::hasHostUpToDateData() const {
    return mHostHasData && mHostDataIsUpToDate;
}

bool Image::hasOpenCLUpToDateImageData(OpenCLDevice::pointer device) const {
    if(mCLImagesIsUpToDate.count(device) == 0)
        return false;
    return mCLImagesIsUpToDate.at(device);
}

bool Image::hasOpenCLUpToDateBufferData(OpenCLDevice::pointer device) const {
    if(mCLBuffersIsUpToDate.count(device) == 0)
        return false;
    return mCLBuffersIsUpToDate.at(device);
}

} // end namespace fast;


