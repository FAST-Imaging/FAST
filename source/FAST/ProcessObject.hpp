#pragma once

#include <FAST/Utility.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <FAST/Object.hpp>
#include <FAST/Data/DataObject.hpp>
#include <FAST/ExecutionDevice.hpp>
#include <FAST/DeviceManager.hpp>
#include <FAST/Config.hpp>
#include <FAST/DataChannels/DataChannel.hpp>
#include <FAST/DataStream.hpp>
#include <FAST/OpenCLProgram.hpp>

namespace fast {


/**
 * @defgroup segmentation Segmentation
 * Objects and functions for image segmentation.
 */

/**
 * @defgroup filter Filter
 * Objects and functions for image filtering.
 */

/**
 * @defgroup registration Registration
 * Objects and functions for image and feature registration.
 */

/**
 * @defgroup motion-and-tracking Motion and Tracking
 * Objects and functions for motion and tracking.
 */

/**
 * @defgroup morphology Morphology
 * Objects and functions for morphology
 */

/**
 * @defgroup bounding-box Bounding Box
 * Objects and functions for bounding box detection etc.
 */

/**
 * @defgroup ultrasound Ultrasound
 * Objects and functions used for ultrasound imaging.
 */

/**
 * @defgroup wsi Whole Slide Images (WSI)
 * Objects and functions used for whole slide images (WSI) in digital pathology.
 */

/**
 * @defgroup draw Drawing
 * Objects for drawing circles, splines etc. on images.
 */

/**
 * @defgroup image-comparison Image Comparison
 * Objects for comparing and calculating the similarity of two images.
 */

/**
 * @brief Abstract base class for all process objects
 */
class FAST_EXPORT ProcessObject : public AttributeObject {
    public:
        /**
         * @brief Update/Run the pipeline up to this process object.
         *
         * Do update on this PO, which will trigger update on all connected POs
         * thus running the entire pipeline.
         *
         * An optional executeToken can be used to synchronize updating to avoid
         * duplicate execution for the same frames when using streaming.
         * Increment the token for every timestep with a positive value.
         *
         * @param executeToken Negative value means that the execute token is disabled.
         */
#ifndef SWIG
        [[deprecated("Use ProcessObject::run() instead of ProcessObject::update().")]]
#endif
        void update(int executeToken = -1);
        typedef std::shared_ptr<ProcessObject> pointer;

        // Device stuff
        void setMainDevice(ExecutionDevice::pointer device);
        void setMainDeviceCriteria(const DeviceCriteria& citeria);
        ExecutionDevice::pointer getMainDevice() const;
        void setDevice(uint deviceNumber, ExecutionDevice::pointer device);
        void setDeviceCriteria(uint deviceNumber, const DeviceCriteria& criteria);
        ExecutionDevice::pointer getDevice(uint deviceNumber) const;

        virtual DataChannel::pointer getOutputPort(uint portID = 0);
        virtual DataChannel::pointer getInputPort(uint portID = 0);
        virtual void setInputConnection(DataChannel::pointer port);
        virtual void setInputConnection(uint portID, DataChannel::pointer port);
        virtual void setInputData(DataObject::pointer data);
        virtual void setInputData(uint portID, DataObject::pointer data);
        /**
         * Get current output data for a given port
         * @param portID
         * @return
         */
        DataObject::pointer getOutputData(uint portID = 0);
        /**
         * Get current output data for a given port
         * @tparam DataType data type to convert to
         * @param portID
         * @return
         */
        template <class DataType>
        std::shared_ptr<DataType> getOutputData(uint portID = 0);
        /**
          * Get current output data for a given port
          * @param portID
          * @return
          */
        DataObject::pointer getOutput(uint portID = 0);
        /**
         * Get current output data for a given port
         * @tparam DataType data type to convert to
         * @param portID
         * @return
         */
        template <class DataType>
        std::shared_ptr<DataType> getOutput(uint portID = 0);
        std::map<uint, DataChannel::pointer> getInputConnections() const;
        int getNrOfInputConnections() const;
        int getNrOfOutputPorts() const;
        int getNrOfInputPorts() const;

        static std::string getStaticNameOfClass() {
            return "ProcessObject";
        }

        /**
         * @brief Stop a pipeline.
         */
        virtual void stopPipeline();

        /**
         * @brief Mark this process object as modified or not.
         * A modified PO will execute next time it is updated.
         *
         * @param modified
         */
        void setModified(bool modified);
        bool isModified();

        template <class DataType>
        std::shared_ptr<DataType> updateAndGetOutputData(uint portID = 0);

        template <class DataType>
        std::shared_ptr<DataType> runAndGetOutputData(uint portID = 0, int64_t executeToken = -1);
        std::shared_ptr<DataObject> runAndGetOutputData(uint portID = 0, int64_t executeToken = -1);

        //// NEW V4 PO SEMANTICS
        std::shared_ptr<ProcessObject> run(int64_t executeToken = -1);
        std::shared_ptr<ProcessObject> connect(std::shared_ptr<ProcessObject> parentProcessObject, uint outputPortID = 0);
        std::shared_ptr<ProcessObject> connect(uint inputPortID, std::shared_ptr<ProcessObject> parentProcessObject, uint outputPortID = 0);
        std::shared_ptr<ProcessObject> connect(std::shared_ptr<DataObject> inputDataObject);
        std::shared_ptr<ProcessObject> connect(uint inputPortID, std::shared_ptr<DataObject> inputDataObject);
        int getLastExecuteToken() const;
        /**
         * If set to true, this will only trigger this PO to execute if one of its inputs is marked as being "last frame".
         * This is useful if one want to export the results of a PatchStitcher, but only when it is complete.
         *
         * @param executeOnLastFrameOnly
         */
        void setExecuteOnLastFrameOnly(bool executeOnLastFrameOnly);
        bool getExecuteOnLastFrameOnly() const;

        /**
         * @brief Convert attributes to string
         * @return
         */
        virtual std::string attributesToString() { return ""; };

        /**
         * @brief Whether this PO has received input data with last frame flag set
         * @return
         */
        bool hasReceivedLastFrameFlag() const;
    protected:
        ProcessObject();
        // Flag to indicate whether the object has been modified
        // and should be executed again
        std::atomic_bool mIsModified;

        // An integer id which act as a token of when this PO last executed
        int m_lastExecuteToken = -1;

        // Pure virtual method for executing the pipeline object
        virtual void execute()=0;
        virtual void preExecute();
        virtual void postExecute();

        void createInputPort(uint portID, std::string name = "", std::string description = "", bool required = true);
        void createOutputPort(uint portID, std::string name = "", std::string description = "");
        template <class DataType>
        void createInputPort(uint portID, bool required = true);
        template <class DataType>
        void createOutputPort(uint portID);

        template <class DataType>
        std::shared_ptr<DataType> getInputData(uint portID = 0, bool readFrameData = true);
        void addOutputData(DataObject::pointer data, bool propagateLastFrameData = true, bool propagateFrameData = true);
        void addOutputData(uint portID, DataObject::pointer data, bool propagateLastFrameData = true, bool propagateFrameData = true);

        bool hasNewInputData(uint portID);

        virtual void waitToFinish() {};

        cl::Program getOpenCLProgram(
                std::shared_ptr<OpenCLDevice> device,
                std::string name = "",
                std::string buildOptions = ""
        );

        void changeDeviceOnInputs(uint deviceNumber, ExecutionDevice::pointer device);

        std::map<uint, bool> mRequiredInputs;
        std::map<uint, std::vector<uint> > mInputDevices;
        std::map<uint, ExecutionDevice::pointer> mDevices;
        std::map<uint, DeviceCriteria> mDeviceCriteria;

        // New pipeline
        std::map<uint, DataChannel::pointer> mInputConnections;
        std::map<uint, std::vector<std::weak_ptr<DataChannel>>> mOutputConnections;
        struct InputPort {
            std::string name;
            std::string description;
            bool required = true;
        };
        std::map<uint, InputPort> mInputPorts;
        struct OutputPort {
            std::string name;
            std::string description;
            DataObject::pointer currentData = nullptr;
        };
        std::map<uint, OutputPort> mOutputPorts;
        // <port id, timestep>, register the last timestep of input data which this PO executed with
        std::map<uint, std::pair<DataObject::pointer, uint64_t>> mLastProcessed;

        void validateInputPortExists(uint portID);
        void validateOutputPortExists(uint portID);

        std::unordered_map<std::string, std::shared_ptr<OpenCLProgram> > mOpenCLPrograms;

        // Frame data
        // Similar to metadata, only this is transferred from input to output
        std::unordered_map<std::string, std::string> m_frameData;
        // Indicates whether this data object is the last frame in a stream, and if so, the name of the stream
        std::unordered_set<std::string> m_lastFrame;

        int m_maximumNrOfFrames = -1;
        bool m_executeOnLastFrameOnly = false;

        std::mutex m_mutex;

        // OpenCL
        /**
         * @brief Get OpenCL Kernel from a OpenCL program
         * Get an OpenCL Kernel from an OpenCL program created in this process object using createOpenCLProgram or createInlineOpenCLProgram
         * @param name
         * @param programName
         * @param buildOptions
         * @param device
         * @return
         */
        Kernel getKernel(std::string name, std::string programName = "", std::string buildOptions = "", OpenCLDevice::pointer device = nullptr);
        Queue getQueue(OpenCLDevice::pointer device = nullptr);
        OpenCLDevice::pointer getMainOpenCLDevice() const;
        /**
         * @brief Create OpenCL program from a file
         * @param sourceFilename
         * @param name
         */
        void createOpenCLProgram(std::string sourceFilename, std::string name = "");
        /**
         * @brief Create OpenCL program from source code string
         * @param sourceCode
         * @param name
         */
        void createInlineOpenCLProgram(std::string sourceCode, std::string name = "");
        /**
         * @brief Create an uninitialized OpenCL buffer
         * @param size
         * @param kernelAccess
         * @param hostAccess
         * @param device If no device is provided, the main device for this process object is used.
         * @return OpenCL buffer
         */
        OpenCLBuffer createBuffer(
                std::size_t size,
                KernelMemoryAccess kernelAccess = KernelMemoryAccess::READ_WRITE,
                HostMemoryAccess hostAccess = HostMemoryAccess::UNSPECIFIED,
                OpenCLDevice::pointer device = nullptr
        ) const;
        /**
        * @brief Create an OpenCL buffer of the provided data
        * @param data
        * @param kernelAccess
        * @param hostAccess
        * @param device If no device is provided, the main device for this process object is used.
        * @return OpenCL buffer
        */
        template <class T>
        OpenCLBuffer createBuffer(
                const std::vector<T>& data,
                KernelMemoryAccess kernelAccess = KernelMemoryAccess::READ_WRITE,
                HostMemoryAccess hostAccess = HostMemoryAccess::UNSPECIFIED,
                OpenCLDevice::pointer device = nullptr
        ) const;
#ifndef SWIG
        /**
         * @brief Create an OpenCL buffer of the provided data
         * @param size data size in number of bytes
         * @param data pointer to data
         * @param kernelAccess
         * @param hostAccess
         * @param device If no device is provided, the main device for this process object is used.
         * @return OpenCL Buffer
         */
        OpenCLBuffer createBuffer(
                std::size_t size,
                void* data,
                KernelMemoryAccess kernelAccess = KernelMemoryAccess::READ_WRITE,
                HostMemoryAccess hostAccess = HostMemoryAccess::UNSPECIFIED,
                OpenCLDevice::pointer device = nullptr
        ) const;
#endif
        friend class ComputationThread;
};

template<class DataType>
void ProcessObject::createInputPort(uint portID, bool required) {
    InputPort inputPort;
    inputPort.required = required;
    mInputPorts[portID] = inputPort;
}

template<class DataType>
void ProcessObject::createOutputPort(uint portID) {
    OutputPort outputPort;
    mOutputPorts[portID] = outputPort;
}

template<class DataType>
std::shared_ptr<DataType> ProcessObject::getInputData(uint portID, bool readFrameData) {
    validateInputPortExists(portID);
    DataChannel::pointer port = mInputConnections.at(portID);
    DataObject::pointer data = port->getNextFrame();
    mLastProcessed[portID] = std::make_pair(data, data->getTimestamp());
    auto convertedData = std::dynamic_pointer_cast<DataType>(data);
    // Check if the conversion went ok
    if(!convertedData)
        throw BadCastException(data->getNameOfClass(), DataType::getStaticNameOfClass());

    // Store frame data for this input data so it can be added to output data later
    if(readFrameData) {
        for(auto&& lastFrame : data->getLastFrame())
            m_lastFrame.insert(lastFrame);
        for(auto&& frameData : data->getFrameData())
            m_frameData[frameData.first] = frameData.second;
    }

    return convertedData;
}

template<class DataType>
std::shared_ptr<DataType> ProcessObject::getOutputData(uint portID) {
    return getOutput<DataType>(portID);
}

template<class DataType>
std::shared_ptr<DataType> ProcessObject::getOutput(uint portID) {
    auto data = getOutput(portID);
    auto convertedData = std::dynamic_pointer_cast<DataType>(data);
    // Check if the conversion went ok
    if(!convertedData)
        throw BadCastException(data->getNameOfClass(), DataType::getStaticNameOfClass());

    return convertedData;
}

template<class DataType>
std::shared_ptr<DataType> ProcessObject::updateAndGetOutputData(uint portID) {
    return runAndGetOutputData<DataType>();
}

template<class DataType>
std::shared_ptr<DataType> ProcessObject::runAndGetOutputData(uint portID, int64_t executeToken) {
    auto data = runAndGetOutputData(portID, executeToken);
    auto castedObject = std::dynamic_pointer_cast<DataType>(data);
    if(!castedObject)
        throw BadCastException(data->getNameOfClass(), DataType::getStaticNameOfClass());
    return castedObject;
}


template<class T>
OpenCLBuffer ProcessObject::createBuffer(const std::vector<T>& data, KernelMemoryAccess kernelAccess, HostMemoryAccess hostAccess,
                                         OpenCLDevice::pointer device) const {
    if(!device)
        device = getMainOpenCLDevice();
    return OpenCLBuffer(data.size()*sizeof(T), device, kernelAccess, hostAccess, data.data());
}

}; // end namespace fast
