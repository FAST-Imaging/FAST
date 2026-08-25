#pragma once
#include "FAST/ExecutionDevice.hpp"
#include "FAST/Data/DataTypes.hpp"
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

// This file contains a set of utility functions

// Undefine windows crap
#undef min
#undef max

namespace fast {

FAST_EXPORT double log2(double n);
FAST_EXPORT double round(double n);
FAST_EXPORT double round(double n, int decimals);

/**
 * Does simply x^2 = x*x
 * @tparam T
 * @param x a numeric value
 * @return x*x
 */
template <class T>
T square(T x) {
    return x*x;
}

template <typename ...Args>
std::string format(std::string format, Args && ... args) {
    auto size = std::snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args)...);
    std::string output(size + 1, '\0');
    std::sprintf(&output[0], format.c_str(), std::forward<Args>(args)...);
    return output;
}

template<class T>
T min(T a, T b) {
    return a < b ? a : b;
}

template<class T>
T max(T a, T b) {
    return a > b ? a : b;
}

template<class T>
T sign(T value) {
    if(value > 0) {
        return 1;
    } else if (value < 0) {
        return -1;
    } else {
        return 0;
    }
}

FAST_EXPORT unsigned int getPowerOfTwoSize(unsigned int size);
FAST_EXPORT void* allocateDataArray(unsigned int voxels, DataType type, unsigned int nrOfComponents);
template <class T>
float getSumFromOpenCLImageResult(void* voidData, unsigned int size, unsigned int nrOfComponents) {
    T* data = (T*)voidData;
    float sum = 0.0f;
    for(unsigned int i = 0; i < size*nrOfComponents; i += nrOfComponents) {
        sum += data[i];
    }
    return sum;
}

FAST_EXPORT void getMaxAndMinFromOpenCLImage(OpenCLDevice::pointer device, cl::Image2D image, DataType type, float* min, float* max);
FAST_EXPORT void getMaxAndMinFromOpenCLImage(OpenCLDevice::pointer device, cl::Image3D image, DataType type, float* min, float* max);
FAST_EXPORT void getMaxAndMinFromOpenCLBuffer(OpenCLDevice::pointer device, cl::Buffer buffer, unsigned int size, DataType type, float* min, float* max);
FAST_EXPORT void getIntensitySumFromOpenCLImage(OpenCLDevice::pointer device, cl::Image2D image, DataType type, float* sum);
FAST_EXPORT void getIntensitySumFromOpenCLImage(OpenCLDevice::pointer device, cl::Image3D image, DataType type, float* sum);
FAST_EXPORT void getIntensityStdDevFromOpenCLImage(OpenCLDevice::pointer device, cl::Image2D image, DataType type, float average, float* sum);
FAST_EXPORT void getIntensityStdDevFromOpenCLImage(OpenCLDevice::pointer device, cl::Image3D image, DataType type, float average, float* sum);

template <class T>
void getMaxAndMinFromData(void* voidData, unsigned int nrOfElements, float* min, float* max) {
    T* data = (T*)voidData;

    *min = std::numeric_limits<float>::max();
    *max = std::numeric_limits<float>::min();
    for(unsigned int i = 0; i < nrOfElements; i++) {
        if((float)data[i] < *min) {
            *min = (float)data[i];
        }
        if((float)data[i] > *max) {
            *max = (float)data[i];
        }
    }
}

template <class T>
float getSumFromData(void* voidData, unsigned int nrOfElements) {
    T* data = (T*)voidData;

    float sum = 0.0f;
    for(unsigned int i = 0; i < nrOfElements; i++) {
        sum += (float)data[i];
    }
    return sum;
}

template <class T>
float getStandardDeviationFromData(void* voidData, unsigned int nrOfElements) {
    T* data = (T*)voidData;

    float sum = 0.0f;
    for(unsigned int i = 0; i < nrOfElements; i++) {
        sum += (float)data[i];
    }
    float average = sum / nrOfElements;
    sum = 0.0f;
    for(unsigned int i = 0; i < nrOfElements; i++) {
        sum += ((float)data[i] - average)*((float)data[i] - average);
    }

    return std::sqrt(sum / nrOfElements);
}



FAST_EXPORT cl::size_t<3> createRegion(unsigned int x, unsigned int y, unsigned int z);
FAST_EXPORT cl::size_t<3> createRegion(Vector3ui size);
FAST_EXPORT cl::size_t<3> createOrigoRegion();

FAST_EXPORT std::string getCLErrorString(cl_int err);

/**
 * Function for splitting a string
 * @param input string
 * @param delimiter string
 * @return vector of strings
 */
FAST_EXPORT std::vector<std::string> split(const std::string input, const std::string& delimiter = " ", float removeEmpty = true);

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [](unsigned char c) {return !std::isspace(c); }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
        [](unsigned char c) {return !std::isspace(c); }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

/*
 * Replace all occurences of from to to in str
 */
FAST_EXPORT std::string replace(std::string str, std::string find, std::string replacement);

template <class T>
static inline void hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

FAST_EXPORT Matrix4f loadPerspectiveMatrix(float fovy, float aspect, float zNear, float zFar);

FAST_EXPORT Matrix4f loadOrthographicMatrix(float left, float right, float bottom, float top, float zNear, float zFar);

/**
 * Creates a directory at the given path.
 * Throws exception if it fails
 */
FAST_EXPORT void createDirectory(std::string path);

/**
 * Creates all directories in the given path.
 * Throws exception if it fails
 */
FAST_EXPORT void createDirectories(std::string path);

/**
 * Check if file exists
 * @param filename
 * @return
 */
FAST_EXPORT bool fileExists(std::string filename);

/**
 * @brief Returns size in bytes of file
 * @param filename
 * @return size in bytes
 */
FAST_EXPORT uint64_t fileSize(std::string filename);

/**
 * Returns a list of all files in a directory
 * @param path
 * @param getFiles Set to true to find files in directory
 * @param getDirectories Set to true to find subdirectories
 * @return list of files or subdirectories
 */
FAST_EXPORT std::vector<std::string> getDirectoryList(std::string path, bool getFiles = true, bool getDirectories = false);

/**
 * Returns the dir name of the given path. Example: getDirName("/home/user/something/file.txt")
 * returns home/user/something/.
 *
 * @param path
 * @return
 */
FAST_EXPORT std::string getDirName(std::string path);

/**
 * @brief Returns the name of a file of a path.
 * Example: getFileName("/home/user/something.txt") returns something.txt
 * @param path
 * @return
 */
FAST_EXPORT std::string getFileName(std::string path);

/**
 * @brief Returns absolute path of relative path to file
 * e.g. file.txt located at /home/asd/asd/ would return /home/asd/asd/
 * @param path
 * @return
 */
FAST_EXPORT std::string getAbsolutePath(std::string path);

/**
 * Returns a string of the current date
 * @param format see http://en.cppreference.com/w/cpp/chrono/c/strftime
 * @return
 */
FAST_EXPORT std::string currentDateTime(std::string format = "%Y-%m-%d-%H%M%S");

/**
 * Removes trailing /
 * @param path
 * @return
 */
FAST_EXPORT std::string join(std::string path);

/**
 * Get modified date of a file as a string
 * @param filename
 * @return
 */
FAST_EXPORT std::string getModifiedDate(std::string filename);

/**
 * Join multiple paths.
 *
 * @tparam T
 * @param path1
 * @param args
 * @return
 */
template<typename ...T>
std::string join(const std::string& path1, T... args) {
    return path1 + "/" + join(args...);
}

/**
 * Check if path is a file.
 * @param path
 * @return
 */
FAST_EXPORT bool isFile(const std::string& path);

/**
 * Check if path is a directory.
 * @param path
 * @return
 */
FAST_EXPORT bool isDir(const std::string& path);

/**
 * Same as make_unique(std::size_t size), except this version will not
 * value initialize the dynamic array. This is useful for large arrays.
 * @tparam T
 * @param size
 * @return
 */
template <class T>
std::unique_ptr<T> make_uninitialized_unique(std::size_t size) {
    return std::unique_ptr<T>(new typename std::remove_extent<T>::type[size]);
}

/**
 * Extract the contents of a zip file to a given destination.
 * @param zipFilepath path to zip file to extract
 * @param destination path to where to extract the conents of the zip file
 */
FAST_EXPORT void extractZipFile(std::string zipFilepath, std::string destination);

/**
 * @brief Convert string to only upper case
 * @return upper case string
 */
FAST_EXPORT std::string stringToLower(std::string);

/**
 * @brief Convert string to only lower case
 * @return lower case string
 */
FAST_EXPORT std::string stringToUpper(std::string);

/**
 * @brief Generate random alphanumeric string of a given length
 * @param length
 * @return random string
 */
FAST_EXPORT std::string generateRandomString(int length);

/**
 * @brief Round double to a certain number of decimals, then convert to string
 * @param value
 * @return string
 */
FAST_EXPORT std::string roundToString(double value, int decimals = 0);
/**
 * @brief Round float to a certain number of decimals, then convert to string
 * @param value
 * @return string
 */
FAST_EXPORT std::string roundToString(float value, int decimals = 0);

/**
 * @brief Get character width of terminal/console.
 * If not found, default to 80
 * @return width
 */
FAST_EXPORT int getConsoleWidth();

/**
 * @brief A utility class for keeping track of progress and printing progress to the console.
 */
class FAST_EXPORT Progress {
    public:
        explicit Progress(uint64_t max, bool print = true, bool autoStart = false);
        void start();
        void update(uint64_t current);
        float getPercent() const;
        void setText(std::string text);
        uint64_t getCurrent() const;
        void setMax(uint64_t max);
        void setUnit(std::string unit, float scale = 1.0f, int precision = 1);
    private:
        std::chrono::high_resolution_clock::time_point m_startTime;
        bool m_print = true;
        bool m_started = false;
        uint64_t m_max;
        uint64_t m_current;
        std::string m_text = "";
        std::string m_unit = "";
        float m_unitScale = 1.0f;
        int m_unitPrecision = -1;
};

void downloadAndExtractZipFile(const std::string& URL, const std::string& destination, const std::string& name);

enum class ConsoleColor {DEFAULT = 0, RED, GREEN, BLUE, YELLOW, MAGENTA, CYAN};

void print(const std::string& text, ConsoleColor color = ConsoleColor::DEFAULT, bool bold = false, bool addNewline = true);

} // end namespace fast

