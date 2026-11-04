#pragma once

#include "FAST/Object.hpp"
#include "RuntimeMeasurementManager.hpp"

class QOpenGLContext;

namespace fast {

class FAST_EXPORT  ExecutionDevice : public Object {
    public:
        typedef std::shared_ptr<ExecutionDevice> pointer;
        bool isHost() {return mIsHost;};
        virtual ~ExecutionDevice() {};
        static std::string getStaticNameOfClass() {
            return "ExecutionDevice";
        }
    protected:
        bool mIsHost;
        std::weak_ptr<ExecutionDevice> mPtr;

};

class DeviceManager; // Forward declaration
class FAST_EXPORT  Host : public ExecutionDevice {
    public:
        typedef std::shared_ptr<Host> pointer;
        static Host::pointer getInstance() {
            static Host::pointer instance = Host::New();
            return instance;
        }
        static std::string getStaticNameOfClass() {
            return "Host";
        }
    // Declare factory method New private
    private:
        static Host::pointer New() {
            Host* ptr = new Host();
            Host::pointer smartPtr(ptr);
            ptr->setPtr(smartPtr);

            return smartPtr;
        }
        void setPtr(Host::pointer ptr) {
            mPtr = ptr;
        }
        Host() {
            mIsHost = true;
        }

};


enum OpenCLPlatformVendor {
    PLATFORM_VENDOR_APPLE,
    PLATFORM_VENDOR_AMD,
    PLATFORM_VENDOR_INTEL,
    PLATFORM_VENDOR_NVIDIA,
    PLATFORM_VENDOR_POCL,
    PLATFORM_VENDOR_MESA,
    PLATFORM_VENDOR_UNKNOWN
};

enum DeviceVendor {
    DEVICE_VENDOR_AMD,
    DEVICE_VENDOR_INTEL,
    DEVICE_VENDOR_NVIDIA,
    DEVICE_VENDOR_UKNOWN
};

class FAST_EXPORT  OpenCLDevice : public ExecutionDevice {
    FAST_OBJECT(OpenCLDevice)
    public:
        cl::CommandQueue getCommandQueue();
        cl::Device getDevice();

        int createProgramFromSource(std::string filename, std::string buildOptions = "", bool caching = true);
        int createProgramFromSource(std::vector<std::string> filenames, std::string buildOptions = "");
        int createProgramFromString(std::string code, std::string buildOptions = "");
        int createProgramFromSourceWithName(std::string programName, std::string filename, std::string buildOptions = "");
        int createProgramFromSourceWithName(std::string programName, std::vector<std::string> filenames, std::string buildOptions = "");
        int createProgramFromStringWithName(std::string programName, std::string code, std::string buildOptions = "");
        cl::Program getProgram(unsigned int i);
        cl::Program getProgram(std::string name);
        bool hasProgram(std::string name);

        bool isImageFormatSupported(cl_channel_order order, cl_channel_type type, cl_mem_object_type imageType);
        bool isOpenGLInteropSupported();

        //OpenCLPlatformVendor getPlatformVendor();
        //DeviceVendor getDeviceVendor();

        cl::CommandQueue getQueue(unsigned int i);
        cl::Device getDevice(unsigned int i);
        cl::Device getDevice(cl::CommandQueue queue);
        cl::Context getContext();
        cl::Platform getPlatform();

        OpenCLDevice(std::vector<cl::Device> devices, QOpenGLContext* context);
        std::string getName() {
            return getDevice().getInfo<CL_DEVICE_NAME>();
        }
        OpenCLPlatformVendor getPlatformVendor();
        bool isWritingTo3DTexturesSupported();
        RuntimeMeasurementsManager::pointer getRunTimeMeasurementManager();
        ~OpenCLDevice();
        bool hasExtension(const std::string& name);
        std::vector<std::string> getExtensions();
    private:
        OpenCLDevice();
        cl::Program writeBinary(std::string filename, std::string buildOptions);
        cl::Program readBinary(std::string filename);
        cl::Program buildProgramFromBinary(std::string filename, std::string buildOptions);
        cl::Program buildSources(cl::Program::Sources source, std::string buildOptions);
        std::tuple<std::string, std::string, std::vector<std::string>> getBinaryName(std::string filename, std::string buildOptions);

        bool m_hasOpenGLInterop = false;
        cl::Context context;
        std::vector<cl::CommandQueue> queues;
        std::map<std::string, int> programNames;
        std::vector<cl::Program> programs;
        std::vector<cl::Device> devices;
        cl::Platform platform;

        bool profilingEnabled;
        RuntimeMeasurementsManager::pointer runtimeManager;

};
#ifndef SWIG
cl_context_properties* createInteropContextProperties(
            cl_platform_id platform,
            cl_context_properties OpenGLContext,
            cl_context_properties display, std::string type = "");
#endif
} // end namespace fast

