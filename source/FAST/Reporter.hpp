#pragma once

#include <map>
#include <iostream>
#include <sstream>
#include <thread>
#include "FASTExport.hpp"
#ifdef WIN32
#include <windows.h>
#endif

#undef ERROR // undefine some windows garbage

namespace fast {

// Use to signal end of report line
class FAST_EXPORT  ReporterEnd {

};

class FAST_EXPORT  Reporter {
    public:
        static ReporterEnd end();
        static Reporter info();
        static Reporter warning();
        static Reporter error();
        enum Type {INFO, WARNING, ERROR};
        enum Method {NONE, COUT, LOG};
        void setType(Type);
        Reporter(Type type);
        Reporter();
        template <class T>
        void process(const T& content);
        void processEnd();
        void setReportMethod(Method method);
        void setReportMethod(Type type, Method method);
        static void setGlobalReportMethod(Method method);
        static void setGlobalReportMethod(Type type, Method method);
        static Method getGlobalReportMethod(Type type);
    private:
        Method getMethod(Type) const;
        Type mType;
        static std::map<Type, Method> mGlobalReporterMethods;
        // The local report methods override the global, if they are defined
        std::map<Type, Method> mLocalReporterMethods;

        // Variable to keep track of first <<
        bool mFirst;
        std::string m_textBuffer;
};

template <class T>
void Reporter::process(const T& content) {
    Method reportMethod = getMethod(mType);

    std::stringstream stream;
    if(mFirst) {
        // Write prefix first
        if(reportMethod == COUT) {
            if(mType == INFO) {
                stream << "INFO [" << std::this_thread::get_id() << "] ";
            } else if(mType == WARNING) {
                stream << "WARNING [" << std::this_thread::get_id() << "] ";
            } else if(mType == ERROR) {
                stream << "ERROR [" << std::this_thread::get_id() << "] ";
            }
        }
        mFirst = false;
    }

    stream << content;
    m_textBuffer += stream.str();
}

template <class T>
Reporter operator<<(Reporter report, const T& content) {
    report.process(content);
    return report;
}

template <>
FAST_EXPORT Reporter operator<<(Reporter report, const ReporterEnd& end);

} // end namespace fast
