#include "Config.hpp"
#include "Exception.hpp"
#include "Utility.hpp"
#include <fstream>
#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif
#ifdef FAST_MODULE_VISUALIZATION
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QUrl>
#include <FAST/Visualization/Window.hpp>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QDir>
#endif

// Includes needed to get path of dynamic library
#ifdef _WIN32
#include <windows.h>
#else
#define __USE_GNU 1
#include <dlfcn.h>
#endif

namespace fast {

		namespace {
			// Initialize global variables, put in anonymous namespace to hide them
			bool mConfigurationLoaded = false;
			std::string mConfigFilename = "";
			std::string mBasePath = "";
			std::string mTestDataPath;
			std::string mKernelSourcePath;
			std::string mKernelBinaryPath;
			std::string mDocumentationPath;
			std::string mPipelinePath;
			std::string mLibraryPath;
			std::string mQtPluginsPath;
			bool m_visualization = true;
			bool m_terminateHandlerDisabled = false;
#ifdef FAST_MODULE_VISUALIZATION
			bool m_GUI = true;
			bool m_OpenGL = true;
#else
            bool m_GUI = false;
            bool m_OpenGL = false;
#endif
		}

		static void copyPipelineFilesRecursivly(std::string pipelineSourcePath, std::string pipelineDestinationPath) {
			for(auto&& pipeline : getDirectoryList(pipelineSourcePath, true, true)) {
				if(isDir(pipelineSourcePath + pipeline)) {
				    createDirectories(join(pipelineDestinationPath, pipeline));
					copyPipelineFilesRecursivly(join(pipelineSourcePath, pipeline), join(pipelineDestinationPath, pipeline));
				} else {
					if(!fileExists(join(pipelineDestinationPath, pipeline))) {
						// Copy file from source folder to writable destination folder
						std::ifstream  src(join(pipelineSourcePath, pipeline), std::ios::binary);
						std::ofstream  dst(join(pipelineDestinationPath, pipeline), std::ios::binary);

						dst << src.rdbuf();
					}
				}
			}
		}

#ifndef WIN32
        std::string getHomePath() {
		    const char *homedir;
		    if((homedir = getenv("HOME")) == NULL) {
		        homedir = getpwuid(getuid())->pw_dir;
		    }
		    std::string homedircpp = std::string(homedir);
		    return homedircpp;
		}
#endif

		std::string Config::getPath() {
			if (mBasePath != "")
				return mBasePath;
			std::string path = "";
			std::string slash = "/";
			// Find path of the FAST dynamic library
			// The fast_configuration.txt file should lie in the folder below
#ifdef _WIN32
			// http://stackoverflow.com/questions/6924195/get-dll-path-at-runtime
			HMODULE hm = NULL;

			if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)&getPath,
				&hm)) {
				int ret = GetLastError();
				throw Exception("Error reading dyanmic library address in getPath()");
			}
			char dlpath[MAX_PATH];
			GetModuleFileNameA(hm, dlpath, sizeof(dlpath));
			path = std::string(dlpath);
			path = replace(path, "\\", "/"); // Replace windows \ with /
			path = replace(path, "/FAST.dll", "");
#else
			// http://stackoverflow.com/questions/1681060/library-path-when-dynamically-loaded
			Dl_info dl_info;
			int ret = dladdr((void *)&getPath, &dl_info);
			if (ret == 0)
				throw Exception("Error reading dynamic library address in getPath()");
			const char* dlpath = dl_info.dli_fname;
			path = std::string(dlpath);
			// Remove lib name and lib folder
			int libPos = path.rfind(slash + "lib" + slash);
			path = path.substr(0, libPos) + slash + "bin";
#endif


			path = path + slash; // Make sure there is a slash at the end
			return path;
		}
		
		void Config::loadConfiguration() {
			if (mConfigurationLoaded)
				return;

			// Set default paths
			// Check if data is located in the FAST folder and if so use that
			if(fileExists(getPath() + "../../data/LICENSE.md")) {
				mTestDataPath = getPath() + "../../data/";
			} else {
#ifdef WIN32
				mTestDataPath = "C:/ProgramData/FAST/data/";
#else
				mTestDataPath = getHomePath() + "/FAST/data/";
#endif
			}
#ifdef WIN32
			mKernelBinaryPath = "C:/ProgramData/FAST/kernel_binaries/";
			mPipelinePath = "C:/ProgramData/FAST/pipelines/";
			std::string writeablePath = "C:/ProgramData/FAST/";
			mLibraryPath = getPath();
#else
			mKernelBinaryPath = getHomePath() + "/FAST/kernel_binaries/";
			mPipelinePath = getHomePath() + "/FAST/pipelines/";
			std::string writeablePath = getHomePath() + "/FAST/";
			mLibraryPath = getPath() + "/../lib/";
#endif
			mKernelSourcePath = getPath() + "../../source/FAST/";
			mDocumentationPath = getPath() + "../../doc/";
			mQtPluginsPath = getPath() + "../plugins/";

			createDirectories(mTestDataPath);
			createDirectories(mPipelinePath);

			// Copy pipelines (first time only)
			try {
				if(isDir(getPath() + "../../pipelines/")) {
					copyPipelineFilesRecursivly(getPath() + "../../pipelines/", mPipelinePath);
				} else {
					copyPipelineFilesRecursivly(getPath() + "../pipelines/", mPipelinePath);
				}
			} catch(Exception & e) {
				Reporter::warning() << e.what() << Reporter::end();
			}

#ifdef FAST_MODULE_VISUALIZATION
			m_visualization = true;
#else
            Reporter::warning() << "Rendering is disabled in this build." << Reporter::end();
			m_visualization = false;
#endif

			// Read and parse configuration file
			// It should reside in the build folder when compiling, and in the root folder when using release
			std::string filename;
			if(mConfigFilename == "") {
				filename = getPath() + "fast_configuration.txt";
			}
			else {
				filename = mConfigFilename;
			}
			std::ifstream file(filename);
			if (!file.is_open()) {
				Reporter::info() << "Unable to open the configuration file " << filename << ". Using defaults instead." << Reporter::end();
				mConfigurationLoaded = true;
				return;
			}
			Reporter::info() << "Loaded configuration file: " << filename << Reporter::end();

			do {
				std::string line;
				std::getline(file, line);
				trim(line);
				if (line[0] == '#' || line.size() == 0) {
					// Comment or empty line, skip
					continue;
				}
				std::vector<std::string> list = split(line, "=");
				if (list.size() != 2) {
					throw Exception("Error parsing configuration file. Expected key = value pair, got: " + line);
				}

				std::string key = list[0];
				std::string value = list[1];
				trim(key);
				trim(value);				

				if (key == "TestDataPath") {
					value = replace(value, "@ROOT@", getPath() + "/../");
					mTestDataPath = value;
				}
				else if (key == "KernelSourcePath") {
					value = replace(value, "@ROOT@", getPath() + "/../");
					mKernelSourcePath = value;
				}
				else if (key == "KernelBinaryPath") {
					value = replace(value, "@ROOT@", writeablePath);
					mKernelBinaryPath = value;
				}
				else if (key == "DocumentationPath") {
					value = replace(value, "@ROOT@", getPath() + "/../");
					mDocumentationPath = value;
				}
				else if (key == "PipelinePath") {
					value = replace(value, "@ROOT@", writeablePath);
					mPipelinePath = value;
				}
				else if (key == "QtPluginsPath") {
					value = replace(value, "@ROOT@", getPath() + "/../");
					mQtPluginsPath = value;
				}
				else if (key == "LibraryPath") {
					value = replace(value, "@ROOT@", getPath() + "/../");
					mLibraryPath = value;
				}
				else {
					throw Exception("Error parsing configuration file. Unrecognized key: " + key);
				}
			} while (!file.eof());
			file.close();

			Reporter::info() << "Test data path: " << mTestDataPath << Reporter::end();
			Reporter::info() << "Kernel source path: " << mKernelSourcePath << Reporter::end();
			Reporter::info() << "Kernel binary path: " << mKernelBinaryPath << Reporter::end();
			Reporter::info() << "Documentation path: " << mDocumentationPath << Reporter::end();
			Reporter::info() << "Pipeline path: " << mPipelinePath << Reporter::end();
			Reporter::info() << "Qt plugins path: " << mQtPluginsPath << Reporter::end();
            Reporter::info() << "Library path: " << mLibraryPath << Reporter::end();

			mConfigurationLoaded = true;
		}

		std::string Config::getTestDataPath() {
			loadConfiguration();
			// Check if test data exists. If it doesn't issue a warning
			if(!fileExists(mTestDataPath + "/LICENSE.md"))
                            Reporter::warning() << "Test data was NOT found at " << mTestDataPath << ". Download the test data by running ./downloadTestData or fast.downloadTestDataIfNotExists() in Python." << Reporter::end();

			return mTestDataPath;
		}

		std::string Config::getKernelSourcePath() {
			loadConfiguration();
			return mKernelSourcePath;
		}

		std::string Config::getKernelBinaryPath() {
			loadConfiguration();
			return mKernelBinaryPath;
		}

		std::string Config::getDocumentationPath() {
			loadConfiguration();
			return mDocumentationPath;
		}

		std::string Config::getPipelinePath() {
			loadConfiguration();
			return mPipelinePath;
		}

		std::string Config::getLibraryPath() {
		    loadConfiguration();
		    return mLibraryPath;
		}

		std::string Config::getQtPluginsPath() {
			loadConfiguration();
			return mQtPluginsPath;
		}

		void Config::setConfigFilename(std::string filename) {
			mConfigFilename = filename;
			loadConfiguration();
		}

		void Config::setBasePath(std::string path) {
			mBasePath = path;
			if (mBasePath[mBasePath.size() - 1] != '/')
				mBasePath += "/";
			mTestDataPath = getPath() + "../data/";
			mKernelSourcePath = getPath() + "../source/FAST/";
			mKernelBinaryPath = getPath() + "kernel_binaries/";
			mDocumentationPath = getPath() + "../doc/";
			mPipelinePath = getPath() + "../pipelines/";
			mQtPluginsPath = getPath() + "../plugins/";
#ifdef WIN32
            mLibraryPath = getPath() + "../bin/";
#else
            mLibraryPath = getPath() + "../lib/";
#endif
			loadConfiguration();
		}

		void Config::setTestDataPath(std::string path) {
			mTestDataPath = path;
		}

		void Config::setKernelSourcePath(std::string path) {
			mKernelSourcePath = path;
		}

		void Config::setKernelBinaryPath(std::string path) {
			mKernelBinaryPath = path;
		}

		void Config::setDocumentationPath(std::string path) {
			mDocumentationPath = path;
		}

		void Config::setPipelinePath(std::string path) {
			mPipelinePath = path;
		}

		void Config::setVisualization(bool visualization) {
		    m_visualization = visualization;
		}

		bool Config::getVisualization() {
		    loadConfiguration();
            return m_visualization;
		}

        void Config::setTerminateHandlerDisabled(bool disabled) {
            m_terminateHandlerDisabled = disabled;
        }

        bool Config::getTerminateHandlerDisabled() {
		    return m_terminateHandlerDisabled;
		}

        bool Config::hasGUI() {
            return m_GUI;
        }

        void Config::disableGUI() {
		    m_GUI = false;
        }

        bool Config::hasOpenGLRendering() {
            return m_OpenGL;
        }

        void Config::disableOpenGLRendering() {
            m_OpenGL = false;
        }

    void downloadTestDataIfNotExists(std::string destination, bool force) {
#ifdef FAST_MODULE_VISUALIZATION
			if(destination.empty())
				destination = Config::getTestDataPath();
			if(!force && fileExists(destination + "/LICENSE.md"))
				return;
			std::cout << "Downloading test data (~2GB) to " << destination << std::endl;
			createDirectories(destination);
			Window::initializeQtApp();
			QNetworkAccessManager manager;
			QUrl url("https://fast-datahub.sintef.no/downloads/FAST_Test_Data.zip");
			QNetworkRequest request(url);
			request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
			auto reply = manager.get(request);
			Progress progressBar(1);
			progressBar.setText("Downloading");
			QObject::connect(reply, &QNetworkReply::downloadProgress, [&progressBar](quint64 current, quint64 max) {
                progressBar.setMax(max)	;
			    progressBar.update(current)	;
			});
			auto tempLocation = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/FAST_Test_Data.zip";
			QFile file(tempLocation);
			if(!file.open(QIODevice::WriteOnly)) {
				throw Exception("Could not write to " + tempLocation.toStdString());
			}
			QObject::connect(reply, &QNetworkReply::readyRead, [&reply, &file]() {
				file.write(reply->read(reply->bytesAvailable()));
			});
			QObject::connect(&manager, &QNetworkAccessManager::finished, [reply, &file, destination]() {
				std::cout << "Finished downloading file. Processing.." << std::endl;
				file.close();
				std::cout << "Unzipping the data file ..." << std::endl;
				try {
					extractZipFile(file.fileName().toStdString(), destination + "/../");
				} catch(Exception & e) {
					std::cout << "ERROR: Zip extraction failed." << std::endl;
				}
				
				file.remove();
				std::cout << "Done." << std::endl;
			});

			auto eventLoop = new QEventLoop(&manager);

			// Make sure to quit the event loop when download is finished
			QObject::connect(&manager, &QNetworkAccessManager::finished, eventLoop, &QEventLoop::quit);

			// Wait for it to finish
			eventLoop->exec();
#else
			throw Exception("downloadTestDataIfNotExists() only available if FAST is built with Qt");
#endif

		}

}; // end namespace fast
