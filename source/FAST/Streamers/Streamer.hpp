#pragma once

#include <FAST/ProcessObject.hpp>
#include "FAST/Data/DataTypes.hpp"
#include "FAST/Exception.hpp"
#include <thread>

namespace fast {


class FAST_EXPORT  NoMoreFramesException : public Exception {
    public:
        explicit NoMoreFramesException(std::string message) : Exception(message) {};
};

/**
 * A streamer is a PO that runs a separate thread of execution which produces data
 */
class FAST_EXPORT Streamer : public ProcessObject {
    public:
        typedef std::shared_ptr<Streamer> pointer;
        Streamer();
        virtual ~Streamer() {};
        static std::string getStaticNameOfClass() {
            return "Streamer";
        }
        virtual std::string getNameOfClass() const = 0;

        /**
         * Stop the stream
         */
        virtual void stop();

        virtual void setMaximumNrOfFrames(int maximumNrOfFrames);
    protected:
        /**
         * Block until the first data frame has been sent using a condition variable
         */
        virtual void waitForFirstFrame();

        /**
         * Starts the stream if it is not already started
         */
        virtual void startStream();

        /**
         * Signal that a frame was added. Necessary for unlocking the block in waitForFirstFrame
         */
        virtual void frameAdded();

        /**
         * The function producing the data stream
         */
        virtual void generateStream() = 0;

        bool m_firstFrameIsInserted = false;
        bool m_streamIsStarted = false;
        bool m_stop = false;

        std::mutex m_firstFrameMutex;
        std::mutex m_stopMutex;
        std::unique_ptr<std::thread> m_thread;
        std::condition_variable m_firstFrameCondition;


};

} // end namespace fast
