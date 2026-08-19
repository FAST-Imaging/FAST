#include "FAST/ProcessObject.hpp"
#include "FAST/Exception.hpp"
#include "FAST/OpenCLProgram.hpp"
#include "FAST/Streamers/Streamer.hpp"
#include <unordered_set>
#include <FAST/DataChannels/QueuedDataChannel.hpp>
#include <FAST/DataChannels/NewestFrameDataChannel.hpp>
#include <FAST/DataChannels/StaticDataChannel.hpp>
#include <FAST/OpenCL.hpp>


namespace fast {

ProcessObject::ProcessObject() : mIsModified(false) {
    mDevices[0] = DeviceManager::getInstance()->getDefaultDevice();
}

static bool isStreamer(ProcessObject* po) {
    Streamer* derived_ptr = dynamic_cast<Streamer *>(po);
    bool isStreamer = true;
    if(derived_ptr == nullptr)
        isStreamer = false;

    return isStreamer;
}

void ProcessObject::update(int executeToken) {
    // Call update on all parents
    bool newInputData = false;
    bool inputMarkedAsLastFrame = false;
    for(auto parent : mInputConnections) {
        auto port = parent.second;
        port->getProcessObject()->update(executeToken);

        if(mLastProcessed.count(parent.first) > 0) {
            // Compare the last processed data with the new data for this data port
            std::pair<DataObject::pointer, uint64_t> data = mLastProcessed[parent.first];
            if(port->hasCurrentData()) {
                auto previousData = data.first;
                auto previousTimestamp = data.second;
                try {
                    auto currentData = port->getFrame();
                    if(currentData->isLastFrame())
                        inputMarkedAsLastFrame = true;
                    auto currentTimestamp = currentData->getTimestamp();
                    //std::cout << currentData << " " << previousData << " size: " << port->getSize() << std::endl;
                    if(currentData != previousData ||
                       previousTimestamp < currentTimestamp) { // There has arrived new data, or data has changed
                        newInputData = true;
                    }
                } catch(Exception &e) {
                    reportWarning() << "Expcetion in ProcessObject: " << e.what() << reportEnd();
                }
            } else {
                // If port currently doesn't have data, check if parent is streamer. If streamer, always execute. PO will block until data arrives
                //std::cout << "check if parent of " << getNameOfClass() << " is a streamer.. " << std::endl;
                if(isStreamer(port->getProcessObject().get())) {
                    // Check if last data element was last frame or not
                    // If it is last frame, don't update
                    if(m_lastFrame.count(port->getProcessObject()->getNameOfClass()) == 0) {
                        newInputData = true;
                        //reportInfo() << "Parent is streamer, execute.." << reportEnd();
                    } else {
                        //reportInfo() << "Parent is streamer but last frame has been sent: don't execute" << reportEnd();
                    }
                } else {
                    // TODO should not be possible?
                    reportError() << "Impossible event in ProcessObject::update of " << getNameOfClass() << reportEnd();
                }
            }
        } else {
            //std::cout << "" << getNameOfClass() << " first time execute.. " << std::endl;
            // First time executing, always execute in this case, unless execute on last frame
            if(port->hasCurrentData()) {
                if(port->getFrame()->isLastFrame())
                    inputMarkedAsLastFrame = true;
            }
            newInputData = true;
        }
    }

    // Set streaming mode for output connections
    // Also remove dead output ports if any
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto&& outputPorts : mOutputConnections) {
            std::vector<int> deadOutputPorts;
            for (int i = 0; i < outputPorts.second.size(); ++i) {
                auto output = outputPorts.second[i];
                if (!output.expired()) {
                    DataChannel::pointer port = output.lock();
                }
                else {
                    deadOutputPorts.push_back(i);
                }
            }
            for (auto deadLink : deadOutputPorts)
                outputPorts.second.erase(outputPorts.second.begin() + deadLink);
        }
    }

    // If execute token is enabled (its positive), check if current token is equal to last; if so don't reexecute.
    if(executeToken >= 0 && m_lastExecuteToken == executeToken)
        return;
    // If execute on last frame only is ON, but no input data is marked as last frame, don't execute
    if(m_executeOnLastFrameOnly && !inputMarkedAsLastFrame)
        return;
    // If this object is modified, or any parents has new data for this PO: Call execute
    if(mIsModified || newInputData) {
        this->mRuntimeManager->startRegularTimer("execute");
        // set isModified to false before executing to avoid recursive update calls
        if(mIsModified) {
            reportInfo() << "EXECUTING " << getNameOfClass() << " because PO is modified." << reportEnd();
        } else if(newInputData) {
            reportInfo() << "EXECUTING " << getNameOfClass() << " because PO has new input data." << reportEnd();
        }
        mIsModified = false;
        preExecute();
        execute();
        postExecute();
        m_lastExecuteToken = executeToken;
        if(this->mRuntimeManager->isEnabled())
            this->waitToFinish();
        this->mRuntimeManager->stopRegularTimer("execute");
    }

    // Need to clear m_frameData m_lastFrame
    m_frameData.clear();
    m_lastFrame.clear();
}

DataChannel::pointer ProcessObject::getOutputPort(uint portID) {
    validateOutputPortExists(portID);
    // Create DataChannel, and it to list and return it
    DataChannel::pointer dataChannel;
    if(isStreamer(this)) {
        auto streamingMode = std::dynamic_pointer_cast<Streamer>(mPtr.lock())->getStreamingMode();
        if(streamingMode == StreamingMode::ProcessAllFrames) {
            dataChannel = QueuedDataChannel::New();
            if(m_maximumNrOfFrames > 0)
                dataChannel->setMaximumNumberOfFrames(m_maximumNrOfFrames);
        } else if(streamingMode == StreamingMode::NewestFrameOnly) {
            dataChannel = NewestFrameDataChannel::New();
        } else {
            throw Exception("Unsupported streaming mode");
        }
    } else {
        dataChannel = StaticDataChannel::New();
    }
    dataChannel->setProcessObject(std::static_pointer_cast<ProcessObject>(mPtr.lock()));

    // If this output port has current data, add it to the data channel:
    if(mOutputPorts[portID].currentData)
        dataChannel->addFrame(mOutputPorts[portID].currentData);

	std::lock_guard<std::mutex> lock(m_mutex);
    if(mOutputConnections.count(portID) == 0)
        mOutputConnections[portID] = std::vector<std::weak_ptr<DataChannel>>();

    mOutputConnections[portID].push_back(std::weak_ptr<DataChannel>(dataChannel));

    return dataChannel;
}

DataChannel::pointer ProcessObject::getInputPort(uint portID) {
    return mInputConnections.at(portID);
}

void ProcessObject::setInputConnection(uint portID, DataChannel::pointer port) {
    validateInputPortExists(portID);
    if(port->getProcessObject().get() == this)
        throw Exception("Can't set setInputConnection on self");
    mInputConnections[portID] = port;
    mIsModified = true;
}

void ProcessObject::setInputConnection(DataChannel::pointer port) {
    setInputConnection(0, port);
}

void ProcessObject::addOutputData(uint portID, DataObject::pointer data, bool propagateLastFrameData, bool propagateFrameData) {
    validateOutputPortExists(portID);

    // Copy frame data from input data
    if(propagateLastFrameData) {
        for(auto&& lastFrame : m_lastFrame) {
            data->setLastFrame(lastFrame);
        }
    }
    if(propagateFrameData)
        for(auto&& frameData : m_frameData)
            data->setFrameData(frameData.first, frameData.second);

    // Add to current data for this port
    mOutputPorts[portID].currentData = data;

    // Add it to all output connections, if any connections exist
    if(mOutputConnections.count(portID) > 0) {
        for(const auto& output : mOutputConnections.at(portID)) {
            auto port = output.lock();
            if(port)
                port->addFrame(data);
        }
    }
}

void ProcessObject::setInputData(DataObject::pointer data) {
    setInputData(0, data);
}

class EmptyProcessObject : public ProcessObject {
    FAST_OBJECT(EmptyProcessObject)
    public:
        void setOutputData(DataObject::pointer data) {
            mData = data;
            mIsModified = true;
        };
    private:
        EmptyProcessObject() {
            createOutputPort<DataObject>(0);
        };
        void execute() {
            addOutputData(0, mData);
        };
        DataObject::pointer mData;
};

void ProcessObject::setInputData(uint portID, DataObject::pointer data) {
    if(data == nullptr)
        throw Exception("Data object given to setInputData was null");
    validateInputPortExists(portID);
    auto PO = EmptyProcessObject::New();
    PO->setOutputData(data);
    setInputConnection(portID, PO->getOutputPort());
    setModified(true);
}

void ProcessObject::preExecute() {
    // Validate that all required input connections have been set
    for(auto input : mInputPorts) {
        if(input.second.required) { // if required
            if(mInputConnections.count(input.first) == 0) {
                throw Exception("Input port " + std::to_string(input.first) + " on process object " + getNameOfClass() + " is missing its required connection.");
            }
        }
    }
}

void ProcessObject::validateInputPortExists(uint portID) {
    if(mInputPorts.count(portID) == 0)
        throw Exception(getNameOfClass() + " has no input port with ID " + std::to_string(portID));
}

void ProcessObject::validateOutputPortExists(uint portID) {
    if(mOutputPorts.count(portID) == 0)
        throw Exception(getNameOfClass() + " has no output port with ID " + std::to_string(portID));
}

void ProcessObject::postExecute() {
    /*
    // TODO Release input data if they are marked as "release after execute"
    std::unordered_map<uint, bool>::iterator it;
    for(it = mReleaseAfterExecute.begin(); it != mReleaseAfterExecute.end(); it++) {
        if(it->second) {
            std::vector<uint>::iterator it2;
            for(it2 = mInputDevices[it->first].begin(); it2 != mInputDevices[it->first].end(); it2++) {
                DataObject::pointer data = getInputData(it->first);
                data->release(getDevice(*it2));
            }
        }
    }
     */
}

void ProcessObject::changeDeviceOnInputs(uint deviceNumber, ExecutionDevice::pointer device) {
    /*
    // TODO For each input, release old device and retain on new device
    std::unordered_map<uint, ProcessObjectPort>::iterator it;
    for(it = mInputConnections.begin(); it != mInputConnections.end(); it++) {
        std::vector<uint>::iterator it2;
        for(it2 = mInputDevices[it->first].begin(); it2 != mInputDevices[it->first].end(); it2++) {
            if(*it2 == deviceNumber) {
                getInputData(it->first)->release(mDevices[deviceNumber]);
                getInputData(it->first)->retain(device);
            }
        }
    }
     */
}

void ProcessObject::setMainDevice(ExecutionDevice::pointer device) {
    setDevice(0, device);
}

ExecutionDevice::pointer ProcessObject::getMainDevice() const {
    return mDevices.at(0);
}

void ProcessObject::setDevice(uint deviceNumber,
        ExecutionDevice::pointer device) {
    if(mDeviceCriteria.count(deviceNumber) > 0) {
        if(!DeviceManager::getInstance()->deviceSatisfiesCriteria(std::dynamic_pointer_cast<OpenCLDevice>(device), mDeviceCriteria[deviceNumber]))
            throw Exception("Tried to set device which does not satisfy device criteria");
    }
    if(mDevices.count(deviceNumber) > 0) {
        changeDeviceOnInputs(deviceNumber, device);
    }
    mDevices[deviceNumber] = device;
}

ExecutionDevice::pointer ProcessObject::getDevice(uint deviceNumber) const {
    return mDevices.at(deviceNumber);
}

void ProcessObject::setMainDeviceCriteria(const DeviceCriteria& criteria) {
    mDeviceCriteria[0] = criteria;
    mDevices[0] = DeviceManager::getInstance()->getDevice(criteria);
}

void ProcessObject::setDeviceCriteria(uint deviceNumber,
        const DeviceCriteria& criteria) {
    mDeviceCriteria[deviceNumber] = criteria;
    mDevices[deviceNumber] = DeviceManager::getInstance()->getDevice(criteria);
}

void ProcessObject::createOpenCLProgram(std::string sourceFilename, std::string name) {
    auto program = OpenCLProgram::New();
    program->setName(name);
    program->setSourceFilename(sourceFilename);
    mOpenCLPrograms[name] = program;
}

cl::Program ProcessObject::getOpenCLProgram(
        OpenCLDevice::pointer device,
        std::string name,
        std::string buildOptions
        ) {

    if(mOpenCLPrograms.count(name) == 0) {
        throw Exception("OpenCL program with the name " + name + " not found in " + getNameOfClass());
    }

    OpenCLProgram::pointer program = mOpenCLPrograms[name];
    return program->build(device, buildOptions);
}

int ProcessObject::getNrOfInputConnections() const {
    return mInputConnections.size();
}

void ProcessObject::stopPipeline() {
    for(auto input : mInputConnections) {
        input.second->stop();
        input.second->getProcessObject()->stopPipeline(); // Stop parent POs
    }

}

bool ProcessObject::hasNewInputData(uint portID) {
    return mInputConnections.at(portID)->hasCurrentData();
}

int ProcessObject::getNrOfOutputPorts() const {
    return mOutputPorts.size();
}

void ProcessObject::setModified(bool modified) {
    mIsModified = modified;

}

void ProcessObject::createInputPort(uint portID, std::string name, std::string description, bool required) {
    InputPort inputPort;
    inputPort.required = required;
    inputPort.name = name;
    inputPort.description = description;
    mInputPorts[portID] = inputPort;
}

void ProcessObject::createOutputPort(uint portID, std::string name, std::string description) {
    OutputPort outputPort;
    outputPort.name = name;
    outputPort.description = description;
    mOutputPorts[portID] = outputPort;
}

void ProcessObject::addOutputData(DataObject::pointer data, bool propagateLastFrameData, bool propagateFrameData) {
    addOutputData(0, data, propagateLastFrameData, propagateFrameData);
}

int ProcessObject::getNrOfInputPorts() const {
    return mInputPorts.size();
}

std::shared_ptr<ProcessObject> ProcessObject::run(int64_t executeToken) {
    update(executeToken);
    return std::static_pointer_cast<ProcessObject>(mPtr.lock());
}

std::shared_ptr<ProcessObject> ProcessObject::connect(std::shared_ptr<ProcessObject> parentProcessObject, uint outputPortID) {
    return connect(0, std::move(parentProcessObject), outputPortID);
}

std::shared_ptr<ProcessObject> ProcessObject::connect(uint inputPortID, std::shared_ptr<ProcessObject> parentProcessObject, uint outputPortID) {
    setInputConnection(inputPortID, parentProcessObject->getOutputPort(outputPortID));
    return std::static_pointer_cast<ProcessObject>(mPtr.lock());
}

std::shared_ptr<ProcessObject> ProcessObject::connect(std::shared_ptr<DataObject> inputDataObject) {
    return connect(0, inputDataObject);
}

std::shared_ptr<ProcessObject> ProcessObject::connect(uint inputPortID, std::shared_ptr<DataObject> inputDataObject) {
    setInputData(inputPortID, inputDataObject);
    return std::static_pointer_cast<ProcessObject>(mPtr.lock());
}

int ProcessObject::getLastExecuteToken() const {
    return m_lastExecuteToken;
}

void ProcessObject::setExecuteOnLastFrameOnly(bool executeOnLastFrameOnly) {
    m_executeOnLastFrameOnly = true;
}

bool ProcessObject::getExecuteOnLastFrameOnly() const {
    return m_executeOnLastFrameOnly;
}

DataObject::pointer ProcessObject::getOutputData(uint portID) {
    validateOutputPortExists(portID);

    auto data = mOutputPorts[portID].currentData;
    if(!data)
        throw Exception("Error in getOutputData: Process object has not produced any output data");

    return data;
}

std::shared_ptr<DataObject> ProcessObject::runAndGetOutputData(uint portID, int64_t executeToken) {
    auto port = getOutputPort(portID);
    run(executeToken);
    return port->getNextFrame();
}

bool ProcessObject::hasReceivedLastFrameFlag() const {
    bool lastFrame = false;
    for(auto input : mInputConnections) {
        if(input.second->getFrame()->isLastFrame())
            lastFrame = true;
    }
    return lastFrame;
}

Kernel ProcessObject::getKernel(std::string name, std::string programName, std::string buildOptions, OpenCLDevice::pointer device) {
    if(!device)
        device = getMainOpenCLDevice();
    auto program = getOpenCLProgram(device, programName, buildOptions);
    auto kernel = cl::Kernel(program, name.c_str());
    return Kernel(kernel, device);
}

Queue ProcessObject::getQueue(OpenCLDevice::pointer device) {
    if(!device) {
        device = std::dynamic_pointer_cast<OpenCLDevice>(getMainDevice());
        if(!device)
            throw Exception("Main device was set to Host. Cannot run OpenCL on Host");
    }
    return Queue(device->getCommandQueue());
}

OpenCLDevice::pointer ProcessObject::getMainOpenCLDevice() const {
    auto device = std::dynamic_pointer_cast<OpenCLDevice>(getMainDevice());
    if(!device)
        throw Exception("Cannot getMainOpenCLDevice because main device was Host");
    return device;
}

void ProcessObject::createInlineOpenCLProgram(std::string sourceCode, std::string name) {
    auto program = OpenCLProgram::New();
    program->setName(name);
    program->setSourceCode(sourceCode);
    mOpenCLPrograms[name] = program;
}

OpenCLBuffer ProcessObject::createBuffer(std::size_t size, KernelMemoryAccess kernelAccess, HostMemoryAccess hostAccess,
                        OpenCLDevice::pointer device) const {
    return createBuffer(size, nullptr, kernelAccess, hostAccess, device);
}

OpenCLBuffer ProcessObject::createBuffer(std::size_t size, void* data, KernelMemoryAccess kernelAccess, HostMemoryAccess hostAccess,
                    OpenCLDevice::pointer device) const {
    if(!device)
        device = getMainOpenCLDevice();
    return OpenCLBuffer(size, device, kernelAccess, hostAccess, data);
}

DataObject::pointer ProcessObject::getOutput(uint portID) {
    validateOutputPortExists(portID);

    auto data = mOutputPorts[portID].currentData;
    if(!data)
        throw Exception("Error in getOutputData: Process object has not produced any output data. Did you forget to call run()?");

    return data;
}

} // namespace fast
