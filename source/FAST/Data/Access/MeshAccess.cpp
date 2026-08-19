#include "MeshAccess.hpp"
#include "FAST/Data/Mesh.hpp"

namespace fast {

MeshAccess::MeshAccess(
        std::vector<float>* coordinates,
        std::vector<float>* normals,
        std::vector<float>* colors,
        std::vector<uchar>* labels,
        std::vector<uint>* lines,
        std::vector<uint>* triangles,
        std::shared_ptr<Mesh> mesh) {

    mCoordinates = coordinates;
    mNormals = normals;
    mColors = colors;
    m_labels = labels;
    mLines = lines;
    mTriangles = triangles;
    mMesh = mesh;
}

void MeshAccess::release() {
	mMesh->accessFinished();
}

MeshAccess::~MeshAccess() {
	release();
}

void MeshAccess::setVertex(uint i, MeshVertex vertex, bool updateBoundingBox) {
    Vector3f pos = vertex.getPosition();
    (*mCoordinates)[i*3] = pos[0];
    (*mCoordinates)[i*3+1] = pos[1];
    (*mCoordinates)[i*3+2] = pos[2];
    Vector3f normal = vertex.getNormal();
    if(normal != Vector3f::Zero() || !mNormals->empty()) {
        if(mNormals->empty())
            mNormals->resize((i+1)*3, 0.0f);
        (*mNormals)[i*3] = normal[0];
        (*mNormals)[i*3+1] = normal[1];
        (*mNormals)[i*3+2] = normal[2];
    }
    Color color = vertex.getColor();
    if(!color.isNull() || !mColors->empty()) {
        if(mColors->empty())
            mColors->resize((i+1)*3, -1.0f);
        (*mColors)[i*3] = color.getRedValue();
        (*mColors)[i*3+1] = color.getGreenValue();
        (*mColors)[i*3+2] = color.getBlueValue();
    }
    auto label = vertex.getLabel();
    if(label > 0 || !m_labels->empty()) {
        if(m_labels->empty())
            m_labels->resize(i+1, 0);
        (*m_labels)[i] = label;
    }
    if(updateBoundingBox) {
        auto box = mMesh->getBoundingBox();
        box.update({vertex.getPosition()});
        mMesh->setBoundingBox(box);
    }
}

MeshVertex MeshAccess::getVertex(uint i) {
    Vector3f coordinate((*mCoordinates)[i*3], (*mCoordinates)[i*3+1], (*mCoordinates)[i*3+2]);

    Color color;
    if(!mColors->empty())
        color = Color((*mColors)[i*3], (*mColors)[i*3+1], (*mColors)[i*3+2]);

    Vector3f normal = Vector3f::Zero();
    if(!mNormals->empty())
        normal = Vector3f((*mNormals)[i*3], (*mNormals)[i*3+1], (*mNormals)[i*3+2]);

    auto label = 0;
    if(!m_labels->empty())
        label = (*m_labels)[i];

    return MeshVertex(coordinate, normal, color, label);
}

MeshTriangle MeshAccess::getTriangle(uint i) {
    MeshTriangle triangle((*mTriangles)[i*3], (*mTriangles)[i*3+1], (*mTriangles)[i*3+2]);
    return triangle;
}

void MeshAccess::setLine(uint i, MeshLine line) {
    (*mLines)[i*2] = line.getEndpoint1();
    (*mLines)[i*2+1] = line.getEndpoint2();
}

MeshLine MeshAccess::getLine(uint i) {
    MeshLine line((*mLines)[i*2], (*mLines)[i*2+1]);
    return line;
}

void MeshAccess::setTriangle(uint i, MeshTriangle triangle) {
    (*mTriangles)[i*3] = triangle.getEndpoint1();
    (*mTriangles)[i*3+1] = triangle.getEndpoint2();
    (*mTriangles)[i*3+2] = triangle.getEndpoint3();
}

std::vector<MeshVertex> MeshAccess::getVertices() {
    std::vector<MeshVertex> vertex;
    for(uint i = 0; i < mCoordinates->size()/3; i++) {
        vertex.push_back(getVertex(i));
    }
    return vertex;
}

std::vector<MeshTriangle> MeshAccess::getTriangles() {
    std::vector<MeshTriangle> triangles;
    for(uint i = 0; i < mTriangles->size()/3; i++) {
        triangles.push_back(getTriangle(i));
    }
    return triangles;
}

std::vector<MeshLine> MeshAccess::getLines() {
    std::vector<MeshLine> lines;
    for(uint i = 0; i < mLines->size()/2; i++) {
        lines.push_back(getLine(i));
    }
    return lines;
}

void MeshAccess::addVertex(MeshVertex v) {
    // Add dummy values
    mCoordinates->push_back(0);
    mCoordinates->push_back(0);
    mCoordinates->push_back(0);
    if(!mNormals->empty()) {
        mNormals->push_back(0);
        mNormals->push_back(0);
        mNormals->push_back(0);
    }
    if(!mColors->empty()) {
        mColors->push_back(0);
        mColors->push_back(0);
        mColors->push_back(0);
    }
    if(!m_labels->empty())
        m_labels->push_back(0);
    setVertex(mCoordinates->size()/3 - 1, v);
}

void MeshAccess::addTriangle(MeshTriangle t) {
    mTriangles->push_back(t.getEndpoint1());
    mTriangles->push_back(t.getEndpoint2());
    mTriangles->push_back(t.getEndpoint3());
}

void MeshAccess::addLine(MeshLine l) {
    mLines->push_back(l.getEndpoint1());
    mLines->push_back(l.getEndpoint2());
}

void MeshAccess::addVertices(const std::vector<MeshVertex>& vertices) {
    int startIndex = (int)(mCoordinates->size()/3);
    mCoordinates->resize(mCoordinates->size() + vertices.size()*3, 0);
    std::vector<Vector3f> coords;
    bool resizedColors = false; // only do it once if needed
    bool resizedLabels = false;
    bool resizedNormals = false;
    for(int i = 0; i < vertices.size(); ++i) {
        if(!resizedNormals && (!mNormals->empty() || vertices[i].getNormal() != Vector3f::Zero())) {
            mNormals->resize(startIndex*3 + vertices.size()*3, 0);
            resizedNormals = true;
        }
        if(!resizedColors && (!mColors->empty() || !vertices[i].getColor().isNull())) {
            mColors->resize(startIndex*3 + vertices.size()*3, -1);
            resizedColors = true;
        }
        if(!resizedLabels && (!m_labels->empty() || vertices[i].getLabel() != 0)) {
            m_labels->resize(startIndex + vertices.size(), 0);
            resizedLabels = true;
        }
        setVertex(startIndex + i, vertices[i], false);
        coords.push_back(vertices[i].getPosition());
    }
    auto box = mMesh->getBoundingBox();
    box.update(coords);
    mMesh->setBoundingBox(box);
}

} // end namespace fast


