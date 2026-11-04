#include "ClariusStreamer.hpp"
#include "FAST/Data/Image.hpp"
#include <cast/cast.h>
#include <functional>
#include <QGLContext>
#include <fstream>
#include <FAST/Algorithms/Color/ColorToGrayscale.hpp>
#ifdef WIN32
#else
#include <dlfcn.h>
#include <FAST/Visualization/Window.hpp>
#include <QOpenGLContext>
#endif

namespace fast {

#ifdef WIN32
//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
static std::string GetLastErrorAsString()
{
    //Get the error message, if any.
    DWORD errorMessageID = ::GetLastError();
    if(errorMessageID == 0)
        return std::string(); //No error message has been recorded

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);

    //Free the buffer.
    LocalFree(messageBuffer);

    return message;
}
#endif


ClariusStreamer::ClariusStreamer(std::string ipAddress, int port, bool grayscale, int width, int height) {
    createOutputPort<Image>(0);
    mNrOfFrames = 0;
    mFirstFrameIsInserted = false;
    mStreamIsStarted = false;
    mIsModified = true;
    mGrayscale = grayscale;

    createStringAttribute("ip", "IP address", "IP address of Clarius device to connect to", ipAddress);
    createIntegerAttribute("port", "Port number", "Port number of Clarius device to connect to", port);
    createBooleanAttribute("grayscale", "Grayscale", "Convert input image to grayscale", mGrayscale);

    setConnectionAddress(ipAddress);
    setConnectionPort(port);
    setOutputSize(width, height);
    setStreamingMode(StreamingMode::NewestFrameOnly);

    loadLibrary();
}

void ClariusStreamer::loadLibrary() {
#if defined(_M_ARM64) || defined(__aarch64__)
    const std::string arch = "arm64";
#else
    const std::string arch = "x86_64";
#endif

    // Load Clarius Cast library dynamically
    reportInfo() << "Loading clarius cast library" << reportEnd();
#ifdef WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS); // To avoid diaglog box, when not able to load a DLL
    if(fileExists(Config::getLibraryPath() + "cast.dll")) {
        m_handle = LoadLibrary("cast.dll");
    } else {
        std::string path = Config::getKernelBinaryPath() + "/../lib/cast/cast.dll";
        if(!fileExists(path)) {
            // Download
            std::cout << "Clarius Cast was not bundled with this distribution." << std::endl;
            const std::string destination = Config::getKernelBinaryPath() + "/../lib/cast/";
            createDirectories(destination);
            downloadAndExtractZipFile("https://github.com/FAST-Imaging/FAST-dependencies/releases/download/v4.0.0/cast_12.0.2_windows_" + arch + ".zip", destination, "cast");
        }
        m_handle = LoadLibrary(path.c_str());
    }
    if(!m_handle) {
        std::string msg = GetLastErrorAsString();
        throw Exception("Failed to use load Clarius Cast library. Error message: " + msg);
    }
#else

#if defined(__APPLE__)
    std::string ext = "dylib";
#else
    std::string ext = "so";
#endif
    if(fileExists(Config::getLibraryPath() + "libcast." + ext)) {
        m_handle = dlopen(("libcast." + ext).c_str(), RTLD_LAZY);
    } else {
        std::string path = Config::getKernelBinaryPath() + "/../lib/cast/libcast." + ext;
        if(!fileExists(path)) {
            // Download
            std::cout << "Clarius Cast was not bundled with this distribution." << std::endl;
            const std::string destination = Config::getKernelBinaryPath() + "/../lib/cast/";
            createDirectories(destination);

#if defined(__APPLE__)
            std::cout << "FAST will now download Clarius Cast ... "<< std::endl;
            downloadAndExtractZipFile("https://github.com/FAST-Imaging/FAST-dependencies/releases/download/v4.0.0/cast_12.0.2_macos_" + arch + ".zip", destination, "cast");
#else
            // Get ubuntu version
            std::string output;
            std::ifstream file("/etc/os-release", std::ios::in);
            if(!file.is_open())
                throw Exception("Unable to read /etc/os-release. Are you on ubuntu? Clarius Cast is only supported for Ubuntu");
            std::map<std::string, std::string> values;
            std::string line;
            while(std::getline(file, line)) {
                auto parts = split(line, "=");
                if(parts.size() == 2) {
                    values[parts[0]] = replace(parts[1], "\"", "");
                }
            }
            file.close();
            if(values.at("NAME") != "Ubuntu")
                throw Exception("ClariusStreamer only supports ubuntu linux");
            auto version = split(values.at("VERSION_ID"), ".");
            if(version.size() < 2)
                throw Exception("Error reading os-release output.");

            std::string ubuntuVersion = version[0] + "." + version[1];

            if((version[0] != "20" && version[0] != "22" && version[0] != "24") || version[1] != "04")
                throw Exception("Clarius Cast is only available for Ubuntu 20.04, 22.04 and 24.04. You have: " + ubuntuVersion);

            std::cout << "FAST will now download Clarius Cast for Ubuntu " << ubuntuVersion << " ..." << std::endl;
            downloadAndExtractZipFile("https://github.com/FAST-Imaging/FAST-dependencies/releases/download/v4.0.0/cast_12.0.2_ubuntu_" + ubuntuVersion + "_" + arch + ".zip", destination, "cast");
#endif
        }

        // Try to load it
        m_handle = dlopen(path.c_str(), RTLD_LAZY);
    }
    if(!m_handle) {
        std::string msg = dlerror();
        throw Exception("Failed to use load Clarius Cast library. Error message: " + msg);
    }
#endif
    reportInfo() << "Finished loading clarius cast library" << reportEnd();
}

void ClariusStreamer::loadAttributes() {
    mIPAddress = getStringAttribute("ip");
    mPort = getIntegerAttribute("port");
    mGrayscale = getBooleanAttribute("grayscale");
}

void* ClariusStreamer::getFunc(std::string name) {
#ifdef WIN32
    auto func = GetProcAddress(m_handle, name.c_str());
    if(!func) {
        std::string msg = GetLastErrorAsString();
        FreeLibrary(m_handle);
        throw Exception("Failed to get address of function in ClariusStreamer. Error message: " + msg);
    }
    return func;
#else
    auto func = dlsym(m_handle, name.c_str());
    if(!func) {
        std::string msg = dlerror();
        dlclose(m_handle);
        throw Exception("Failed to get address of function in ClariusStreamer. Error message: " + msg);
    }
    return func;
#endif
}

ClariusStreamer::pointer ClariusStreamer::self = nullptr; // class static

void ClariusStreamer::execute() {
    if(!mStreamIsStarted) {
        reportInfo() << "Trying to set up Clarius streaming..." << reportEnd();
        std::string keydir = Config::getKernelBinaryPath();
        // TODO A hack here to get this to work. Fix later
        // Lambdas converted to C style pointers can't have captures
        self = std::dynamic_pointer_cast<ClariusStreamer>(mPtr.lock());

        CusInitParams params;
        params.args.argc = 0;
        params.storeDir = keydir.c_str();
        // All callbacks have to be defined, or it will crash
        params.newProcessedImageFn = [](const void* img, const CusProcessedImageInfo* nfo, int npos, const CusPosInfo* pos) {
            self->getReporter().info() << "Processed image received. Current FPS: " << nfo->fps << self->getReporter().end();
            self->newProcessedImage(img, nfo, npos, pos);
        };
        params.buttonFn = [](CusButton btn, int clicks) {
            self->getReporter().info() << "Button pressed: " << (btn == CusButton::ButtonDown ? "DOWN" : "UP") << self->getReporter().end();
        };
        params.newRawImageFn = [](const void* img, const CusRawImageInfo* nfo, int npos, const CusPosInfo* pos) {
            self->getReporter().info() << "Raw image received" << self->getReporter().end();
        };
        params.newImuDataFn = [](const CusPosInfo* pos) {
            self->getReporter().info() << "IMU data received" << self->getReporter().end();
        };
        params.progressFn = [](int progress) {
            self->getReporter().info() << "Progress: " << progress  << self->getReporter().end();
        };
        params.newSpectralImageFn = [](const void* img, const CusSpectralImageInfo* nfo) {
            self->getReporter().info() << "Spectral image received" << self->getReporter().end();
        };
        params.freezeFn = [](int state) {
            self->getReporter().info() << "Freeze " << ((state == 1) ? "ON" : "OFF") << self->getReporter().end();
        };
        params.width = m_width;
        params.height = m_height;
        params.errorFn = [](const char* msg) {
            // Ignore OpenGL error which has the following message:
            std::string find = "Failed to make context current";
            std::string find2 = "Failed to make no context current";
            std::string msg2 = msg;
            if(msg2.size() > find2.size()) {
                if(msg2.substr(0, find.size()) == find || msg2.substr(0, find2.size()) == find2)
                    return;
            }

            self->signalCastStop(msg2);
        };

        auto init = (int (*)(const CusInitParams* params))getFunc("cusCastInit");
        int success = init(&params);
        if(success != 0)
            throw Exception("Unable to initialize clarius cast");
        m_castInitialized = true;
        reportInfo() << "Clarius streamer initialized" << reportEnd();

        mStreamIsStarted = true;
        m_thread = std::make_unique<std::thread>(std::bind(&ClariusStreamer::generateStream, this));
        // We need a separate thread to reset ClariusStreamer::self which will cause destructor to be called in this thread
        // Which in turn will call destroy on cast
        auto thread2 = new std::thread([this]() {
            // Wait until cast disconnect has finished before call reset on self
            reportInfo() << "Thread is waiting for cast disconnect" << reportEnd();
            {
                std::unique_lock<std::mutex> lock2(m_castDisconnectMutex);
                while(!m_castDisconnected) {
                    m_castDisconnectCV.wait(lock2);
                }
            }
            reportInfo() << "Resetting self..." << reportEnd();
            self.reset();
            reportInfo() << "Reset done ." << reportEnd();
        });

        auto connect = (int (*)(const char* ipAddress, unsigned int port, const char* cert, CusConnectFn fn))getFunc("cusCastConnect");
        success = connect(mIPAddress.c_str(), mPort, "research", [](int port, int imuPort, int swMatch) {
#ifndef WIN32
            // For some reason we need to create a GL context here
            auto glContext = new QOpenGLContext();
            glContext->setShareContext(Window::getSecondaryGLContext());
            glContext->create();
            if(!glContext->isValid())
                self->getReporter().error() << "Cast GL CONTEXT WAS VALID!" << self->getReporter().end();
            glContext->makeCurrent(Window::getQSurface());
#endif
            if (port > 0) {
                self->getReporter().info() << "Clarius connect on UDP port " << port << self->getReporter().end();
                if (swMatch == CUS_FAILURE) {
                    self->getReporter().warning() << "A software mismatch between Clarius Cast API in FAST and the scanner was detected." << self->getReporter().end();
                }
            }
            else {
                self->signalCastStop("Failed to connect to Clarius device");
            }
        });
        if(success != 0)
            throw Exception("Unable to connect to clarius scanner");
        reportInfo() << "Clarius streamer connecting ...." << reportEnd();
    }

    // Wait here for first frame
    waitForFirstFrame();
}

void ClariusStreamer::newProcessedImage(const void *img, const _CusProcessedImageInfo *nfo, int npos,
                                        const _CusPosInfo *pos) {
    if(nfo->bitsPerPixel != 32)
        throw Exception("Expected 32 bits per pixel (4 channels with 8 bits) each in ClariusStreamer, but got " + std::to_string(nfo->bitsPerPixel));

    // Copy pixels
    const int width = nfo->width;
    const int height = nfo->height;
    auto image = Image::create(width, height, TYPE_UINT8, 4, img);
    if(mGrayscale) {
        image = ColorToGrayscale::create()->connect(image)->runAndGetOutputData<Image>();
    }
    float spacing = (float)nfo->micronsPerPixel/1000.0f; // convert spacing to millimeters
    image->setSpacing(Vector3f(spacing, spacing, 1));
    image->setCreationTimestamp(nfo->tm / 1000000); // convert timestsamp to milliseconds

    addOutputData(0, image);
    frameAdded();
    mNrOfFrames++;
}

uint ClariusStreamer::getNrOfFrames() {
    return mNrOfFrames;
}

ClariusStreamer::~ClariusStreamer() {
    reportInfo() << "Destroying ClariusStreamer ..." << reportEnd();
    stop();

    // Calling destroy in the cast thread causes crash:
    // Calling destroy in generateStream causes block
    // How to guarantee that this is not run in cast thread?: run in a separate detached thread..?
    // This is now solved by spawning another thread called thread2, which calls reset on self
    // and then the destructor
    if(m_castInitialized) {
        auto destroy = (int (*)())getFunc("cusCastDestroy");
        getReporter().info() << "Destroying cast ..." << getReporter().end();
        int success = destroy();
        getReporter().info() << "Cast destroy done." << getReporter().end();
        if(success < 0)
            getReporter().error() << "Unable to destroy clarius cast" << getReporter().end();
    }

    reportInfo() << "ClariusStreamer destroyed" << reportEnd();
}

void ClariusStreamer::generateStream() {
    // Use this thread to listen to a stop signal
    {
        std::unique_lock<std::mutex> lock(m_castStopMutex);
        while(!m_castStop) {
            m_castStopCV.wait(lock);
        }
    }
    stop();
	stopWithError(m_stopMessage); // This will cause ThreadStopped exception
    reportInfo() << "ClariusStreamer::generateStream() done" << reportEnd();
}

void ClariusStreamer::signalCastStop(std::string message) {
    {
        std::lock_guard<std::mutex> lock(m_castStopMutex);
        m_castStop = true;
        m_stopMessage = message;
    }
    m_castStopCV.notify_all();
}

void ClariusStreamer::signalDisconnect() {
    {
        std::lock_guard<std::mutex> lock(m_castDisconnectMutex);
        m_castDisconnected = true;
    }
    m_castDisconnectCV.notify_all();
}

void ClariusStreamer::stop() {
    reportInfo() << "In clarius streamer stop()" << reportEnd();
    if(!mStreamIsStarted)
        return;
    reportInfo() << "Stopping clarius streamer.." << reportEnd();
    mStreamIsStarted = false;
    auto disconnect = (int (*)(CusReturnFn))getFunc("cusCastDisconnect");
    int success = disconnect([](int code) {
        self->getReporter().info() << "Clarius Disconnect callback with return code: " << ((code == CUS_FAILURE) ? "Failure" : "Success") << self->getReporter().end();
        self->signalDisconnect();
    });
    reportInfo() << "Clarius Disconnect done." << reportEnd();
    if(success == 0)
        reportError() << "Unable to disconnect from clarius scanner" << reportEnd();
	frameAdded(); // Unblock execute if needed
    reportInfo() << "Clarius streamer stopped" << Reporter::end();
    // TODO How to stop program gracefully if no frames are ever streamed?
}

void ClariusStreamer::stopPipeline() {
    signalCastStop();
}

void ClariusStreamer::setConnectionAddress(std::string ipAddress) {
    mIPAddress = ipAddress;
}

void ClariusStreamer::setConnectionPort(int port) {
    if(port <= 0)
        throw Exception("Illegal port nr " + std::to_string(port));
    mPort = port;
}

void ClariusStreamer::toggleFreeze() {
    auto userFunc = (int (*)(CusUserFunction cmd, double val, CusReturnFn fn))getFunc("cusCastUserFunction");
    if(userFunc(Freeze, 0.0, nullptr) < 0)
        reportError() << "Error toggling freeze" << reportEnd();
}

void ClariusStreamer::increaseDepth() {
    auto userFunc = (int (*)(CusUserFunction cmd, double val, CusReturnFn fn))getFunc("cusCastUserFunction");
	if(userFunc(DepthInc, 0.0, nullptr) < 0)
        reportError() << "Error increasing depth" << reportEnd();
}

void ClariusStreamer::decreaseDepth() {
    auto userFunc = (int (*)(CusUserFunction cmd, double val, CusReturnFn fn))getFunc("cusCastUserFunction");
	if(userFunc(DepthDec, 0.0, nullptr) < 0)
        reportError() << "Error decreasing depth" << reportEnd();
}

void ClariusStreamer::setDepth(float depth) {
    auto userFunc = (int (*)(CusUserFunction cmd, double val, CusReturnFn fn))getFunc("cusCastUserFunction");
	if(userFunc(SetDepth, depth, nullptr) < 0)
        reportError() << "Error setting depth to " << depth << reportEnd();
}

void ClariusStreamer::setGain(float gain) {
    auto userFunc = (int (*)(CusUserFunction cmd, double val, CusReturnFn fn))getFunc("cusCastUserFunction");
	if(userFunc(SetGain, gain, nullptr) < 0)
        reportError() << "Error setting gain to " << gain << reportEnd();
}

void ClariusStreamer::setOutputSize(int width, int height) {
    m_width = width;
    m_height = height;
}

}
