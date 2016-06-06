#ifndef SURFACE_HPP_
#define SURFACE_HPP_

#include "SpatialDataObject.hpp"
#include "DynamicData.hpp"
#include "FAST/Data/Access/Access.hpp"
#include <vector>
#include "FAST/Data/DataTypes.hpp"
#include "FAST/Data/Access/VertexBufferObjectAccess.hpp"
#include "FAST/Data/Access/MeshAccess.hpp"
#include <boost/thread/condition_variable.hpp>

namespace fast {

class Mesh : public SpatialDataObject {
    FAST_OBJECT(Mesh)
    public:
        void create(std::vector<Vector3f> vertices, std::vector<Vector3f> normals, std::vector<Vector3ui> triangles);
        void create(std::vector<MeshVertex> vertices, std::vector<Vector3ui> triangles);
        void create(unsigned int nrOfTriangles);
        VertexBufferObjectAccess::pointer getVertexBufferObjectAccess(accessType access, OpenCLDevice::pointer device);
        MeshAccess::pointer getMeshAccess(accessType access);
        unsigned int getNrOfTriangles() const;
        unsigned int getNrOfVertices() const;
        void setBoundingBox(BoundingBox box);
        ~Mesh();
    private:
        Mesh();
        void freeAll();
        void free(ExecutionDevice::pointer device);

        bool mIsInitialized;
        unsigned int mNrOfTriangles;

        // VBO data
        bool mVBOHasData;
        bool mVBODataIsUpToDate;
        GLuint mVBOID;

        // Host data
        bool mHostHasData;
        bool mHostDataIsUpToDate;
        std::vector<MeshVertex> mVertices;
        std::vector<Vector3ui> mTriangles;

        // Declare as friends so they can get access to the accessFinished methods
        friend class MeshAccess;
        friend class VertexBufferObjectAccess;
};

} // end namespace fast


#endif /* SURFACE_HPP_ */
