#include "FAST/DeviceManager.hpp"
#include "FAST/Exception.hpp"
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include "AffineTransformationFileStreamer.hpp"
#include "FAST/AffineTransformation.hpp"
#include <fstream>
#include <chrono>

namespace fast {

AffineTransformationFileStreamer::AffineTransformationFileStreamer() {
    mStreamIsStarted = false;
    mIsModified = true;
    mLoop = false;
    thread = NULL;
    mFirstFrameIsInserted = false;
    mHasReachedEnd = false;
    mTimestampFilename = "";
    mSleepTime = 0;
    mNrOfFrames = 0;
    createOutputPort<AffineTransformation>(0, OUTPUT_DYNAMIC);
}

void AffineTransformationFileStreamer::setStreamingMode(StreamingMode mode) {
    Streamer::setStreamingMode(mode);
}

void AffineTransformationFileStreamer::setSleepTime(uint milliseconds) {
    mSleepTime = milliseconds;
}

void AffineTransformationFileStreamer::setTimestampFilename(std::string filepath) {
    mTimestampFilename = filepath;
}

void AffineTransformationFileStreamer::execute() {
    getOutputData<AffineTransformation>(0)->setStreamer(mPtr.lock());
    if(mFilename == "")
        throw Exception("No filename was given to the AffineTransformationFileStreamer");
    if(!mStreamIsStarted) {
        // Check that first frame exists before starting streamer

        mStreamIsStarted = true;
        thread = new boost::thread(boost::bind(&AffineTransformationFileStreamer::producerStream, this));
    }

    // Wait here for first frame
    boost::unique_lock<boost::mutex> lock(mFirstFrameMutex);
    while(!mFirstFrameIsInserted) {
        mFirstFrameCondition.wait(lock);
    }
}

void AffineTransformationFileStreamer::setFilename(std::string str) {
    mFilename = str;
}

void AffineTransformationFileStreamer::producerStream() {
    Streamer::pointer pointerToSelf = mPtr.lock(); // try to avoid this object from being destroyed until this function is finished

    // Read timestamp file if available
    std::ifstream timestampFile;
    unsigned long previousTimestamp = 0;
    auto previousTimestampTime = std::chrono::high_resolution_clock::time_point::min();
    if(mTimestampFilename != "") {
        timestampFile.open(mTimestampFilename.c_str());
        if(!timestampFile.is_open()) {
            throw Exception("Timestamp file not found in AffineTransformationFileStreamer");
        }
    }

    // Open file
    std::ifstream transformationFile(mFilename);
    if(!transformationFile.is_open()) {
    	throw Exception("Transformation file " + mFilename + " not found in AffineTransformationFileStreamer");
    }

    while(true) {
		// Read the next transformation from the file into a matrix
		Matrix4f matrix = Matrix4f::Identity();
		std::string line;
		std::vector<std::string> elements;

		bool restart = false;
		for(int row = 0; row < 3; ++row) {
			std::getline(transformationFile, line);
			boost::split(elements, line, boost::is_any_of(" "));

			boost::trim(elements[0]);
			if(elements.size() != 4 && elements.size() > 1) {
				throw Exception("Error reading transformation file " + mFilename + " expected 4 numbers per line, "
						+ boost::lexical_cast<std::string>(elements.size()) + " found.");
			} else if(elements.size() == 1 && elements[0] == "") {
				reportInfo() << "Reached end of stream" << Reporter::end;
				// If there where no files found at all, we need to release the execute method
				if(!mFirstFrameIsInserted) {
					{
						boost::lock_guard<boost::mutex> lock(mFirstFrameMutex);
						mFirstFrameIsInserted = true;
					}
					mFirstFrameCondition.notify_one();
				}
				if(mLoop) {
					// Restart stream
					if(timestampFile.is_open()) {
						previousTimestamp = 0;
						previousTimestampTime = std::chrono::high_resolution_clock::time_point::min();
						timestampFile.clear(); // Must clear errors before reset
						timestampFile.seekg(0); // Reset file to start
					}
					transformationFile.clear(); // Must clear errors before reset
					transformationFile.seekg(0); // Reset file to start
					restart = true;
				}
				mHasReachedEnd = true;
				// Reached end of stream
				break;
			}

			matrix.row(row) = Vector4f(
					boost::lexical_cast<float>(elements[0]),
					boost::lexical_cast<float>(elements[1]),
					boost::lexical_cast<float>(elements[2]),
					boost::lexical_cast<float>(elements[3]));
		}

		if(restart)
			continue;

		AffineTransformation::pointer transformation = AffineTransformation::New();
		transformation->matrix() = matrix;

		// Set and use timestamp if available
		if(mTimestampFilename != "") {
			std::string line;
			std::getline(timestampFile, line);
			if(line != "") {
				unsigned long timestamp = boost::lexical_cast<unsigned long>(line);
				transformation->setCreationTimestamp(timestamp);
				// Wait as long as necessary before adding image
				auto timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::high_resolution_clock::now() - previousTimestampTime);
				//reportInfo() << timestamp << reportEnd();
				//reportInfo() << previousTimestamp << reportEnd();
				//reportInfo() << "Time passed: " << timePassed.count() << reportEnd();
				while(timestamp > previousTimestamp + timePassed.count()) {
					// Wait
					boost::this_thread::sleep(boost::posix_time::milliseconds(timestamp-(long)previousTimestamp-timePassed.count()));
					timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::high_resolution_clock::now() - previousTimestampTime);
					//reportInfo() << "wait" << reportEnd();
					//reportInfo() << timestamp << reportEnd();
					//reportInfo() << previousTimestamp << reportEnd();
					//reportInfo() << "Time passed: " << timePassed.count() << reportEnd();
				}
				previousTimestamp = timestamp;
				previousTimestampTime = std::chrono::high_resolution_clock::now();
			}
		}
		DynamicData::pointer ptr = getOutputData<AffineTransformation>();
		if(ptr.isValid()) {
			try {
				ptr->addFrame(transformation);
				if(mSleepTime > 0)
					boost::this_thread::sleep(boost::posix_time::milliseconds(mSleepTime));
			} catch(NoMoreFramesException &e) {
				throw e;
			} catch(Exception &e) {
				reportInfo() << "streamer has been deleted, stop" << Reporter::end;
				break;
			}
			if(!mFirstFrameIsInserted) {
				{
					boost::lock_guard<boost::mutex> lock(mFirstFrameMutex);
					mFirstFrameIsInserted = true;
				}
				mFirstFrameCondition.notify_one();
			}
		} else {
			reportInfo() << "DynamicData object destroyed, stream can stop." << Reporter::end;
			break;
		}
		mNrOfFrames++;
    }
}


uint AffineTransformationFileStreamer::getNrOfFrames() const {
    return mNrOfFrames;
}

AffineTransformationFileStreamer::~AffineTransformationFileStreamer() {
    if(mStreamIsStarted) {
        if(thread->get_id() != boost::this_thread::get_id()) { // avoid deadlock
            thread->join();
        }
        delete thread;
    }
}

bool AffineTransformationFileStreamer::hasReachedEnd() const {
    return mHasReachedEnd;
}

void AffineTransformationFileStreamer::enableLooping() {
    mLoop = true;
}

void AffineTransformationFileStreamer::disableLooping() {
    mLoop = false;
}

} // end namespace fast
