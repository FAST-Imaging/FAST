#include "BoundingBox.hpp"
#ifdef FAST_MODULE_VISUALIZATION
#include <FAST/Visualization/Window.hpp>
#include <QApplication>
#include <QGLFunctions>
#include <QOpenGLFunctions_3_3_Core>
#endif

namespace fast {

BoundingBox::BoundingBox(Vector2f position, Vector2f size, uchar label, float score) {
	if(size.x() <= 0 || size.y() <= 0)
		throw Exception("Size must be > 0, got: " + std::to_string(size.x()) + " " + std::to_string(size.y()));
	std::lock_guard<std::mutex> lock(m_mutex);
    m_position = position;
	m_size = size;
    m_label = label;
    m_score = score;
	m_initialized = true;
}

void BoundingBox::setLabel(uchar label) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_label = label;
}

uchar BoundingBox::getLabel() {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_label;
}

void BoundingBox::setPosition(Vector2f position) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_position = position;
}

Vector2f BoundingBox::getPosition() {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_position;
}

void BoundingBox::setSize(Vector2f size) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if(size.x() <= 0 || size.y() <= 0)
		throw Exception("Size must be > 0, got: " + std::to_string(size.x()) + " " + std::to_string(size.y()));
	m_size = size;
}

Vector2f BoundingBox::getSize() {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_size;
}

void BoundingBox::setScore(float score) {
	std::lock_guard<std::mutex> lock(m_mutex);
    m_score = score;
}

float BoundingBox::getScore() {
	std::lock_guard<std::mutex> lock(m_mutex);
    return m_score;
}

float BoundingBox::intersectionOverUnion(BoundingBox::pointer bb2) const {
    auto bb1 = std::dynamic_pointer_cast<BoundingBox>(mPtr.lock());
	Vector2f bb1_1 = bb1->getPosition();
	Vector2f bb1_2 = bb1->getPosition() + bb1->getSize();
	Vector2f bb2_1 = bb2->getPosition();
	Vector2f bb2_2 = bb2->getPosition() + bb2->getSize();
	float x_left = std::max(bb1_1.x(), bb2_1.x());
	float y_top = std::max(bb1_1.y(), bb2_1.y());
	float x_right = std::min(bb1_2.x(), bb2_2.x());
	float y_bottom = std::min(bb1_2.y(), bb2_2.y());

    if(x_right < x_left || y_bottom < y_top) // There is no overlap
        return 0.0f;

	float intersection_area = (x_right - x_left) * (y_bottom - y_top);

    float bb1_area = (bb1->getSize().x()) * (bb1->getSize().y());
    float bb2_area = (bb2->getSize().x()) * (bb2->getSize().y());

	float iou = intersection_area / (bb1_area + bb2_area - intersection_area);
    return iou;
}

Vector2f BoundingBox::getCentroid() {
    Vector2f centroid = (getPosition() + (getPosition() + getSize()))*0.5f;
    return centroid;
}

BoundingBoxSet::BoundingBoxSet() {
    mVBOHasData = false;
    mVBODataIsUpToDate = false;
    mIsInitialized = true;
    mHostHasData = true;
    mHostDataIsUpToDate = true;
    m_minimumSize = std::numeric_limits<float>::max();
}

int BoundingBoxSet::getNrOfLines() {
    if(mHostHasData && mHostDataIsUpToDate)
        mNrOfLines = mLines.size() / 2;
	return mNrOfLines;
}

int BoundingBoxSet::getNrOfVertices() {
    if(mHostHasData && mHostDataIsUpToDate)
        mNrOfVertices = mCoordinates.size() / 3;

    return mNrOfVertices;
}

void BoundingBoxSet::setAllDataToOutOfDate() {
    mHostDataIsUpToDate = false;
    mVBODataIsUpToDate = false;
}



BoundingBoxSetAccess::pointer BoundingBoxSet::getAccess(accessType type) {
    if(!mIsInitialized) {
        throw Exception("BoundingBoxSet has not been initialized.");
    }

    blockIfBeingWrittenTo();

    if(type == ACCESS_READ_WRITE) {
    	blockIfBeingAccessed();
		std::lock_guard<std::mutex> lock(mDataIsBeingWrittenToMutex);
		mDataIsBeingWrittenTo = true;
    }
    // Update data:
    if(!mHostHasData) {
#ifdef FAST_MODULE_VISUALIZATION
        // Host has not allocated data
        // Get all mesh data from VBOs
        if(!mVBOHasData)
            throw Exception("No data available");

        // Make sure GL context is available
        if(!QApplication::instance()) {
            Window::initializeQtApp();
        }
        if(QGLContext::currentContext() == nullptr)
            Window::getMainGLContext()->makeCurrent();

        QOpenGLFunctions_3_3_Core *fun = new QOpenGLFunctions_3_3_Core;
        fun->initializeOpenGLFunctions();

        mCoordinates.resize(mNrOfVertices*3);
        fun->glBindBuffer(GL_ARRAY_BUFFER, mCoordinateVBO);
        fun->glGetBufferSubData(GL_ARRAY_BUFFER, 0, mNrOfVertices*3*sizeof(float), mCoordinates.data());

        mLines.resize(mNrOfLines*2);
	    // Line EBO
		fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mLineEBO);
		fun->glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mNrOfLines*sizeof(uint), mLines.data());
		fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        m_labels.resize(mNrOfVertices);
		fun->glBindBuffer(GL_ARRAY_BUFFER, m_labelVBO);
		fun->glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mNrOfLines/4*sizeof(uchar), m_labels.data());
		fun->glBindBuffer(GL_ARRAY_BUFFER, 0);


        mHostHasData = true;
        mHostDataIsUpToDate = true;
#else
        throw Exception("Creating bounding box set with VBO is disabled as FAST module visualization is disabled.");
#endif
    } else {
        if(!mHostDataIsUpToDate) {
            throw Exception("Not implemented yet!");
        }
    }
	if(type == ACCESS_READ_WRITE) {
        setAllDataToOutOfDate();
        updateModifiedTimestamp();
    }
    mHostDataIsUpToDate = true;
    {
        std::lock_guard<std::mutex> lock(mDataIsBeingAccessedMutex);
        mDataIsBeingAccessed = true;
    }

    BoundingBoxSetAccess::pointer accessObject(new BoundingBoxSetAccess(&mCoordinates, &mLines, &m_labels, &m_scores, &m_minimumSize, std::static_pointer_cast<BoundingBoxSet>(mPtr.lock())));
	return std::move(accessObject);
}

BoundingBoxSetOpenGLAccess::pointer BoundingBoxSet::getOpenGLAccess(
        accessType type) {
    if(!mIsInitialized)
        throw Exception("BoundingBoxSet has not been initialized.");

    blockIfBeingWrittenTo();

    if (type == ACCESS_READ_WRITE) {
    	blockIfBeingAccessed();
		std::unique_lock<std::mutex> lock(mDataIsBeingWrittenToMutex);
		mDataIsBeingWrittenTo = true;
    }
    // Update data
    if(!mVBOHasData) {
        // VBO has not allocated data: Create VBO
#ifdef FAST_MODULE_VISUALIZATION
        // Make sure GL context is available
        if(!QApplication::instance()) {
            Window::initializeQtApp();
        }
        if(QGLContext::currentContext() == nullptr)
            Window::getMainGLContext()->makeCurrent();
        QGLFunctions *fun = Window::getMainGLContext()->functions();
        fun->glDeleteBuffers(1, &mCoordinateVBO);
        fun->glGenBuffers(1, &mCoordinateVBO);
		fun->glDeleteBuffers(1, &mLineEBO);
		fun->glGenBuffers(1, &mLineEBO);
		fun->glDeleteBuffers(1, &m_labelVBO);
		fun->glGenBuffers(1, &m_labelVBO);
        if(mHostHasData) {
            // If host has data, transfer it.
            // Coordinates
            fun->glBindBuffer(GL_ARRAY_BUFFER, mCoordinateVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, mCoordinates.size()*sizeof(float), mCoordinates.data(), GL_STATIC_DRAW);
            // Line EBO
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mLineEBO);
            fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mLines.size()*sizeof(uint), mLines.data(), GL_STATIC_DRAW);
            // Labels
            fun->glBindBuffer(GL_ARRAY_BUFFER, m_labelVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, m_labels.size()*sizeof(uchar), m_labels.data(), GL_STATIC_DRAW);
        } else {
            // Only allocate space
            // Coordinates
            fun->glBindBuffer(GL_ARRAY_BUFFER, mCoordinateVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, mNrOfVertices*3*sizeof(float), NULL, GL_STATIC_DRAW);
			// Line EBO
			fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mLineEBO);
			fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mNrOfLines*sizeof(uint), NULL, GL_STATIC_DRAW);
            // Label VBO
            fun->glBindBuffer(GL_ARRAY_BUFFER, m_labelVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, mNrOfVertices*sizeof(uchar), NULL, GL_STATIC_DRAW);
        }
        fun->glBindBuffer(GL_ARRAY_BUFFER, 0);
        fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glFinish();
        if(glGetError() == GL_OUT_OF_MEMORY) {
        	throw Exception("OpenGL out of memory while creating mesh data for VBO");
        }
        //reportInfo() << "Created VBO with ID " << mVBOID << " and " << mNrOfTriangles << " of triangles" << Reporter::end();

        mVBOHasData = true;
#else
        throw Exception("Creating bounding box set with VBO is disabled as FAST module visualization is disabled.");
#endif
    } else {
        if(!mVBODataIsUpToDate) {
#ifdef FAST_MODULE_VISUALIZATION
			QGLFunctions *fun = Window::getMainGLContext()->functions();
            // Update VBO/EBO data from host
            // Coordinates
            fun->glBindBuffer(GL_ARRAY_BUFFER, mCoordinateVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, mCoordinates.size()*sizeof(float), mCoordinates.data(), GL_STATIC_DRAW);
            // Lines
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mLineEBO);
            fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mLines.size()*sizeof(uint), mLines.data(), GL_STATIC_DRAW);
            // Labels
            fun->glBindBuffer(GL_ARRAY_BUFFER, m_labelVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, m_labels.size()*sizeof(uchar), m_labels.data(), GL_STATIC_DRAW);
#else
            throw Exception("Creating bounding box set with VBO is disabled as FAST module visualization is disabled.");
#endif
        }
    }
	if(type == ACCESS_READ_WRITE) {
        setAllDataToOutOfDate();
        updateModifiedTimestamp();
    }
	mVBODataIsUpToDate = true;

    {
        std::unique_lock<std::mutex> lock(mDataIsBeingAccessedMutex);
        mDataIsBeingAccessed = true;
    }

	BoundingBoxSetOpenGLAccess::pointer accessObject(
            new BoundingBoxSetOpenGLAccess(
                    mCoordinateVBO,
                    mLineEBO,
					m_labelVBO,
                    std::static_pointer_cast<BoundingBoxSet>(mPtr.lock())
            )
    );
	return std::move(accessObject);
}

void BoundingBoxSet::freeAll() {
    // TODO finish
    if(mVBOHasData) {
#ifdef FAST_MODULE_VISUALIZATION
        glFinish(); // Make sure any draw calls are done before deleting this mesh
        // Need mutual exclusive write lock to delete data
        //VertexBufferObjectAccess::pointer access = getVertexBufferObjectAccess(ACCESS_READ_WRITE);
        //Window::getMainGLContext()->makeCurrent(); // Need an active context to delete the mesh VBO
        QGLFunctions *fun = Window::getMainGLContext()->functions();
        // glDeleteBuffer is not used due to multi-threading issues..
        //fun->glDeleteBuffers(1, &mVBOID);

        // OLD delete method:
        // This should delete the data, by replacing it with 1 byte buffer
        // Ideally it should be 0, but then the data is not deleted..
        //fun->glBindBuffer(GL_ARRAY_BUFFER, mCoordinateVBO);
        //fun->glBufferData(GL_ARRAY_BUFFER, 1, NULL, GL_STATIC_DRAW);
        fun->glDeleteBuffers(1, &mCoordinateVBO);
        fun->glBindBuffer(GL_ARRAY_BUFFER, 0);
            /*
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mLineEBO);
            fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, 1, NULL, GL_STATIC_DRAW);
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mTriangleEBO);
            fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, 1, NULL, GL_STATIC_DRAW);
            */
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            fun->glDeleteBuffers(1, &mLineEBO);
        fun->glDeleteBuffers(1, &m_labelVBO);
        fun->glBindBuffer(GL_ARRAY_BUFFER, 0);
        glFinish();
#endif
    }
    mVBOHasData = false;

    mCoordinates.clear();
    mLines.clear();
    mHostHasData = false;
}

void BoundingBoxSet::free(ExecutionDevice::pointer device) {
    // TODO
    if(device->isHost()) {
        mCoordinates.clear();
        mLines.clear();
        mHostHasData = false;
    } else {
    }
}


BoundingBoxSet::~BoundingBoxSet() {
    freeAll();
}

DataBoundingBox BoundingBoxSet::getBoundingBox() const {
    // Find min and max
    if(mCoordinates.size() == 0)
        throw Exception("No coordinates. Can't calculate DataBoundingBox");

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::min();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::min();
    for(int i = 0; i < mCoordinates.size(); i += 3) {
        minX = std::min(mCoordinates[i], minX);
        maxX = std::max(mCoordinates[i], maxX);
        minY = std::min(mCoordinates[i+1], minY);
        maxY = std::max(mCoordinates[i+1], maxY);
    }

    return DataBoundingBox(Vector3f(minX, minY, 0), Vector3f(maxX - minX, maxY - minY, 0));
}

float BoundingBoxSet::getMinimumSize() const {
    return m_minimumSize;
}


BoundingBoxSetAccumulator::BoundingBoxSetAccumulator() {
    createInputPort<BoundingBoxSet>(0);
    createOutputPort<BoundingBoxSet>(0);
}

void BoundingBoxSetAccumulator::execute() {
    auto input = getInputData<BoundingBoxSet>();
    if(!m_accumulatedBBset) {
        m_accumulatedBBset = BoundingBoxSet::create();
    }

    auto outputAccess = m_accumulatedBBset->getAccess(ACCESS_READ_WRITE);
    auto inputAccess = input->getAccess(ACCESS_READ);


	const float offsetX = std::stoi(input->getFrameData("patchid-x")) * std::stoi(input->getFrameData("patch-width")) * std::stof(input->getFrameData("patch-spacing-x"));
	const float offsetY = std::stoi(input->getFrameData("patchid-y")) * std::stoi(input->getFrameData("patch-height")) * std::stof(input->getFrameData("patch-spacing-y"));

	auto coords = inputAccess->getCoordinates();
    for(int i = 0; i < coords.size(); i += 3) {
        coords[i + 0] += offsetX;
        coords[i + 1] += offsetY;
    }

    outputAccess->addBoundingBoxes(coords, inputAccess->getLines(), inputAccess->getLabels(), inputAccess->getScores(), input->getMinimumSize());
    addOutputData(0, m_accumulatedBBset);
}

}