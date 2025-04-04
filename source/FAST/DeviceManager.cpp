#include "FAST/DeviceManager.hpp"
#include "FAST/Exception.hpp"
#include <algorithm>
#include <FAST/Config.hpp>
#ifdef FAST_MODULE_VISUALIZATION
#include "FAST/Visualization/Window.hpp"
#endif

#if defined(__APPLE__) || defined(__MACOSX)
#include <CL/cl_gl.h>
#include <OpenGL/OpenGL.h>
#else
#if _WIN32
#include <windows.h>
#include <GL/gl.h>
#include <CL/cl_gl.h>
#else
#include <GL/glx.h>
#include <CL/cl_gl.h>
#endif
#endif

namespace fast {

DeviceManager* DeviceManager::mInstance = nullptr;
DevicePlatform DeviceManager::m_devicePlatform = DEVICE_PLATFORM_ANY;

inline cl_context_properties* createInteropContextProperties(
        const cl_platform_id &platform,
        cl_context_properties OpenGLContext,
        cl_context_properties display) {
#if defined(__APPLE__) || defined(__MACOSX)
    // Apple (untested)
    // TODO: create GL context for Apple
CGLSetCurrentContext((CGLContextObj)OpenGLContext);
CGLShareGroupObj shareGroup = CGLGetShareGroup((CGLContextObj)OpenGLContext);
if(shareGroup == NULL)
throw Exception("Not able to get sharegroup");
    cl_context_properties * cps = new cl_context_properties[3];
    cps[0] = CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE;
    cps[1] = (cl_context_properties)shareGroup;
    cps[2] = 0;

#else
#ifdef _WIN32
    // Windows
    cl_context_properties * cps = new cl_context_properties[7];
    cps[0] = CL_GL_CONTEXT_KHR;
    cps[1] = OpenGLContext;
    cps[2] = CL_WGL_HDC_KHR;
    cps[3] = display;
    cps[4] = CL_CONTEXT_PLATFORM;
    cps[5] = (cl_context_properties)platform;
	cps[6] = 0;
#else
    cl_context_properties * cps = new cl_context_properties[7];
    cps[0] = CL_GL_CONTEXT_KHR;
    cps[1] = OpenGLContext;
    cps[2] = CL_GLX_DISPLAY_KHR;
    cps[3] = display;
    cps[4] = CL_CONTEXT_PLATFORM;
    cps[5] = (cl_context_properties)platform;
	cps[6] = 0;
#endif
#endif
    return cps;
}

DeviceManager* DeviceManager::getInstance() {
    if(mInstance == nullptr) {
        auto debugFlag = std::getenv("FAST_DEBUG");
        if(debugFlag != nullptr)
            Reporter::setGlobalReportMethod(Reporter::COUT);
#ifdef FAST_MODULE_VISUALIZATION
        Window::initializeQtApp();
#endif
        mInstance = new DeviceManager();
    }

    return mInstance;
}

void DeviceManager::deleteInstance() {
    if(mInstance != NULL) {
        // TODO this creates a memory leak, but deleting this instance causes seg fault for some reason
        //delete mInstance;
        mInstance = NULL;
    }
}

std::vector<OpenCLDevice::pointer> DeviceManager::getDevices(DeviceCriteria criteria, bool enableVisualization) {
    unsigned long * glContext = nullptr;
    if(enableVisualization) {
#ifdef FAST_MODULE_VISUALIZATION
        fast::Window::getMainGLContext(); // Still have to create GL context
		Window::getMainGLContext()->makeCurrent(nullptr); // If not nullptr here, we get NV-GLX missing on display error
#endif
#if defined(__APPLE__) || defined(__MACOSX)
		CGLContextObj appleContext = CGLGetCurrentContext();

		glContext = (unsigned long *)appleContext;
#elif _WIN32
        glContext = (unsigned long *)wglGetCurrentContext();
#else
        glContext = (long unsigned int*)glXGetCurrentContext();
#endif
        criteria.setCapabilityCriteria(DEVICE_CAPABILITY_OPENGL_INTEROP);
    }
    std::vector<PlatformDevices> platformDevices = getDevices(criteria);
    if(platformDevices.empty())
        return {};
    std::vector<cl::Device> validDevices = getDevicesForBestPlatform(criteria, platformDevices);

    std::vector<OpenCLDevice::pointer> executionDevices;
    for(unsigned int i = 0; i < validDevices.size(); i++) {
        std::vector<cl::Device> deviceVector;
        deviceVector.push_back(validDevices[i]);
        OpenCLDevice * device = new OpenCLDevice(deviceVector, glContext);
        executionDevices.push_back(OpenCLDevice::pointer(device));
    }

    return executionDevices;
}

std::vector<OpenCLDevice::pointer> DeviceManager::getAllDevices(
        bool enableVisualization) {

    DeviceCriteria criteria;
    return getDevices(criteria,enableVisualization);
}

std::vector<OpenCLDevice::pointer> DeviceManager::getAllGPUDevices(
        bool enableVisualization) {
    DeviceCriteria criteria;
    criteria.setTypeCriteria(DEVICE_TYPE_GPU);
    return getDevices(criteria,enableVisualization);
}

std::vector<OpenCLDevice::pointer> DeviceManager::getAllCPUDevices(
        bool enableVisualization) {
    DeviceCriteria criteria;
    criteria.setTypeCriteria(DEVICE_TYPE_CPU);
    return getDevices(criteria,enableVisualization);
}

OpenCLDevice::pointer DeviceManager::getOneGPUDevice(
        bool enableVisualization) {

    DeviceCriteria criteria;
    criteria.setTypeCriteria(DEVICE_TYPE_GPU);
    criteria.setDeviceCountCriteria(1);
    std::vector<OpenCLDevice::pointer> devices = getDevices(criteria,enableVisualization);
	if(devices.size() == 0)
		throw Exception("No compatible OpenCL devices found");
    return devices[0];
}

OpenCLDevice::pointer DeviceManager::getOneOpenCLDevice(
        bool enableVisualization, DevicePlatform platform) {

    DeviceCriteria criteria;

    // Check if a GPU is available first, if not choose any
    criteria.setTypeCriteria(DEVICE_TYPE_GPU);
    criteria.setDeviceCountCriteria(1);
    criteria.setDevicePreference(DEVICE_PREFERENCE_COMPUTE_UNITS);
    criteria.setPlatformCriteria(platform);
    std::vector<OpenCLDevice::pointer> devices = getDevices(criteria,enableVisualization);
    if(devices.size() > 0) {
        return devices[0];
    } else {
        reportInfo() << "NO suitable GPU found! Trying to find other devices" << Reporter::end();
        criteria.setTypeCriteria(DEVICE_TYPE_ANY);
        std::vector<OpenCLDevice::pointer> devices = getDevices(criteria,enableVisualization);
		if(devices.size() == 0)
			throw Exception("No compatible OpenCL devices found");
        return devices[0];
    }
}

OpenCLDevice::pointer DeviceManager::getOneCPUDevice(
        bool enableVisualization) {
    DeviceCriteria criteria;
    criteria.setTypeCriteria(DEVICE_TYPE_CPU);
    criteria.setDeviceCountCriteria(1);
    std::vector<OpenCLDevice::pointer> devices = getDevices(criteria,enableVisualization);
	if(devices.size() == 0)
		throw Exception("No compatible OpenCL devices found");
    return devices[0];
}

Host::pointer DeviceManager::getHostDevice() {
    return Host::getInstance();
}

void DeviceManager::setDefaultDevice(ExecutionDevice::pointer device) {
    mDefaultComputationDevice = device;
}

ExecutionDevice::pointer DeviceManager::getDefaultDevice() {
    return mDefaultComputationDevice;
}

DeviceManager::DeviceManager() {
    cl::Platform::get(&platforms);

    auto platformEnvVar = std::getenv("FAST_OPENCL_PLATFORM");
    if(platformEnvVar != nullptr) {
        auto platform = stringToUpper(platformEnvVar);
        std::map<std::string, DevicePlatform> platformMap = {
            {"AMD", DEVICE_PLATFORM_AMD},
            {"NVIDIA", DEVICE_PLATFORM_NVIDIA},
            {"INTEL", DEVICE_PLATFORM_INTEL},
            {"APPLE", DEVICE_PLATFORM_APPLE},
            {"POCL", DEVICE_PLATFORM_POCL},
            {"MESA", DEVICE_PLATFORM_MESA},
            {"OPENCLON12", DEVICE_PLATFORM_OPENCL_ON_12}
        };
        if(platformMap.count(platform) > 0) {
            reportInfo() << "Selecting OpenCL platform from provided environment variable FAST_OPENCL_PLATFORM=" + std::string(platformEnvVar) << reportEnd();
            m_devicePlatform = platformMap[platform];
        } else {
            throw Exception("OpenCL platform environment variable specified to an unsupported value: " + std::string(platformEnvVar));
        }
    }

    // Set one random device as default device
    // First try to get one OpenCL device with OpenGL interop:
    try {
        setDefaultDevice(getOneOpenCLDevice(Config::getVisualization(), m_devicePlatform));
    } catch(Exception& e) {
        // If that fails, try to get a device, without GL interop
        reportInfo() << "No devices with OpenGL interop was found. Looking again for best device WITHOUT OpenGL interop, this may lower visualization performance." << reportEnd();
        setDefaultDevice(getOneOpenCLDevice(false, m_devicePlatform));
    }

    auto device = std::static_pointer_cast<OpenCLDevice>(getDefaultDevice());
    reportInfo() << "The following device was selected as main device: " << device->getName() << reportEnd();
    if(!device->isWritingTo3DTexturesSupported()) {
        reportInfo() << "Writing directly to 3D textures/images is NOT supported on main device" << reportEnd();
    } else {
        reportInfo() << "Writing directly to 3D textures/images is supported on main device" << reportEnd();
    }
}

OpenCLDevice::pointer DeviceManager::getDevice(
        DeviceCriteria criteria) {
    bool interop = false;
    if(criteria.hasCapabilityCriteria(DEVICE_CAPABILITY_OPENGL_INTEROP)) {
        interop = true;
    }
    criteria.setDeviceCountCriteria(1);
    std::vector<OpenCLDevice::pointer> devices = getDevices(criteria, interop);
    if(devices.size() == 0)
        throw Exception("Found no devices which satisfies the device criteria");
    return devices[0];
}

bool DeviceManager::deviceSatisfiesCriteria(OpenCLDevice::pointer device,
        const DeviceCriteria& criteria) {
    return deviceSatisfiesCriteria(criteria, device->getDevice(), device->getPlatform());
}


bool DeviceManager::deviceHasOpenGLInteropCapability(const cl::Device &device, const cl::Platform &platform) {
#ifdef WIN32
    reportInfo() << "Windows detected, disabling OpenGL interop due to random crashes when rendering image and segmentation at the same time." << reportEnd();
    return false;
#else
    if(platform.getInfo<CL_PLATFORM_VENDOR>().find("NVIDIA") != std::string::npos) {
        //reportInfo() << "NVIDIA platform was detected on linux, disabling OpenGL interop due to error Xlib extension NV-GLX missing" << reportEnd();
        //return false;
    }
#endif
    // Get the cl_device_id of the device
    cl_device_id deviceID = device();
    // Get the platform of device
#if CL_HPP_TARGET_OPENCL_VERSION >= 300
    cl_platform_id platformId = device.getInfo<CL_DEVICE_PLATFORM>()();
#else
    cl_platform_id platformId = device.getInfo<CL_DEVICE_PLATFORM>();
#endif
    // Get all devices that are capable of OpenGL interop with this platform
    // Create properties for CL-GL context
#ifdef FAST_MODULE_VISUALIZATION
		Window::getMainGLContext()->makeCurrent(Window::getQSurface());
#endif
		unsigned long* glContext;
#if defined(__APPLE__) || defined(__MACOSX)
		CGLContextObj appleContext = CGLGetCurrentContext();

		glContext = (unsigned long *)appleContext;
#elif _WIN32
        cl_context_properties * cps = createInteropContextProperties(platformId, (cl_context_properties)wglGetCurrentContext(), (cl_context_properties)wglGetCurrentDC());
#else
        glContext = (unsigned long*)glXGetCurrentContext();
        cl_context_properties * cps = createInteropContextProperties(platformId, (cl_context_properties)glContext, (cl_context_properties)glXGetCurrentDisplay());
#endif
#if defined(__APPLE__) || defined(__MACOSX)

CGLShareGroupObj shareGroup = CGLGetShareGroup((CGLContextObj)glContext);
if(shareGroup == NULL)
throw Exception("Not able to get sharegroup");

    cl_context_properties cps[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
        (cl_context_properties)shareGroup,
        0};

    cl_device_id cl_gl_device_ids[32];
    size_t returnSize = 0;
	cl_context clContext = clCreateContext(cps, 0, NULL, 0, 0, NULL);

/*
  size_t size;
  cl_uint num_devices;
  clGetContextInfo(clContext, CL_CONTEXT_DEVICES, 0, NULL, &size);

  num_devices = size / sizeof(cl_device_id);
*/

    //clGetGLContextInfoAPPLE_fn func = (clGetGLContextInfoAPPLE_fn) clGetExtensionFunctionAddressForPlatform(platformId, "clGetGLContextInfoAPPLE");
    clGetGLContextInfoAPPLE(clContext, glContext, CL_CGL_DEVICE_FOR_CURRENT_VIRTUAL_SCREEN_APPLE, 32 * sizeof(cl_device_id), &cl_gl_device_ids, &returnSize);

    //reporter.report("There are " + number(returnSize / sizeof(cl_device_id)) + " devices that can be associated with the GL context", INFO);

    bool found = false;
    for (int i = 0; i < returnSize / sizeof(cl_device_id); i++) {
        cl::Device device2(cl_gl_device_ids[i]);
        if (deviceID == device2()) {
            found = true;
            break;
        }
    }

#else
	// check if any of these devices have the same cl_device_id as deviceID
    // Query which devices are associated with GL context
    cl_device_id cl_gl_device_ids[32];
    size_t returnSize = 0;
    clGetGLContextInfoKHR_fn glGetGLContextInfo_func = (clGetGLContextInfoKHR_fn) clGetExtensionFunctionAddressForPlatform(platformId, "clGetGLContextInfoKHR");
    if(glGetGLContextInfo_func == NULL) {
	    Reporter::info() << "clGetGLContextInfoKHR function was not found. OpenGL interop unavailable." << Reporter::end();
	    return false;
    }
    auto extensions = device.getInfo<CL_DEVICE_EXTENSIONS>();
    if(extensions.find("cl_khr_gl_sharing") == std::string::npos) {
	    Reporter::info() << "Device does not support cl_khr_gl_sharing." << Reporter::end();
	    return false;
    }
    glGetGLContextInfo_func(cps, CL_DEVICES_FOR_GL_CONTEXT_KHR, 32 * sizeof(cl_device_id), &cl_gl_device_ids, &returnSize);
    delete[] cps;

    reportInfo() << "There are " << (returnSize / sizeof(cl_device_id)) << " devices that can be associated with the GL context" << Reporter::end();

    bool found = false;
    for (int i = 0; i < returnSize / sizeof(cl_device_id); i++) {
        cl::Device device2(cl_gl_device_ids[i]);
        reportInfo() << deviceID << " - " << device2() << Reporter::end();
        if (deviceID == device2()) {
            found = true;
            break;
        }
    }

#endif // windows or linux
	// Cleanup
	return found;
}

bool DeviceManager::devicePlatformMismatch(
        const cl::Device &device,
        const cl::Platform &platform) {
    std::string platformVendorStr = platform.getInfo<CL_PLATFORM_VENDOR>();
    DevicePlatform platformVendor = getDevicePlatform(platformVendorStr);

    std::string deviceVendorStr = device.getInfo<CL_DEVICE_VENDOR>();
    DevicePlatform deviceVendor = getDevicePlatform(deviceVendorStr);

    return platformVendor != deviceVendor;
}

typedef struct deviceAndScore {
        int score;
        cl::Device device;
} deviceAndScore;
bool compareScores(deviceAndScore a, deviceAndScore b) {
    return (a.score > b.score);
}
;
void DeviceManager::sortDevicesAccordingToPreference(
        int numberOfPlatforms,
        int maxNumberOfDevices,
        std::vector<PlatformDevices> platformDevices,
        DevicePreference preference,
        std::vector<cl::Device> * sortedPlatformDevices,
        std::vector<int>& platformScores) {
    for (int i = 0; i < numberOfPlatforms; i++) {
        if (platformDevices[i].second.size() == 0)
            continue;
        // Go through each device and give it a score based on the preference
        std::vector<deviceAndScore> deviceScores;
        for (int j = 0; j < platformDevices[i].second.size(); j++) {
            cl::Device device = platformDevices[i].second[j];
            deviceAndScore das;
            std::string deviceVendorStr = device.getInfo<CL_DEVICE_VENDOR>();
            DevicePlatform deviceVendor = getDevicePlatform(deviceVendorStr);
            // Prefer AMD and NVIDIA platform over Intel
            if(deviceVendor == DevicePlatform::DEVICE_PLATFORM_NVIDIA) {
                das.score = 1000000;
            } else if(deviceVendor == DevicePlatform::DEVICE_PLATFORM_AMD) {
                das.score = 1000000;
            } else {
                das.score = 0;
            }

            das.device = device;
            switch (preference) {
            case DEVICE_PREFERENCE_NOT_CONNECTED_TO_SCREEN:
                if (!deviceHasOpenGLInteropCapability(device, platformDevices[i].first)) {
                    das.score += 1;
                } else {
                    das.score += 0;
                }
                break;
            case DEVICE_PREFERENCE_COMPUTE_UNITS:
                das.score += device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
                reportInfo() << device.getInfo<CL_DEVICE_NAME>() << " has " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << " compute units" << reportEnd();
                break;
            case DEVICE_PREFERENCE_GLOBAL_MEMORY:
                das.score += device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>()
                        / (1024 * 1024); // In MBs
                break;
            default:
                // Do nothing
                reportInfo() << "No valid preference selected." << Reporter::end();
                break;
            }
            reportInfo() << "The device " <<  device.getInfo<CL_DEVICE_NAME>() << " got a score of " << (das.score) << Reporter::end();
            deviceScores.push_back(das);
        }

        // Sort devices according to the scores
        std::sort(deviceScores.begin(), deviceScores.end(), compareScores);

        // put devices back to vector and calculate scores
        int platformScore = 0;
        for (int j = 0; j < std::min(maxNumberOfDevices, (int) platformDevices[i].second.size()); j++) {
            sortedPlatformDevices[i].push_back(deviceScores[j].device);
            platformScore += deviceScores[j].score;
        }
        platformScores[i] = platformScore;
    }
}

DevicePlatform DeviceManager::getDevicePlatform(std::string platformVendor) {
    DevicePlatform retval;
    if (platformVendor.find("Advanced Micro Devices") != std::string::npos || platformVendor.find("AMD") != std::string::npos) {
        retval = DEVICE_PLATFORM_AMD;
    } else if (platformVendor.find("Apple") != std::string::npos) {
        retval = DEVICE_PLATFORM_APPLE;
    } else if (platformVendor.find("Intel") != std::string::npos) {
        retval = DEVICE_PLATFORM_INTEL;
    } else if (platformVendor.find("NVIDIA") != std::string::npos) {
        retval = DEVICE_PLATFORM_NVIDIA;
    } else if(platformVendor.find("Portable Computing Language") != std::string::npos) {
        retval = DEVICE_PLATFORM_POCL;
    } else if(platformVendor.find("Mesa") != std::string::npos) {
        retval = DEVICE_PLATFORM_MESA;
    } else if(platformVendor.find("OpenCLOn12") != std::string::npos) {
        retval = DEVICE_PLATFORM_OPENCL_ON_12;
    } else {
        retval = DEVICE_PLATFORM_UNKNOWN;
	}
    return retval;
}

std::string DeviceManager::getDevicePlatformVendor(DevicePlatform devicePlatform) {
    std::string retval;
    switch (devicePlatform) {
    case DEVICE_PLATFORM_NVIDIA:
        retval = "NVIDIA";
        break;
    case DEVICE_PLATFORM_AMD:
        retval = "Advanced Micro Devices";
        break;
    case DEVICE_PLATFORM_INTEL:
        retval = "Intel";
        break;
    case DEVICE_PLATFORM_APPLE:
        retval = "Apple";
        break;
    case DEVICE_PLATFORM_POCL:
        retval = "pocl";
        break;
    case DEVICE_PLATFORM_MESA:
        retval = "mesa";
        break;
    case DEVICE_PLATFORM_OPENCL_ON_12:
        retval = "Microsoft";
        break;
    case DEVICE_PLATFORM_ANY:
        break;
    }
    return retval;
}

std::vector<cl::Device> DeviceManager::getDevicesForBestPlatform(
        const DeviceCriteria& deviceCriteria,
        std::vector<PlatformDevices> &platformDevices
        ) {

    bool * devicePlatformVendorMismatch = new bool[platformDevices.size()];
    for (int i = 0; i < platformDevices.size(); i++) {
        for (int j = 0; j < platformDevices[i].second.size(); j++) {
            // Check for a device-platform mismatch.
            // This happens for instance if we try to use the AMD platform on a Intel CPU
            // In this case, the Intel platform would be preferred.
            devicePlatformVendorMismatch[i] = devicePlatformMismatch(
                    platformDevices[i].second[j], platformDevices[i].first);
            if (devicePlatformVendorMismatch[i]) {
            	reportInfo() << "A device-platform mismatch was detected." << reportEnd();
            }
        }
    }

    std::vector<cl::Device>* sortedPlatformDevices = new std::vector<cl::Device>[platformDevices.size()];
    std::vector<int> platformScores(platformDevices.size(), 0);
    if (deviceCriteria.getDevicePreference() == DEVICE_PREFERENCE_NONE) {
        for(int i = 0; i < platformDevices.size(); i++) {
            sortedPlatformDevices[i] = platformDevices[i].second;
        }
    } else {
        sortDevicesAccordingToPreference(platformDevices.size(),
                deviceCriteria.getDeviceCountMaxCriteria(), platformDevices,
                deviceCriteria.getDevicePreference(), sortedPlatformDevices,
                platformScores);
    }
    // Now, finally, select the best platform and its devices by inspecting the platformDevices list
    int bestPlatform = -1;
    for (int i = 0; i < platformDevices.size(); i++) {
        if (platformDevices[i].second.size() > 0) {
            // Make sure the platform has some devices that has all criteria
            if (bestPlatform == -1) {
                bestPlatform = i;
            } else if (platformDevices[i].second.size()
                    >= deviceCriteria.getDeviceCountMinCriteria()) {
                // was enough devices found?
                // Check devicePlatformVendorMismatch
                if (devicePlatformVendorMismatch[bestPlatform] == true && devicePlatformVendorMismatch[i]
                        == false) {
                    bestPlatform = i;
                    // If there is not mismatch, choose the one with the best score
                } else if (platformScores[i] > platformScores[bestPlatform]) {
                    bestPlatform = i;
                }
            }
        }
    }
    delete[] devicePlatformVendorMismatch;

    std::vector<cl::Device> validDevices;
    if (bestPlatform == -1) {
        throw Exception("No valid OpenCL platforms!");
    } else {
        // Select the devices from the bestPlatform
        for (int i = 0; i < std::min((int)deviceCriteria.getDeviceCountMaxCriteria(), (int)sortedPlatformDevices[bestPlatform].size()); i++) {
            validDevices.push_back(sortedPlatformDevices[bestPlatform][i]);
        }
		reportInfo() << "The platform " << platformDevices[bestPlatform].first.getInfo<CL_PLATFORM_NAME>() << " was selected as the best platform." << Reporter::end();
		reportInfo() << "A total of " << validDevices.size() << " devices were selected for the context from this platform:" << Reporter::end();
        reportInfo() << "The best device was: " << validDevices[0].getInfo<CL_DEVICE_NAME>() << reportEnd();

		for (int i = 0; i < validDevices.size(); i++) {
			//reporter.report("Device " + number(i) + ": " + validDevices[i].getInfo<CL_DEVICE_NAME>(), INFO);
		}
    }
    delete[] sortedPlatformDevices;
    return validDevices;
}


bool DeviceManager::deviceSatisfiesCriteria(const DeviceCriteria& criteria, const cl::Device &device, const cl::Platform &platform) {
    bool success = true;
    if(criteria.getTypeCriteria() == DEVICE_TYPE_GPU) {
        success = device.getInfo<CL_DEVICE_TYPE>()== CL_DEVICE_TYPE_GPU;
    } else if(criteria.getTypeCriteria() == DEVICE_TYPE_CPU) {
        success =  device.getInfo<CL_DEVICE_TYPE>()== CL_DEVICE_TYPE_CPU;
    }
    if(!success)
        return false;
    if(criteria.hasCapabilityCriteria(DEVICE_CAPABILITY_OPENGL_INTEROP)) {
        success = deviceHasOpenGLInteropCapability(device, platform);
    }
    if(!success)
        return false;
    if(criteria.getPlatformCriteria() != DEVICE_PLATFORM_ANY) {
        success = getDevicePlatform(device.getInfo<CL_DEVICE_VENDOR>()) == criteria.getPlatformCriteria();
    }
    return success;
}

std::vector<PlatformDevices> DeviceManager::getDevices(
        const DeviceCriteria &deviceCriteria) {

    if (platforms.size() == 0)
        throw Exception("No OpenCL platforms installed on the system!");

    reportInfo() << "Found " << platforms.size() << " OpenCL platforms." << Reporter::end();

    // First, get all the platforms that fit the platform criteria
    std::vector<cl::Platform> validPlatforms = this->getPlatforms(deviceCriteria.getPlatformCriteria());
    reportInfo() << validPlatforms.size() << " platforms selected for inspection." << Reporter::end();

    // Create a vector of devices for each platform
    std::vector<PlatformDevices> platformDevices;
    for (int i = 0; i < validPlatforms.size(); i++) {
        std::vector<cl::Device> devices;
    	reportInfo() << "Platform " << i << ": " <<  validPlatforms[i].getInfo<CL_PLATFORM_VENDOR>() << " - " << validPlatforms[i].getInfo<CL_PLATFORM_NAME>() << " - " <<  validPlatforms[i].getInfo<CL_PLATFORM_VERSION>() << Reporter::end();

        try {
            validPlatforms[i].getDevices(CL_DEVICE_TYPE_ALL, &devices);
            reportInfo() << "This platform has " << devices.size() << " available devices in total" << reportEnd();
        } catch(cl::Error &error) {
            throw Exception("There was an error while getting OpenCL devices: " + std::string(error.what()));
        }

        if(devices.size() == 0) {
            reportInfo() << "No devices found for this platform, skipping to next." << reportEnd();
            continue;
        }

        // Next, get all devices of correct type for each of those platforms
        cl_device_type deviceType;
        if (deviceCriteria.getTypeCriteria() == DEVICE_TYPE_ANY) {
            deviceType = CL_DEVICE_TYPE_ALL;
            reportInfo() << "Looking for all types of devices." << Reporter::end();
        } else if (deviceCriteria.getTypeCriteria() == DEVICE_TYPE_GPU) {
            deviceType = CL_DEVICE_TYPE_GPU;
            reportInfo() << "Looking for GPU devices only." << Reporter::end();
        } else if (deviceCriteria.getTypeCriteria() == DEVICE_TYPE_CPU) {
            deviceType = CL_DEVICE_TYPE_CPU;
            reportInfo() << "Looking for CPU devices only." << Reporter::end();
        }
        try {
            validPlatforms[i].getDevices(deviceType, &devices);
        } catch (cl::Error &error) {
            reportError() << "There was an error while getting OpenCL devices: " + std::string(error.what()) << reportEnd();
        }
        reportInfo() << devices.size() << " selected." << Reporter::end();

        // Go through each device and see if they have the correct capabilities (if any)
        std::vector<cl::Device> acceptedDevices;
        for (int j = 0; j < devices.size(); j++) {
        	reportInfo() << "Inspecting device " << j << " with the name " << devices[j].getInfo<CL_DEVICE_NAME>() << Reporter::end();
            std::vector<DeviceCapability> capabilityCriteria = deviceCriteria.getCapabilityCriteria();
            bool accepted = true;
            for (int k = 0; k < capabilityCriteria.size(); k++) {
                if (capabilityCriteria[k] == DEVICE_CAPABILITY_OPENGL_INTEROP) {
                    if (!deviceHasOpenGLInteropCapability(devices[j], validPlatforms[i])) {
                        accepted = false;
                        reportInfo() << "Device has NOT OpenGL interop capability" << Reporter::end();
                    } else {
                        reportInfo() << "Device has OpenGL interop capability" << Reporter::end();
                    }
                }
            }
            if (accepted) {
            	reportInfo() << "The device was accepted." << Reporter::end();
                acceptedDevices.push_back(devices[j]);
            } else {
            	reportInfo() << "The device was NOT accepted." << Reporter::end();
            }
        }

        if(acceptedDevices.size() > 0)
            platformDevices.push_back(std::make_pair(validPlatforms[i], acceptedDevices));
    }

    return platformDevices;
}

std::vector<cl::Platform> DeviceManager::getPlatforms(
        DevicePlatform platformCriteria) {

    std::vector<cl::Platform> retval;
    if (platformCriteria == DEVICE_PLATFORM_ANY) {
        retval = this->platforms;
    } else {
        // Find the correct platform and add to validPlatforms
        std::string find = getDevicePlatformVendor(platformCriteria);
        for (int i = 0; i < platforms.size(); i++) {
            // TODO match on platform name also
            if (platforms[i].getInfo<CL_PLATFORM_VENDOR>().find(find) != std::string::npos) {
                retval.push_back(platforms[i]);
                break;
            }
        }
    }
    return retval;
}

void DeviceManager::setDefaultPlatform(DevicePlatform devicePlatform) {
    if(mInstance != nullptr)
        Reporter::warning() << "Main device criteria must be set before calling DeviceManager::getInstance() to have effect" << Reporter::end();
    m_devicePlatform = devicePlatform;
}

} // end namespace fast
