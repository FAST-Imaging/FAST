#pragma once

#include <string>
#include <FASTExport.hpp>
#include <FAST/DataChannels/DataChannel.hpp>

namespace fast {

void FAST_EXPORT downloadTestDataIfNotExists(std::string overrideDestination = "", bool force = false);

class FAST_EXPORT Config {
public:
    static std::string getTestDataPath();
    static std::string getKernelSourcePath();
    static std::string getKernelBinaryPath();
    static std::string getDocumentationPath();
    static std::string getPipelinePath();
    static std::string getLibraryPath();
    static std::string getQtPluginsPath();
    static bool getVisualization();
    static bool hasGUI();
    static void disableGUI();
    static bool hasOpenGLRendering();
    static void disableOpenGLRendering();
    static void setTestDataPath(std::string path);
    static void setKernelSourcePath(std::string path);
    static void setKernelBinaryPath(std::string path);
    static void setDocumentationPath(std::string path);
    static void setPipelinePath(std::string path);
    static void setConfigFilename(std::string filename);
    static void setBasePath(std::string path);
    static void setVisualization(bool visualization);
    static void setTerminateHandlerDisabled(bool disabled);
    static bool getTerminateHandlerDisabled();
protected:
    static void loadConfiguration();
    static std::string getPath();
};

}
