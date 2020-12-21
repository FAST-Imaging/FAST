#include "TensorRTEngine.hpp"
#include "NvInfer.h"
#include "NvUffParser.h"
#include "NvOnnxParser.h"
#include "NvUtils.h"
#include <cuda_runtime_api.h>
#include <FAST/Utility.hpp>
#include <fstream>
#include <FAST/Config.hpp>

#define CUDA_CHECK(status)                             \
    do                                            \
    {                                             \
        auto ret = (status);                      \
        if (ret != 0)                             \
        {                                         \
            std::cout << "Cuda failure: " << ret; \
            abort();                              \
        }                                         \
    } while (0)


namespace fast {

/**
 * Logger object for TensorRT
 */
class Logger : public nvinfer1::ILogger {
    void log(Severity severity, const char* msg) override {
        switch(severity) {
            case Severity::kINFO:
                Reporter::info() << msg << Reporter::end();
                break;
            case Severity::kERROR:
            case Severity::kINTERNAL_ERROR:
                Reporter::error() << msg << Reporter::end();
                break;
            case Severity::kWARNING:
                Reporter::warning() << msg << Reporter::end();
                break;
        }
    }
} gLogger;

// Call destroy when an object is deleted
struct Destroy {
    template <typename T>
    void operator()(T* obj) const {
        if (obj)
            obj->destroy();
    }
};

inline unsigned int elementSize(nvinfer1::DataType t) {
    switch (t)
    {
        case nvinfer1::DataType::kINT32:
            // Fallthrough, same as kFLOAT
        case nvinfer1::DataType::kFLOAT: return 4;
        case nvinfer1::DataType::kHALF: return 2;
        case nvinfer1::DataType::kINT8: return 1;
    }
    assert(0);
    return 0;
}


inline int64_t volume(const nvinfer1::Dims& d) {
    int64_t v = 1;
    for (int64_t i = 0; i < d.nbDims; i++)
        v *= d.d[i];
    return v;
}


static std::vector<std::pair<int64_t, nvinfer1::DataType>>
calculateBindingBufferSizes(const nvinfer1::ICudaEngine& engine, int nbBindings, int batchSize) {
    std::vector<std::pair<int64_t, nvinfer1::DataType>> sizes;
    for (int i = 0; i < nbBindings; ++i)
    {
        nvinfer1::Dims dims = engine.getBindingDimensions(i);
        nvinfer1::DataType dtype = engine.getBindingDataType(i);

        int64_t eltCount = volume(dims) * batchSize;
        sizes.push_back(std::make_pair(eltCount, dtype));
    }

    return sizes;
}

void* safeCudaMalloc(size_t memSize)
{
    void* deviceMem;
    CUDA_CHECK(cudaMalloc(&deviceMem, memSize));
    if (deviceMem == nullptr)
    {
        std::cerr << "Out of memory" << std::endl;
        exit(1);
    }
    return deviceMem;
}



void TensorRTEngine::run() {

    const int nbBindings = m_engine->getNbBindings();

    // Get batch size
    const int batchSize = mInputNodes.begin()->second.data->getShape()[0];
    if(batchSize > m_engine->getMaxBatchSize()) {
        reportWarning() << "Batch is larger than the max batch size given to TensorRT" << reportEnd();
    }

    std::vector<void*> buffers(nbBindings);
    auto buffersSizes = calculateBindingBufferSizes(*m_engine, nbBindings, batchSize);

    // TODO assuming 1 input and output here
    std::map<std::string, int> inputIndexes;
    std::map<std::string, int> outputIndexes;
    for(int i = 0; i < nbBindings; ++i) {
        auto name = m_engine->getBindingName(i);
        if (m_engine->bindingIsInput(i)) {
            inputIndexes[name] = i;
        } else {
            outputIndexes[name] = i;
        }
        // Allocate data
        buffers[i] = safeCudaMalloc(buffersSizes[i].first * elementSize(buffersSizes[i].second));
    }

    // Allocate data for each input and copy data to it
    for(const auto& inputNode : mInputNodes) {
        auto tensor = inputNode.second.data;
        auto access = tensor->getAccess(ACCESS_READ);
        float* tensorData = access->getRawData();
        const int index = inputIndexes.at(inputNode.first);
        CUDA_CHECK(cudaMemcpy(buffers[index], tensorData,
                              buffersSizes[index].first * elementSize(buffersSizes[index].second),
                              cudaMemcpyHostToDevice));
        reportInfo() << "Finished copying input data to TensorRT" << reportEnd();
    }

    // Execute network
    //bool success = m_context->executeV2(buffers.data()); // IMPORTANT: Does not work with uff parser atm
    bool success = m_context->execute(batchSize, &buffers[0]);
    if(!success)
        throw Exception("TensorRT execute inference failed!");
    reportInfo() << "Finished execute TensorRT" << reportEnd();

    // Free input memory buffers
    for(const auto& input : inputIndexes)
        CUDA_CHECK(cudaFree(buffers[input.second]));
    reportInfo() << "Finished freeing input data TensorRT" << reportEnd();

    // Transfer output data back
    for(const auto& output : outputIndexes) {
        const int index = output.second;
        reportInfo() << "Processing output node " << output.first << reportEnd();
        auto outputData = make_uninitialized_unique<float[]>(buffersSizes[index].first);
        CUDA_CHECK(cudaMemcpy(outputData.get(), buffers[index],
                              buffersSizes[index].first * elementSize(buffersSizes[index].second),
                              cudaMemcpyDeviceToHost));

        auto outputTensor = Tensor::New();
        mOutputNodes.at(output.first).data = outputTensor;

        // Get output shape
        nvinfer1::Dims dims = m_engine->getBindingDimensions(index);
        TensorShape shape = mOutputNodes.at(output.first).shape;
        if(shape.getDimensions() == 0)
            throw Exception("Missing shape for output node");
        shape[0] = batchSize;
        // TODO check if output dims are correct:
        //for(int i = 0; i < dims.nbDims; ++i) {
        //}

        outputTensor->create(std::move(outputData), shape);
        reportInfo() << "Finished moving data to FAST tensor, TensorRT" << reportEnd();
        reportInfo() << "Finished transfer of output data TensorRT" << reportEnd();
        CUDA_CHECK(cudaFree(buffers[index]));
        reportInfo() << "Finished freeing output data TensorRT" << reportEnd();
    }
}

static TensorShape getTensorShape(nvinfer1::Dims dims) {
    TensorShape shape;
    for(int j = 0; j < dims.nbDims; ++j) {
        auto size = dims.d[j];
        shape.addDimension(size);
    }
    return shape;
}

void TensorRTEngine::load() {

    const auto filename = getFilename();
    std::size_t hash = std::hash<std::string>{}(filename + std::to_string(m_maxBatchSize)); // Hash the full filename any other parameters
    std::string serializedBinaryFilename = join(Config::getKernelBinaryPath(), filename.substr(filename.rfind('/')) + "_" + std::to_string(hash) + ".bin");
    std::string serializedCacheFilename = join(Config::getKernelBinaryPath(), filename.substr(filename.rfind('/')) + "_" + std::to_string(hash) + ".cache");
    bool loadSerializedFile = false;
    if(fileExists(serializedCacheFilename) && fileExists(serializedBinaryFilename)) {
        // Check if date is modified or not
        std::ifstream file(serializedCacheFilename.c_str());
        if(file.fail())
            throw Exception("Failed to read " + serializedCacheFilename);
        std::string line = "";
        std::getline(file, line);
        trim(line);
        std::string modifiedDate = getModifiedDate(filename);
        trim(modifiedDate);
        if(line == modifiedDate) {
            reportInfo() << "Serialized file " << serializedBinaryFilename << " is up to date." << reportEnd();
            loadSerializedFile = true;
        } else {
            reportInfo() << "Serialized file " << serializedBinaryFilename << " was not up to date." << reportEnd();
        }
    }
    if(!loadSerializedFile) {
        // File is not serialized: Create it
        if(!fileExists(filename))
            throw FileNotFoundException(filename);
        reportInfo() << "Loading file " << filename << " using TensorRT" << reportEnd();
        std::unique_ptr<nvinfer1::IBuilder, decltype(Destroy())> builder(nvinfer1::createInferBuilder(gLogger),
                                                                         Destroy());
        const auto flags = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
        std::unique_ptr<nvinfer1::INetworkDefinition, decltype(Destroy())> network;
        std::unique_ptr<nvuffparser::IUffParser, decltype(Destroy())> uffparser(nvuffparser::createUffParser(),
                                                                             Destroy()); // IMPORTANT! UFF parser must be alive even after parse
        if(filename.substr(filename.size()-4) == ".uff") {
            network = {builder->createNetwork(),
                       Destroy()}; // createNetworkV2 not working with UFF for some reason..
        } else {
            network = {builder->createNetworkV2(flags),
                       Destroy()};
        }
        std::unique_ptr<nvonnxparser::IParser, decltype(Destroy())> onnxparser(nvonnxparser::createParser(*network, gLogger),
                                                                           Destroy()); // IMPORTANT! Parser must be alive even after parse

        if(filename.substr(filename.size()-4) == ".uff") {
            // Check that input and output nodes are set
            if(mInputNodes.empty() || mOutputNodes.empty())
                throw Exception("Input and output nodes must be defined before loading Uff files using the TensorRT engine");
            reportInfo() << "Assuming file is in UFF format, parsing..." << reportEnd();

            // Setup input nodes
            reportInfo() << "Going through nodes.." << reportEnd();
            for(auto node : mInputNodes) {
                reportInfo() << node.first << reportEnd();
                auto shape = node.second.shape;
                if(shape.getDimensions() == 0)
                    throw Exception("Unknown shape for input node " + node.first +
                                    ". For TensorRT you need to specify the input tensor shape explictly with addInputNode");
                if(shape.getDimensions() == 4)
                    uffparser->registerInput(node.first.c_str(), nvinfer1::Dims3(shape[1], shape[2], shape[3]),
                                          nvuffparser::UffInputOrder::kNCHW);
                if(shape.getDimensions() == 5)
                    throw Exception("More than 4 dimensions input is not supported");
            }
            reportInfo() << "Input nodes finished" << reportEnd();

            // Setup output nodes
            for(auto node : mOutputNodes) {
                uffparser->registerOutput(node.first.c_str());
            }
            reportInfo() << "Output nodes finished" << reportEnd();

            if(!uffparser->parse(filename.c_str(), *network, nvinfer1::DataType::kFLOAT))
                throw Exception("Error parsing UFF file " + filename);

            reportInfo() << "Finished parsing UFF file" << reportEnd();
        } else {
            // Assuming file is ONNX format
            reportInfo() << "Assuming file is in ONNX format, parsing..." << reportEnd();
            bool parsed = onnxparser->parseFromFile(filename.c_str(), 1);
            if(!parsed)
                throw Exception("Unable to parse ONNX file with TensorRT");
        }

        builder->setMaxBatchSize(m_maxBatchSize);
        //builder->setFp16Mode(builder->platformHasFastFp16());

        std::unique_ptr<nvinfer1::IBuilderConfig, decltype(Destroy())> config(builder->createBuilderConfig(), Destroy());
        config->setMaxWorkspaceSize(m_maxWorkspaceSize);

        m_engine = builder->buildEngineWithConfig(*network, *config);
        if(!m_engine)
            throw Exception("Failed to build CUDA engine for TensorRT");
        reportInfo() << "Finished building CUDA engine for TensorRT" << reportEnd();

        // Serialize the model
        std::unique_ptr<nvinfer1::IHostMemory, decltype(Destroy())> serializedModel(m_engine->serialize(), Destroy());
        // Store model to disk
        std::ofstream ofile(serializedBinaryFilename.c_str(), std::ios::binary);
        ofile.write((char *) serializedModel->data(), serializedModel->size());
        ofile.close();
        std::ofstream ofileCache(serializedCacheFilename.c_str());
        std::string modifiedDate = getModifiedDate(filename);
        ofileCache.write(modifiedDate.c_str(), modifiedDate.size());
        ofileCache.close();
    } else {
        std::unique_ptr<nvinfer1::IRuntime, decltype(Destroy())> runtime(nvinfer1::createInferRuntime(gLogger), Destroy());
        // Read serialized model from disk
        std::ifstream ifile(serializedBinaryFilename.c_str(), std::ios::binary);
        std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(ifile), {});
        ifile.close();
        // Deserialize the model data
        m_engine = runtime->deserializeCudaEngine(buffer.data(), buffer.size(), nullptr);
    }

    // Get input and output nodes from the CUDA engine
    if(filename.substr(filename.size()-4) != ".uff") {
        int inputCount = 0;
        int outputCount = 0;
        for (int i = 0; i < m_engine->getNbBindings(); ++i) {
            auto name = m_engine->getBindingName(i);
            auto shape = getTensorShape(m_engine->getBindingDimensions(i));
            NodeType type = NodeType::IMAGE;
            if (shape.getDimensions() < 4)
                type = NodeType::TENSOR;
            if (m_engine->bindingIsInput(i)) {
                reportInfo() << "Found input node " << name << " with shape " << shape.toString() << reportEnd();
                addInputNode(inputCount, name, type, shape);
                ++inputCount;
            } else {
                reportInfo() << "Found output node " << name << " with shape " << shape.toString() << reportEnd();
                addOutputNode(outputCount, name, type, shape);
                ++outputCount;
            }
        }
    }

    m_context = m_engine->createExecutionContext();
    setIsLoaded(true);
}

ImageOrdering TensorRTEngine::getPreferredImageOrdering() const {
    return ImageOrdering::ChannelFirst;
}

TensorRTEngine::~TensorRTEngine() {
    if(m_engine != nullptr)
        m_engine->destroy();
    //if(m_context != nullptr)
    //    m_context->destroy();
    //nvuffparser::shutdownProtobufLibrary(); // This cannot be called twice in the same program. Maybe use atexit instead?
}

TensorRTEngine::TensorRTEngine() {

}

std::string TensorRTEngine::getName() const {
    return "TensorRT";
}

void TensorRTEngine::setMaxBatchSize(int maxBatchSize) {
    if(maxBatchSize <= 0)
        throw Exception("Max batch size must be > 0");
    m_maxBatchSize = maxBatchSize;
}

int TensorRTEngine::getMaxBatchSize() const {
    return m_maxBatchSize;
}

std::string TensorRTEngine::getDefaultFileExtension() const {
    return "uff";
}

}
