#pragma once

#include <FAST/Data/SpatialDataObject.hpp>
#include <FAST/Data/SimpleDataObject.hpp>
#include <FAST/Data/Access/BoundingBoxSetAccess.hpp>
#include <thread>
#include <FAST/ProcessObject.hpp>

namespace fast {

/**
 * @brief A 2D bounding box data object.
 *
 * @ingroup data bounding-box
 */
class FAST_EXPORT BoundingBox : public SpatialDataObject {
    FAST_DATA_OBJECT(BoundingBox)
    public:
        FAST_CONSTRUCTOR(BoundingBox,
                Vector2f, position,,
                Vector2f, size,,
                uchar, label, = 1,
                float, score, = 0.0f
        )
        void setLabel(uchar label);
        uchar getLabel();
        /**
         * @brief Set position in millimeters
         */
        void setPosition(Vector2f position);
        Vector2f getPosition();

        Vector2f getCentroid();

        /**
         * @brief Set size in millimeters
         */
        void setSize(Vector2f size) ;
        Vector2f getSize();

        void setScore(float score);
        float getScore();

        void free(ExecutionDevice::pointer device) override {};
        void freeAll() override {};
        float intersectionOverUnion(BoundingBox::pointer bbox2) const;
    protected:
        BoundingBox();

        bool m_initialized = false;
        uchar m_label = 1;
        Vector2f m_position;
        Vector2f m_size;
        float m_score;

        std::mutex m_mutex;
};

/**
 * @brief A data object representing a (large) set of bounding boxes.
 *
 * @ingroup data bounding-box
 */
class FAST_EXPORT BoundingBoxSet : public SpatialDataObject {
    FAST_DATA_OBJECT(BoundingBoxSet)
    public:
        FAST_CONSTRUCTOR(BoundingBoxSet)
        int getNrOfLines();
        int getNrOfVertices();
        float getMinimumSize() const;
        BoundingBoxSetAccess::pointer getAccess(accessType type);
        BoundingBoxSetOpenGLAccess::pointer getOpenGLAccess(accessType type);
        void freeAll() override;
        void free(ExecutionDevice::pointer device) override;
        ~BoundingBoxSet();
        virtual DataBoundingBox getBoundingBox() const override;
    protected:
        void setAllDataToOutOfDate();

		// OpenGL data
        bool mVBOHasData;
        bool mVBODataIsUpToDate;
        GLuint mCoordinateVBO = 0;
        GLuint mLineEBO = 0;
        GLuint m_labelVBO = 0;

        // Host data
        bool mHostHasData;
        bool mHostDataIsUpToDate;
        std::vector<float> mCoordinates;
        std::vector<uint> mLines;
        std::vector<uchar> m_labels;
        std::vector<float> m_scores;

        uint mNrOfVertices;
        uint mNrOfLines;

        bool mIsInitialized;

        float m_minimumSize;
};

/**
 * @brief Accumulate a stream of bounding box sets to a single large bounding box set.
 *
 * @todo move to algorithms folder
 * @ingroup bounding-box
 */
class FAST_EXPORT BoundingBoxSetAccumulator : public ProcessObject {
    FAST_PROCESS_OBJECT(BoundingBoxSetAccumulator)
	public:
        FAST_CONSTRUCTOR(BoundingBoxSetAccumulator)
	protected:
        void execute() override;

        BoundingBoxSet::pointer m_accumulatedBBset;
};

}
