#ifndef LINE_SET_ACCESS_HPP_
#define LINE_SET_ACCESS_HPP_

#include "FAST/Data/DataTypes.hpp"
#include "Access.hpp"
#include <vector>
#include "FAST/SmartPointers.hpp"

namespace fast {

class LineSet;

class LineSetAccess {
    public:
        LineSetAccess(std::vector<Vector3f>* vertices, std::vector<Vector2ui>* lines, SharedPointer<LineSet> object);
        Vector3f getPoint(uint i) const;
        void setPoint(uint i, const Vector3f point);
        void addPoint(const Vector3f point);
        uint getNrOfPoints() const;
        uint getNrOfLines() const;
        /**
         * Create line between i and j
         */
        void addLine(uint i, uint j);
        Vector2ui getLine(uint i) const;
        void deleteLine(uint i);
        void deletePoint(uint i);
        void release();
        ~LineSetAccess();
		typedef UniquePointer<LineSetAccess> pointer;
    private:
        std::vector<Vector3f>* mVertices;
        std::vector<Vector2ui>* mLines;
        SharedPointer<LineSet> mObject;
};

} // end namespace fast

#endif
