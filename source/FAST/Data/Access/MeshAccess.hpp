#pragma once

#include <vector>
#include <FAST/Data/DataTypes.hpp>
#include "FAST/Data/MeshVertex.hpp"


namespace fast {

class Mesh;

/**
 * @brief CPU access to Mesh data
 * @sa Mesh
 * @ingroup access
 */
class FAST_EXPORT  MeshAccess {
    public:
        MeshAccess(
            std::vector<float>* mCoordinates,
            std::vector<float>* mNormals,
            std::vector<float>* mColors,
            std::vector<uchar>* labels,
            std::vector<uint>* mLines,
            std::vector<uint>* mTriangles,
            std::shared_ptr<Mesh> mesh
		);
        /**
         * @brief Get vertex at position i
         * @param i index
         */
        MeshVertex getVertex(uint i);
        /**
         * @brief Set vertex at given index
         * @param i index
         * @param vertex Vertex
         * @param updateBoundingBox Whether to update the Mesh bounding box or not
         */
        void setVertex(uint i, MeshVertex vertex, bool updateBoundingBox = true);
        void addVertex(MeshVertex v);
        void addVertices(const std::vector<MeshVertex>& vertices);
        void addLines(const std::vector<MeshLine>& lines);
        void addTriangles(const std::vector<MeshTriangle>& triangles);
        MeshTriangle getTriangle(uint i);
		void setTriangle(uint i, MeshTriangle);
		void addTriangle(MeshTriangle t);
        MeshLine getLine(uint i);
		void setLine(uint i, MeshLine);
		void addLine(MeshLine l);
        std::vector<MeshTriangle> getTriangles();
        std::vector<MeshLine> getLines();
        std::vector<MeshVertex> getVertices();
        void release();
        ~MeshAccess();
		typedef std::unique_ptr<MeshAccess> pointer;
    private:
		std::vector<float>* mCoordinates;
		std::vector<float>* mNormals;
		std::vector<float>* mColors;
        std::vector<uchar>* m_labels;
		std::vector<uint>* mLines;
		std::vector<uint>* mTriangles;
        std::shared_ptr<Mesh> mMesh;
};

} // end namespace fast

