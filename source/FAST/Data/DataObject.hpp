#ifndef DATAOBJECT_HPP_
#define DATAOBJECT_HPP_

#include "FAST/SmartPointers.hpp"
#include "FAST/Object.hpp"
#include "FAST/ExecutionDevice.hpp"
#include <boost/unordered_map.hpp>
#include "FAST/Streamers/Streamer.hpp"
#include <boost/thread/condition_variable.hpp>

namespace fast {

class DataObject : public Object {
    public:
        DataObject();
        typedef SharedPointer<DataObject> pointer;
        unsigned long getTimestamp() const;
        void updateModifiedTimestamp();
        void retain(ExecutionDevice::pointer device);
        void release(ExecutionDevice::pointer device);
        virtual ~DataObject() { };
        bool isDynamicData() const;
        void setStreamer(Streamer::pointer streamer);
        Streamer::pointer getStreamer();
        virtual std::string getNameOfClass() const = 0;
        static std::string getStaticNameOfClass() {
            return "DataObject";
        };
        unsigned long getCreationTimestamp() const;
        void setCreationTimestamp(unsigned long timestamp);
    protected:
        virtual void free(ExecutionDevice::pointer device) = 0;
        virtual void freeAll() = 0;
        bool mIsDynamicData;

        void accessFinished();
        void blockIfBeingWrittenTo();
        void blockIfBeingAccessed();

        boost::mutex mDataIsBeingWrittenToMutex;
        boost::condition_variable mDataIsBeingWrittenToCondition;
        bool mDataIsBeingWrittenTo;

        boost::mutex mDataIsBeingAccessedMutex;
        boost::condition_variable mDataIsBeingAccessedCondition;
        bool mDataIsBeingAccessed;
    private:
        boost::unordered_map<WeakPointer<ExecutionDevice>, unsigned int> mReferenceCount;

        // This is only used for dynamic data, it is defined here for to make the convienice function getStaticOutput/InputData to work
        WeakPointer<Streamer> mStreamer;

        // Timestamp is set to 0 when data object is constructed
        unsigned long mTimestampModified;

        // Timestamp is set to 0 when data object is constructed
        unsigned long mTimestampCreated;

};

}




#endif /* DATAOBJECT_HPP_ */
