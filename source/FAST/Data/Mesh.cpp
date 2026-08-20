#include "Mesh.hpp"
#include <thread>
#include "FAST/Utility.hpp"

#ifdef FAST_MODULE_VISUALIZATION
#include "FAST/Visualization/Window.hpp"
#include <QApplication>
#include <QGLFunctions>
#include <QOpenGLFunctions_3_3_Core>
#endif

namespace fast {

Mesh::Mesh(
        const std::vector<MeshVertex>& vertices,
        const std::vector<MeshLine>& lines,
        const std::vector<MeshTriangle>& triangles
    ) : Mesh() {
     if(mIsInitialized) {
        // Delete old data
        freeAll();
    }
    if(vertices.empty()) {
        mBoundingBox = DataBoundingBox(Vector3f(0,0,0));
        mIsInitialized = true;
        mNrOfVertices = 0;
        mNrOfLines = 0;
        m_hasColors = false;
        m_hasLabels = false;
        m_hasNormals = false;
        mUseEBO = false;
        mNrOfTriangles = 0;
        mHostHasData = true;
        mHostDataIsUpToDate = true;
        updateModifiedTimestamp();
    	return;
    }

    mIsInitialized = true;
    std::vector<Vector3f> positions;
    for(int i = 0; i < vertices.size(); i++) {
    	Vector3f pos = vertices[i].getPosition();
        positions.push_back(pos);
        mCoordinates.push_back(pos[0]);
        mCoordinates.push_back(pos[1]);
        mCoordinates.push_back(pos[2]);
        Vector3f normal = vertices[i].getNormal();
        if(normal != Vector3f::Zero() || !mNormals.empty()) {
            if(mNormals.empty())
                mNormals.resize(i*3, 0.0f);
            mNormals.push_back(normal[0]);
            mNormals.push_back(normal[1]);
            mNormals.push_back(normal[2]);
        }
        Color color = vertices[i].getColor();
        if(!color.isNull() || !mColors.empty()) {
            if(mColors.empty())
                mColors.resize(i*3, -1.0f);
            mColors.push_back(color.getRedValue());
            mColors.push_back(color.getGreenValue());
            mColors.push_back(color.getBlueValue());
        }
        auto label = vertices[i].getLabel();
        if(label > 0 || !m_labels.empty()) {
            if(m_labels.empty())
                m_labels.resize(i, 0);
            m_labels.push_back(label);
        }
    }
    for(auto line : lines) {
        mLines.push_back(line.getEndpoint1());
        mLines.push_back(line.getEndpoint2());
    }
    for(auto triangle : triangles) {
        mTriangles.push_back(triangle.getEndpoint1());
        mTriangles.push_back(triangle.getEndpoint2());
        mTriangles.push_back(triangle.getEndpoint3());
    }

	if(vertices.size() > 0) {
		mBoundingBox = DataBoundingBox(positions);
	} else {
		mBoundingBox = DataBoundingBox(Vector3f(0,0,0)); // TODO Fix
	}
    mNrOfVertices = vertices.size();
    mNrOfLines = lines.size();
    mNrOfTriangles = triangles.size();
    m_hasColors = !mColors.empty();
    m_hasLabels = !m_labels.empty();
    m_hasNormals = !mNormals.empty();
    mUseEBO = true;
    mHostHasData = true;
    mHostDataIsUpToDate = true;
    updateModifiedTimestamp();
}

Mesh::Mesh(
        uint nrOfVertices,
        uint nrOfLines,
        uint nrOfTriangles,
        bool useColors,
        bool useNormals,
        bool useEBO
        ) : Mesh() {
    if(mIsInitialized) {
        // Delete old data
        freeAll();
    }
    mBoundingBox = DataBoundingBox(Vector3f(0,0,0));
    mIsInitialized = true;
    mNrOfVertices = nrOfVertices;
    mNrOfLines = nrOfLines;
    m_hasColors = useColors;
    m_hasNormals = useNormals;
    mUseEBO = useEBO;
    mNrOfTriangles = nrOfTriangles;
    updateModifiedTimestamp();
}

VertexBufferObjectAccess::pointer Mesh::getVertexBufferObjectAccess(
        accessType type) {
    if(!mIsInitialized)
        throw Exception("Mesh has not been initialized.");

    blockIfBeingWrittenTo();

    if (type == ACCESS_READ_WRITE) {
    	blockIfBeingAccessed();
        std::unique_lock<std::mutex> lock(mDataIsBeingWrittenToMutex);
        mDataIsBeingWrittenTo = true;
    }
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
        if(mHostHasData) {
            fun->glDeleteBuffers(1, &mNormalVBO);
            fun->glDeleteBuffers(1, &mColorVBO);
            fun->glDeleteBuffers(1, &m_labelVBO);
            fun->glDeleteBuffers(1, &mLineEBO);
            fun->glDeleteBuffers(1, &mTriangleEBO);
            fun->glGenBuffers(1, &mNormalVBO);
            fun->glGenBuffers(1, &mColorVBO);
            fun->glGenBuffers(1, &m_labelVBO);
            fun->glGenBuffers(1, &mLineEBO);
            fun->glGenBuffers(1, &mTriangleEBO);
            // If host has data, transfer it.
            // Coordinates
            fun->glBindBuffer(GL_ARRAY_BUFFER, mCoordinateVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, mCoordinates.size()*sizeof(float), mCoordinates.data(), GL_STATIC_DRAW);
            // Normals
            fun->glBindBuffer(GL_ARRAY_BUFFER, mNormalVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, mNormals.size()*sizeof(float), mNormals.data(), GL_STATIC_DRAW);
            // Color
            fun->glBindBuffer(GL_ARRAY_BUFFER, mColorVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, mColors.size()*sizeof(float), mColors.data(), GL_STATIC_DRAW);
            // Labels
            fun->glBindBuffer(GL_ARRAY_BUFFER, m_labelVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, m_labels.size()*sizeof(uchar), m_labels.data(), GL_STATIC_DRAW);
            // Line EBO
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mLineEBO);
            fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mLines.size()*sizeof(uint), mLines.data(), GL_STATIC_DRAW);
            // Triangle EBO
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mTriangleEBO);
            fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mTriangles.size()*sizeof(uint), mTriangles.data(), GL_STATIC_DRAW);
        } else {
            // Only allocate space
            fun->glBindBuffer(GL_ARRAY_BUFFER, mCoordinateVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, mNrOfVertices*3*sizeof(float), NULL, GL_STATIC_DRAW);
            if(m_hasNormals) {
                fun->glDeleteBuffers(1, &mNormalVBO);
                fun->glGenBuffers(1, &mNormalVBO);
                fun->glBindBuffer(GL_ARRAY_BUFFER, mNormalVBO);
                fun->glBufferData(GL_ARRAY_BUFFER, mNrOfVertices * 3 * sizeof(float), NULL, GL_STATIC_DRAW);
            }
            if(m_hasColors) {
                fun->glDeleteBuffers(1, &mColorVBO);
                fun->glGenBuffers(1, &mColorVBO);
                fun->glBindBuffer(GL_ARRAY_BUFFER, mColorVBO);
                fun->glBufferData(GL_ARRAY_BUFFER, mNrOfVertices * 3 * sizeof(float), NULL, GL_STATIC_DRAW);
            }
            if(m_hasLabels) {
                fun->glDeleteBuffers(1, &m_labelVBO);
                fun->glGenBuffers(1, &m_labelVBO);
                fun->glBindBuffer(GL_ARRAY_BUFFER, m_labelVBO);
                fun->glBufferData(GL_ARRAY_BUFFER, mNrOfVertices * sizeof(uchar), NULL, GL_STATIC_DRAW);
            }
            if(mUseEBO) {
                // Line EBO
                fun->glDeleteBuffers(1, &mLineEBO);
                fun->glGenBuffers(1, &mLineEBO);
                fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mLineEBO);
                fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mNrOfLines * 2 * sizeof(uint), NULL, GL_STATIC_DRAW);
                // Triangle EBO
                fun->glDeleteBuffers(1, &mTriangleEBO);
                fun->glGenBuffers(1, &mTriangleEBO);
                fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mTriangleEBO);
                fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mNrOfTriangles * 3 * sizeof(uint), NULL, GL_STATIC_DRAW);
            }
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
        throw Exception("Creating mesh with VBO is disabled as FAST module visualization is disabled.");
#endif
    } else {
        if(!mVBODataIsUpToDate) {
            // Update data
#ifdef FAST_MODULE_VISUALIZATION
            QGLFunctions *fun = Window::getMainGLContext()->functions();
            // Coordinates
            fun->glBindBuffer(GL_ARRAY_BUFFER, mCoordinateVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, mCoordinates.size()*sizeof(float), mCoordinates.data(), GL_STATIC_DRAW);
            // Normals
            fun->glBindBuffer(GL_ARRAY_BUFFER, mNormalVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, mNormals.size()*sizeof(float), mNormals.data(), GL_STATIC_DRAW);
            // Color
            fun->glBindBuffer(GL_ARRAY_BUFFER, mColorVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, mColors.size()*sizeof(float), mColors.data(), GL_STATIC_DRAW);
            // Label
            fun->glBindBuffer(GL_ARRAY_BUFFER, m_labelVBO);
            fun->glBufferData(GL_ARRAY_BUFFER, m_labels.size()*sizeof(uchar), m_labels.data(), GL_STATIC_DRAW);
            // Line EBO
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mLineEBO);
            fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mLines.size()*sizeof(uint), mLines.data(), GL_STATIC_DRAW);
            // Triangle EBO
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mTriangleEBO);
            fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mTriangles.size()*sizeof(uint), mTriangles.data(), GL_STATIC_DRAW);

#else
            throw Exception("Creating mesh with VBO is disabled as FAST module visualization is disabled.");
#endif
        } else {
        }
    }

    if(type == ACCESS_READ_WRITE) {
        updateModifiedTimestamp();
        setAllDataToOutOfDate();
    }

    mVBODataIsUpToDate = true;

    {
        std::unique_lock<std::mutex> lock(mDataIsBeingAccessedMutex);
        mDataIsBeingAccessed = true;
    }

	VertexBufferObjectAccess::pointer accessObject(
            new VertexBufferObjectAccess(
                    mCoordinateVBO,
                    mNormalVBO,
                    mColorVBO,
                    m_labelVBO,
                    mLineEBO,
                    mTriangleEBO,
                    m_hasNormals,
                    m_hasColors,
                    m_hasLabels,
                    mUseEBO,
                    std::static_pointer_cast<Mesh>(mPtr.lock())
            )
    );
	return std::move(accessObject);
}


// Hasher for the MeshVertex class
class KeyHasher {
    public:
        std::size_t operator()(const MeshVertex& v) const {

            // Start with a hash value of 0    .
            std::size_t seed = 0;

            // Modify 'seed' by XORing and bit-shifting in
            // one member of 'Key' after the other:
            // http://en.cppreference.com/w/cpp/utility/hash
            hash_combine(seed, std::hash<float>{}(v.getPosition().x()));
            hash_combine(seed, std::hash<float>{}(v.getPosition().y()));
            hash_combine(seed, std::hash<float>{}(v.getPosition().z()));

            // Return the result.
            return seed;
        }
};

bool operator==(const MeshVertex& a, const MeshVertex& b) {
    return a.getPosition()[0] == b.getPosition()[0] &&
    a.getPosition()[1] == b.getPosition()[1] &&
    a.getPosition()[2] == b.getPosition()[2];
}

MeshAccess::pointer Mesh::getMeshAccess(accessType type) {
    if(!mIsInitialized) {
        throw Exception("Mesh has not been initialized.");
    }

    blockIfBeingWrittenTo();

    if(type == ACCESS_READ_WRITE) {
    	blockIfBeingAccessed();
        std::lock_guard<std::mutex> lock(mDataIsBeingWrittenToMutex);
        mDataIsBeingWrittenTo = true;
    }
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
        mNormals.resize(mNrOfVertices*3);
        if(m_hasNormals) {
            fun->glBindBuffer(GL_ARRAY_BUFFER, mNormalVBO);
            fun->glGetBufferSubData(GL_ARRAY_BUFFER, 0, mNrOfVertices * 3 * sizeof(float), mNormals.data());
        }
        mColors.resize(mNrOfVertices*3);
        if(m_hasColors) {
            fun->glBindBuffer(GL_ARRAY_BUFFER, mColorVBO);
            fun->glGetBufferSubData(GL_ARRAY_BUFFER, 0, mNrOfVertices * 3 * sizeof(float), mColors.data());
        }
        m_labels.resize(mNrOfVertices);
        if(m_hasLabels) {
            fun->glBindBuffer(GL_ARRAY_BUFFER, m_labelVBO);
            fun->glGetBufferSubData(GL_ARRAY_BUFFER, 0, mNrOfVertices * sizeof(uchar), m_labels.data());
        }
        fun->glBindBuffer(GL_ARRAY_BUFFER, 0);

        mLines.resize(mNrOfLines*2);
        mTriangles.resize(mNrOfTriangles*3);
        if(mUseEBO) {
              // Line EBO
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mLineEBO);
            fun->glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mNrOfLines * 2 * sizeof(uint), mLines.data());

            // Triangle EBO
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mTriangleEBO);
            fun->glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mNrOfTriangles * 3 * sizeof(uint), mTriangles.data());

            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        } else {
            // If EBO is not used, it is assumed that all vertices define consecutive lines/triangles
            uint counter = 0;
            for(int i = 0; i < mNrOfLines; ++i) {
                mLines[i*2] = counter;
                mLines[i*2+1] = counter+1;
                counter += 2;
            }
            counter = 0;
            for(int i = 0; i < mNrOfTriangles; ++i) {
                mTriangles[i*3] = counter;
                mTriangles[i*3+1] = counter+1;
                mTriangles[i*3+2] = counter+2;
                counter += 3;
            }
        }


        mHostHasData = true;
        mHostDataIsUpToDate = true;
#else
        throw Exception("Creating mesh with VBO is disabled as FAST module visualization is disabled.");
#endif
    } else {
        if(!mHostDataIsUpToDate) {
            throw Exception("Not implemented yet!");
        }
    }

    if(type == ACCESS_READ_WRITE) {
        updateModifiedTimestamp();
        setAllDataToOutOfDate();
    }

    mHostDataIsUpToDate = true;

    {
        std::lock_guard<std::mutex> lock(mDataIsBeingAccessedMutex);
        mDataIsBeingAccessed = true;
    }

    MeshAccess::pointer accessObject(new MeshAccess(&mCoordinates, &mNormals, &mColors, &m_labels, &mLines, &mTriangles, std::static_pointer_cast<Mesh>(mPtr.lock())));
	return std::move(accessObject);
}

void Mesh::updateOpenCLBufferData(OpenCLDevice::pointer device) {
    // If buffer is up to date, no need to update
    if(mCLBuffersIsUpToDate.count(device) > 0 && mCLBuffersIsUpToDate[device] == true)
        return;

    if(!mHostHasData)
        throw Exception("Mesh currently need host data before getting CL buffer data");

    if(mCLBuffersIsUpToDate.count(device) == 0) {
        reportInfo() << "Creating OpenCL buffers for mesh" << reportEnd();

        // Allocate OpenCL buffers, need to know how many coordinates and how many connections
        size_t bufferSize = sizeof(float) * 3 * mNrOfVertices;
        cl::Buffer* coordinatesBuffer = new cl::Buffer(device->getContext(), CL_MEM_READ_WRITE, bufferSize);
        mCoordinatesBuffers[device] = coordinatesBuffer;

        if(mLines.size() > 0) {
            bufferSize = sizeof(int) * 2 * mNrOfLines;
            cl::Buffer *lineBuffer = new cl::Buffer(device->getContext(), CL_MEM_READ_WRITE, bufferSize);
            mLinesBuffers[device] = lineBuffer;
        } else {
            mLinesBuffers[device] = nullptr;
        }

        if(mTriangles.size() > 0) {
            bufferSize = sizeof(int) * 3 * mNrOfTriangles;
            cl::Buffer *triangleBuffer = new cl::Buffer(device->getContext(), CL_MEM_READ_WRITE, bufferSize);
            mTrianglesBuffers[device] = triangleBuffer;
        } else {
            mTrianglesBuffers[device] = nullptr;
        }
    }

    if(mHostHasData && mHostDataIsUpToDate) {
        reportInfo() << "Transfer data from host to CL buffers" << reportEnd();
        // TODO Update data from host

        // Transfer coordinates
        cl::CommandQueue queue = device->getCommandQueue();
        queue.enqueueWriteBuffer(*mCoordinatesBuffers[device], CL_TRUE, 0, mNrOfVertices*3*sizeof(float), mCoordinates.data());

        // Transfer lines
        if(mLines.size() > 0) {
            queue.enqueueWriteBuffer(*mLinesBuffers[device], CL_TRUE, 0, mNrOfLines*2*sizeof(uint), mLines.data());
        }

        // Transfer triangles
        if(mTriangles.size() > 0) {
            queue.enqueueWriteBuffer(*mTrianglesBuffers[device], CL_TRUE, 0, mNrOfTriangles*3*sizeof(uint), mTriangles.data());
        }


    } else if(mVBOHasData && mVBODataIsUpToDate) {
        // TODO update data from VBO
        throw Exception("Mesh currently need host data before getting CL buffer data");
    } else {
        // No data to update from
    }

    mCLBuffersIsUpToDate[device] = true;
}

void Mesh::setAllDataToOutOfDate() {
    mHostDataIsUpToDate = false;
    mVBODataIsUpToDate = false;
    std::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
    for(it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end(); it++) {
        it->second = false;
    }
}

MeshOpenCLAccess::pointer Mesh::getOpenCLAccess(accessType type, OpenCLDevice::pointer device) {
     if(!mIsInitialized) {
        throw Exception("Surface has not been initialized.");
    }

    blockIfBeingWrittenTo();

    if(type == ACCESS_READ_WRITE) {
    	blockIfBeingAccessed();
        std::unique_lock<std::mutex> lock(mDataIsBeingWrittenToMutex);
        mDataIsBeingWrittenTo = true;
    }
    updateOpenCLBufferData(device);
    if(type == ACCESS_READ_WRITE) {
        setAllDataToOutOfDate();
        updateModifiedTimestamp();
    }
    mCLBuffersIsUpToDate[device] = true;
    {
        std::unique_lock<std::mutex> lock(mDataIsBeingAccessedMutex);
        mDataIsBeingAccessed = true;
    }

    MeshOpenCLAccess::pointer accessObject(new MeshOpenCLAccess(mCoordinatesBuffers[device], mLinesBuffers[device], mTrianglesBuffers[device], std::static_pointer_cast<Mesh>(mPtr.lock())));
	return std::move(accessObject);
}

Mesh::~Mesh() {
    freeAll();
}

Mesh::Mesh() {
    mIsInitialized = false;
    mVBOHasData = false;
    mVBODataIsUpToDate = false;
    m_hasColors = false;
    m_hasNormals = false;
    mUseEBO = false;
    mHostHasData = false;
    mHostDataIsUpToDate = false;
}

void Mesh::freeAll() {
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
        if(m_hasNormals) {
            //fun->glBindBuffer(GL_ARRAY_BUFFER, mNormalVBO);
            //fun->glBufferData(GL_ARRAY_BUFFER, 1, NULL, GL_STATIC_DRAW);
            fun->glDeleteBuffers(1, &mNormalVBO);
        }
        if(m_hasColors) {
            //fun->glBindBuffer(GL_ARRAY_BUFFER, mColorVBO);
            //fun->glBufferData(GL_ARRAY_BUFFER, 1, NULL, GL_STATIC_DRAW);
            fun->glDeleteBuffers(1, &mColorVBO);
        }
        if(m_hasLabels) {
            fun->glDeleteBuffers(1, &m_labelVBO);
        }
        fun->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if(mUseEBO) {
            /*
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mLineEBO);
            fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, 1, NULL, GL_STATIC_DRAW);
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mTriangleEBO);
            fun->glBufferData(GL_ELEMENT_ARRAY_BUFFER, 1, NULL, GL_STATIC_DRAW);
            */
            fun->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            fun->glDeleteBuffers(1, &mLineEBO);
            fun->glDeleteBuffers(1, &mTriangleEBO);
        }
        glFinish();
#endif
    }
    mVBOHasData = false;

    // For each CL buffer delete it
    std::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
    for(it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end(); it++) {
        if(mLinesBuffers.count(it->first) > 0)
            delete mLinesBuffers[it->first];
        if(mTrianglesBuffers.count(it->first) > 0)
            delete mTrianglesBuffers[it->first];
        delete mCoordinatesBuffers[it->first];
    }
    mCoordinatesBuffers.clear();
    mLinesBuffers.clear();
    mTrianglesBuffers.clear();
    mCLBuffersIsUpToDate.clear();

    mCoordinates.clear();
    mNormals.clear();
    m_labels.clear();
    mColors.clear();
    mLines.clear();
    mTriangles.clear();
    mHostHasData = false;
}

void Mesh::free(ExecutionDevice::pointer device) {
    // TODO
    if(device->isHost()) {
        mCoordinates.clear();
        mNormals.clear();
        mColors.clear();
        mLines.clear();
        m_labels.clear();
        mTriangles.clear();
        mHostHasData = false;
    } else {
        OpenCLDevice::pointer clDevice = std::static_pointer_cast<OpenCLDevice>(device);
        if(mCLBuffersIsUpToDate.count(clDevice) > 0) {
            mCLBuffersIsUpToDate.erase(clDevice);
            if(mLinesBuffers.count(clDevice) > 0)
                delete mLinesBuffers[clDevice];
            if(mTrianglesBuffers.count(clDevice) > 0)
                delete mTrianglesBuffers[clDevice];
            delete mCoordinatesBuffers[clDevice];
            mLinesBuffers.erase(clDevice);
            mTrianglesBuffers.erase(clDevice);
            mCoordinatesBuffers.erase(clDevice);
        }
    }
}

int Mesh::getNrOfTriangles() {
    return mNrOfTriangles;
}

int Mesh::getNrOfVertices() {
    return mNrOfVertices;
}

void Mesh::setBoundingBox(DataBoundingBox box) {
    mBoundingBox = box;
}

int Mesh::getNrOfLines() {
	return mNrOfLines;
}

void Mesh::accessFinished() {
    if(mHostHasData && mHostDataIsUpToDate) {
        // Make sure state is updated when mesh is modified on host
        m_hasColors = !mColors.empty();
        m_hasLabels = !m_labels.empty();
        m_hasNormals = !mNormals.empty();
        mNrOfVertices = mCoordinates.size() / 3;
        mNrOfLines = mLines.size() / 2;
        mNrOfTriangles = mTriangles.size() / 3;
        mUseEBO = true;
    }
    DataObject::accessFinished();
}

bool Mesh::hasNormals() const {
    return m_hasNormals;
}

bool Mesh::hasColors() const {
    return m_hasNormals;
}

bool Mesh::hasLabels() const {
    return m_hasLabels;
}

} // end namespace fast


