#include "BoundingBoxSetAccess.hpp"
#include <FAST/Data/BoundingBox.hpp>
#include <FAST/Reporter.hpp>

namespace fast {

BoundingBoxSetAccess::BoundingBoxSetAccess(
	std::vector<float>* coordinates,
	std::vector<uint>* lines,
	std::vector<uchar>* labels,
	std::vector<float>* scores,
	float* minimumSize,
	std::shared_ptr<BoundingBoxSet> bbset
	) : m_coordinates(coordinates), m_lines(lines), m_labels(labels), m_scores(scores), m_bbset(bbset), m_minimumSize(minimumSize) {

}


void BoundingBoxSetAccess::addBoundingBox(BoundingBox::pointer box) {
	addBoundingBox(box->getPosition(), box->getSize(), box->getLabel(), box->getScore());
}

void BoundingBoxSetAccess::addBoundingBox(Vector2f position, Vector2f size, uchar label, float score) {
	if(!m_released) {
		int count = m_coordinates->size() / 3;

		*m_minimumSize = std::min(*m_minimumSize, size.x());
        *m_minimumSize = std::min(*m_minimumSize, size.y());

		// Add the four corners of a bounding box
		m_coordinates->push_back(position.x());
		m_coordinates->push_back(position.y());
		m_coordinates->push_back(0);

		m_coordinates->push_back(position.x() + size.x());
		m_coordinates->push_back(position.y());
		m_coordinates->push_back(0);

		m_coordinates->push_back(position.x() + size.x());
		m_coordinates->push_back(position.y() + size.y());
		m_coordinates->push_back(0);

		m_coordinates->push_back(position.x());
		m_coordinates->push_back(position.y() + size.y());
		m_coordinates->push_back(0);

		// Lines are pairs (from,to)
		m_lines->push_back(count);
		m_lines->push_back(count + 1);
		m_lines->push_back(count + 1);
		m_lines->push_back(count + 2);
		m_lines->push_back(count + 2);
		m_lines->push_back(count + 3);
		m_lines->push_back(count + 3);
		m_lines->push_back(count);

		// Add label four times, once for each vertex
		m_labels->push_back(label);
		m_labels->push_back(label);
		m_labels->push_back(label);
		m_labels->push_back(label);

		m_scores->push_back(score);
	} else {
		Reporter::warning() << "Bounding box set access was released, but was accessed." << Reporter::end();
	}
}


std::vector<float> BoundingBoxSetAccess::getCoordinates() const {
	return *m_coordinates;
}

std::vector<uint> BoundingBoxSetAccess::getLines() const {
	return *m_lines;
}

std::vector<uchar> BoundingBoxSetAccess::getLabels() const {
	return *m_labels;
}

std::vector<float> BoundingBoxSetAccess::getScores() const {
	return *m_scores;
}

std::shared_ptr<BoundingBox> BoundingBoxSetAccess::getBoundingBox(uint i) const {
    Vector2f position = Vector2f((*m_coordinates)[i*12], (*m_coordinates)[i*12+1]);
    Vector2f size = Vector2f((*m_coordinates)[i*12+3] - position.x(), (*m_coordinates)[i*12+7] - position.y());
    return BoundingBox::create(position, size, (*m_labels)[i], (*m_scores)[i]);
}

void BoundingBoxSetAccess::addBoundingBoxes(const std::vector<float>& coordinates, std::vector<uint> lines, const std::vector<uchar>& labels, const std::vector<float>& scores, float minimumSize) {
	const int size = m_coordinates->size() / 3;
	m_coordinates->insert(m_coordinates->end(), coordinates.begin(), coordinates.end());
	// Have to update indexes of new lines:
	std::transform(lines.begin(), lines.end(), lines.begin(), [size](uint index) -> uint {
		return index + size;
	});
	*m_minimumSize = std::min(*m_minimumSize, minimumSize);
	m_lines->insert(m_lines->end(), lines.begin(), lines.end());
	m_labels->insert(m_labels->end(), labels.begin(), labels.end());
	m_scores->insert(m_scores->end(), scores.begin(), scores.end());
}


void BoundingBoxSetAccess::release() {
	m_bbset->accessFinished();
	m_released = true;
}

BoundingBoxSetAccess::~BoundingBoxSetAccess() {
	release();
}

void BoundingBoxSetAccess::addBoundingBoxes(const std::vector<std::shared_ptr<BoundingBox>>& boxes) {
    m_coordinates->resize(m_coordinates->size() + boxes.size()*12);
    m_lines->resize(m_lines->size() + boxes.size()*8);
    m_labels->resize(m_labels->size() + boxes.size()*4);
    m_scores->resize(m_scores->size() + boxes.size());
    for(uint i = 0; i < boxes.size(); ++i) {
        setBoundingBox(m_scores->size() + i, boxes[i]);
    }
}

void BoundingBoxSetAccess::setBoundingBox(uint i, std::shared_ptr<BoundingBox> box) {
    // out of bounds check
    if(m_coordinates->size() <= i*12)
        throw Exception("Bounding box at index " + std::to_string(i) + " is out of range.");

    auto size = box->getSize();
    *m_minimumSize = std::min(*m_minimumSize, size.x());
    *m_minimumSize = std::min(*m_minimumSize, size.y());

    // Add the four corners of a bounding box
    auto position = box->getPosition();
    auto coords = *m_coordinates;
    coords[12*i + 0] = position.x();
    coords[12*i + 1] = position.y();
    coords[12*i + 2] = 0.0f;

    coords[12*i + 3] = position.x() + size.x();
    coords[12*i + 4] = position.y();
    coords[12*i + 5] = 0.0f;

    coords[12*i + 6] = position.x() + size.x();
    coords[12*i + 7] = position.y() + size.y();
    coords[12*i + 8] = 0.0f;

    coords[12*i + 9] = position.x();
    coords[12*i + 10] = position.y() + size.y();
    coords[12*i + 11] = 0.0f;

    // Lines are pairs (from,to)
    auto lines = *m_lines;
    uint count = i*4;
    lines[i*8 + 0] = count;
    lines[i*8 + 1] = count+1;
    lines[i*8 + 2] = count+1;
    lines[i*8 + 3] = count+2;
    lines[i*8 + 4] = count+2;
    lines[i*8 + 5] = count+3;
    lines[i*8 + 6] = count+3;
    lines[i*8 + 7] = count;

    // Add label four times, once for each vertex
    auto labels = *m_labels;
    auto label = box->getLabel();
    labels[i*4 + 0] = label;
    labels[i*4 + 1] = label;
    labels[i*4 + 2] = label;
    labels[i*4 + 3] = label;

    (*m_scores)[i] = box->getScore();
}

BoundingBoxSetOpenGLAccess::BoundingBoxSetOpenGLAccess(GLuint coordinatesVBO, GLuint linesEBO, GLuint labelVBO, std::shared_ptr<BoundingBoxSet> bbset) :
	m_coordinatesVBO(coordinatesVBO), m_linesEBO(linesEBO), m_labelVBO(labelVBO), m_bbset(bbset) {

}

GLuint BoundingBoxSetOpenGLAccess::getCoordinateVBO() const {
	if(m_released)
		throw Exception("BoundingBoxSet OpenGL access was released.");
	return m_coordinatesVBO;
}

GLuint BoundingBoxSetOpenGLAccess::getLinesEBO() const {
	if(m_released)
		throw Exception("BoundingBoxSet OpenGL access was released.");
	return m_linesEBO;
}

GLuint BoundingBoxSetOpenGLAccess::getLabelVBO() const {
	if(m_released)
		throw Exception("BoundingBoxSet OpenGL access was released.");
	return m_labelVBO;
}

void BoundingBoxSetOpenGLAccess::release() {
	m_bbset->accessFinished();
	m_released = true;
}

BoundingBoxSetOpenGLAccess::~BoundingBoxSetOpenGLAccess() {
	release();
}

}