#include "SegmentationLabelRenderer.hpp"
#include "FAST/Visualization/View.hpp"
#include <QPainter>
#include <QApplication>
#include <FAST/Data/Image.hpp>
#include <QScreen>
#include <FAST/Algorithms/Region/RegionProperties.hpp>

namespace fast {

void SegmentationLabelRenderer::loadAttributes() {
    auto classTitles = getStringListAttribute("label-text");
    for(int i = 0; i < classTitles.size(); i += 2) {
        setLabelName(std::stoi(classTitles[i]), classTitles[i+1]);
    }
    auto classColors = getStringListAttribute("label-color");
    for(int i = 0; i < classColors.size(); i += 2) {
        setColor(std::stoi(classColors[i]), Color::fromString(classColors[i+1]));
    }
    setAreaThreshold(getFloatAttribute("area-threshold"));
}

void SegmentationLabelRenderer::setAreaThreshold(float threshold) {
    m_areaThreshold = threshold;
    setModified(true);
}

SegmentationLabelRenderer::SegmentationLabelRenderer(std::map<uint, std::string> labelNames, std::map<uint, Color> labelColors, float areaThreshold) {
    createInputPort(0, "Image");
    m_2Donly = true;
    mFontSize = 28;
    setLabelNames(labelNames);
    setColors(labelColors);
    setAreaThreshold(areaThreshold);

    createShaderProgram({
                                Config::getKernelSourcePath() + "/Visualization/SegmentationLabelRenderer/SegmentationLabelRenderer.vert",
                                Config::getKernelSourcePath() + "/Visualization/SegmentationLabelRenderer/SegmentationLabelRenderer.frag",
                        });

    createStringAttribute("label-text", "Label Text", "Text title of each class label", "");
    createStringAttribute("label-color", "Label Colors", "Color of each class label", "");
    createFloatAttribute("area-threshold", "Area threshold", "Only show labels for objects larger than this threshold. Area is in mm^2", m_areaThreshold);
}


void SegmentationLabelRenderer::execute() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if(mStop) {
            return;
        }
    }

    // This simply gets the input data for each connection and puts it into a data structure
    for(uint inputNr = 0; inputNr < getNrOfInputConnections(); inputNr++) {
        if(hasNewInputData(inputNr)) {
            SpatialDataObject::pointer input = getInputData<SpatialDataObject>(inputNr);

            {
                std::lock_guard<std::mutex> lock(mMutex);
                if(mHasRendered) {
                    mHasRendered = false;
                    mDataToRender[inputNr] = input;
                    auto regionProps = RegionProperties::create(false);
                    regionProps->setInputData(input);
                    m_regions[inputNr] = regionProps->updateAndGetOutputData<RegionList>();
                }
            }
        }
    }
}

void SegmentationLabelRenderer::draw(Matrix4f perspectiveMatrix, Matrix4f viewingMatrix, float zNear, float zFar,
                                     bool mode2D, int viewWidth,
                                     int viewHeight) {
    if(!mode2D)
        throw Exception("SegmentationLabelRenderer is only implemented for 2D at the moment");

    auto dataToRender = getDataToRender();
    for(auto it : dataToRender) {
        auto input = std::static_pointer_cast<Image>(it.second);
        if(input->getDataType() != TYPE_UINT8)
            throw Exception("Input to SegmentationLabelRenderer must be image of type UINT8");

        uint inputNr = it.first;

        // Check if a texture has already been created for image
        if(mTexturesToRender.count(inputNr) > 0 && mImageUsed[inputNr] == input && mDataTimestamp[inputNr] == input->getTimestamp())
            continue;

        if(mTexturesToRender.count(inputNr) > 0) {
            // Delete old texture
            glDeleteTextures(1, &mTexturesToRender[inputNr]);
            mTexturesToRender.erase(inputNr);
            glDeleteVertexArrays(1, &mVAO[inputNr]);
            mVAO.erase(inputNr);
        }


        // Font setup
        // Draw the text image with a larger resolution than the input if needed
        int width = input->getWidth();
        float scale = 1.0f;
        if(width < 1024) {
            scale = 1024.0f/width;
            width = 1024;
        }
        mScales[inputNr] = scale;
        int height = scale*input->getHeight();
        Vector3f spacing = input->getSpacing();
        // Compensate for anistropic spacing, (have to use isotropic spacing for text image)
        float spacingScale = spacing.y() / spacing.x();
        height = spacingScale*height;

        // create the QImage and draw txt into it
        QImage textimg(width, height, QImage::Format_RGBA8888);
        textimg.fill(QColor(0, 0, 0, 0));
        QPainter painter(&textimg);
        float pixelArea = spacing.x()*spacing.y();
        std::unique_lock<std::mutex> lock(mMutex);
        auto regionsCopy = m_regions;
        lock.unlock();
        for(auto& region : regionsCopy[inputNr]->get()) {

            if(region.pixelCount * pixelArea < m_areaThreshold) // If object to small.. (area in mm^2)
                continue;

            // If no label name has been set. Skip it
            if(m_labelNames.count(region.label) == 0)
                continue;

            float size = 1.0f;
            if(m_dynamicSize) {
                size = 1.0f + 0.01f*(region.pixelCount * (spacing.x() * spacing.x()) - 1.0f);
            } else {
                size = m_textHeightInMM;
            }

            const int textHeightInPixels = size / (spacing.x()/scale);

            QFont font = QApplication::font();
            font.setPixelSize(textHeightInPixels);
            QFontMetrics metrics(font);

            // Set label text
            std::string text = m_labelNames[region.label];

            // Get size of text
            const int textWidth = metrics.boundingRect(QString(text.c_str())).width()+3;
            const int textHeight = metrics.boundingRect(QString(text.c_str())).height();

            // Set color
            auto selectedColor = Color::Green();
            if(m_labelColors.count(region.label) > 0)
                selectedColor = m_labelColors[region.label];
            QColor color(selectedColor.getRedValue() * 255, selectedColor.getGreenValue() * 255, selectedColor.getBlueValue() * 255, 255);

            // Determine position
            Vector2f position;
            if(m_centerPosition) {
                position.x() = region.centroid.x()/spacing.x()*scale - textWidth * 0.5f;
                position.y() = region.centroid.y()/spacing.y()*scale*spacingScale + textHeight * 0.5f;
            } else {
                position.x() = region.centroid.x()/spacing.x()*scale;
                position.y() = region.centroid.y()/spacing.y()*scale*spacingScale + textHeight;
            }

            // Draw it
            painter.setBrush(color);
            painter.setPen(color);
            painter.setFont(font);
            painter.drawText(position.x(), position.y(), text.c_str());
        }
        QImage textimgFlipped = textimg.mirrored();

        // Make texture of QImage
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     textimg.width(), textimg.height(),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, textimgFlipped.bits());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Draw texture on a quad
        // Delete old VAO
        if(mVAO.count(inputNr) > 0)
            glDeleteVertexArrays(1, &mVAO[inputNr]);
        GLuint VAO_ID;
        glGenVertexArrays(1, &VAO_ID);
        mVAO[inputNr] = VAO_ID;
        glBindVertexArray(VAO_ID);
        float vertices[] = {
                // vertex: x, y, z; tex coordinates: x, y
                0.0f, (float)height, 0.0f, 0.0f, 0.0f,
                (float)width, (float)height, 0.0f, 1.0f, 0.0f,
                (float)width, 0.0f, 0.0f, 1.0f, 1.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        };

        // Delete old VBO
        if(mVBO.count(inputNr) > 0)
            glDeleteBuffers(1, &mVBO[inputNr]);
        // Create VBO
        uint VBO;
        glGenBuffers(1, &VBO);
        mVBO[inputNr] = VBO;
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        // Delete old EBO
        if(mEBO.count(inputNr) > 0)
            glDeleteBuffers(1, &mEBO[inputNr]);
        // Create EBO
        uint EBO;
        glGenBuffers(1, &EBO);
        mEBO[inputNr] = EBO;
        uint indices[] = {  // note that we start from 0!
                0, 1, 3,   // first triangle
                1, 2, 3    // second triangle
        };
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glBindVertexArray(0);

        mTexturesToRender[inputNr] = textureID;
        mImageUsed[inputNr] = input;
        mDataTimestamp[inputNr] = input->getTimestamp();
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    activateShader();
    for(auto it : mImageUsed) {
        const uint inputNr = it.first;


        Affine3f transform = Affine3f::Identity();
        // If rendering is in 2D mode we skip any transformations
        if(!mode2D) {
            transform = SceneGraph::getEigenTransformFromData(it.second);
        }

        auto spacing = it.second->getSpacing();
        transform.scale(Vector3f(spacing.x(), spacing.x(), 1.0f)/mScales[inputNr]);

        // Get width and height of texture
        glBindTexture(GL_TEXTURE_2D, mTexturesToRender[inputNr]);
        uint transformLoc = glGetUniformLocation(getShaderProgram(), "transform");
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, transform.data());
        transformLoc = glGetUniformLocation(getShaderProgram(), "perspectiveTransform");
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, perspectiveMatrix.data());
        transformLoc = glGetUniformLocation(getShaderProgram(), "viewTransform");
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, viewingMatrix.data());
        transformLoc = glGetUniformLocation(getShaderProgram(), "positionType");

        // Actual drawing
        glBindVertexArray(mVAO[inputNr]);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(0);
    }
    glDisable(GL_BLEND);
    deactivateShader();
    glFinish(); // Fixes random crashes in OpenGL on NVIDIA windows due to some interaction with the line renderer. Suboptimal solution as glFinish is a blocking sync operation.
}

void SegmentationLabelRenderer::setLabelName(uint label, std::string name) {
    m_labelNames[label] = name;
    setModified(true);
}

void SegmentationLabelRenderer::setLabelNames(std::map<uint, std::string> labelNames) {
    m_labelNames = labelNames;
    setModified(true);
}



} // end namespace fast


