#define _USE_MATH_DEFINES

#include "View.hpp"
#include "FAST/Data/Camera.hpp"
#include "FAST/Exception.hpp"
#include "FAST/DeviceManager.hpp"
#include "SimpleWindow.hpp"
#include "FAST/Utility.hpp"
#include <QGLFunctions>
#include <algorithm>
#include <QCursor>
#include <QApplication>
#include <FAST/Visualization/VolumeRenderer/VolumeRenderer.hpp>
#include <FAST/Visualization/TextRenderer/TextRenderer.hpp>
#include <FAST/Data/Text.hpp>
#include <FAST/Visualization/LineRenderer/LineRenderer.hpp>
#include <FAST/Data/Mesh.hpp>

namespace fast {

void View::addRenderer(Renderer::pointer renderer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    renderer->setView(this);
    if(renderer->is2DOnly())
        mIsIn2DMode = true;
    if(renderer->is3DOnly())
        mIsIn2DMode = false;
    // Can renderer be casted to volume renderer test:
    auto test = std::dynamic_pointer_cast<VolumeRenderer>(renderer);
    bool thisIsAVolumeRenderer = (bool)test;

    if(thisIsAVolumeRenderer) {
        mVolumeRenderers.push_back(renderer);
    } else {
        mNonVolumeRenderers.push_back(renderer);
    }
}

void View::removeRenderer(Renderer::pointer rendererToRemove) {
    std::lock_guard<std::mutex> lock(m_mutex);
	mVolumeRenderers.erase(std::remove(mVolumeRenderers.begin(), mVolumeRenderers.end(), rendererToRemove), mVolumeRenderers.end());
    mNonVolumeRenderers.erase(std::remove(mNonVolumeRenderers.begin(), mNonVolumeRenderers.end(), rendererToRemove), mNonVolumeRenderers.end());
}

void View::removeAllRenderers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    mVolumeRenderers.clear();
    mNonVolumeRenderers.clear();
}

void View::setBackgroundColor(Color color) {
    mBackgroundColor = color;
}

QGLFormat View::getGLFormat() {
    QGLFormat qglFormat = QGLFormat::defaultFormat();
    qglFormat.setVersion(3, 3);
    qglFormat.setProfile(QGLFormat::CoreProfile);
    return qglFormat;
}

View::View() {
    createInputPort<Camera>(0, false);

    m_zoom = 1.0f;

    createBooleanAttribute("2Dmode", "2D mode", "Switch the view mode between 3D and 2D", false);
    createStringAttribute("background-color", "Background color", "Set the background color of the view", "white");
    createFloatAttribute("zoom", "Zoom level", "Zoom level", m_zoom);

    mBackgroundColor = Color::White();
    zNear = 0.1;
    zFar = 1000;
    fieldOfViewY = 45;
    mIsIn2DMode = false;
    mLeftMouseButtonIsPressed = false;
    mRightButtonIsPressed = false;
    mQuit = false;
    mCameraSet = false;
    mAutoUpdateCamera = false;

    mFramerate = 60;
    // Set up a timer that will call update on this object at a regular interval
    timer = new QTimer(this);
    timer->start(1000 / mFramerate); // in milliseconds
    timer->setSingleShot(false);
    QObject::connect(timer, SIGNAL(timeout()), this, SLOT(updateGL()));

    m_textRenderer = TextRenderer::create(42, Color::Black(), TextRenderer::STYLE_NORMAL, TextRenderer::POSITION_BOTTOM_LEFT);
    m_lineRenderer = LineRenderer::create();
    if(QThread::currentThread() != QApplication::instance()->thread()) {
        throw Exception("FAST View must be created in the main thread");
    }
    QGLContext *context = new QGLContext(getGLFormat(), this);
    context->create(fast::Window::getSecondaryGLContext());
    this->setContext(context);
    if(!context->isValid() || !context->isSharing()) {
        throw Exception("The custom Qt GL context in fast::View is invalid!");
    }
}

void View::loadAttributes() {
    if(getBooleanAttribute("2Dmode")) {
        set2DMode();
    } else {
        set3DMode();
    }
    setBackgroundColor(Color::fromString(getStringAttribute("background-color")));
    setZoom(getFloatAttribute("zoom"));
}

void View::setCameraInputConnection(DataChannel::pointer port) {
    setInputConnection(0, port);
}

void
View::setLookAt(Vector3f cameraPosition, Vector3f targetPosition, Vector3f cameraUpVector, float z_near, float z_far) {
    mCameraPosition = cameraPosition;
    mRotationPoint = targetPosition;
    // Equations based on gluLookAt https://www.opengl.org/sdk/docs/man2/xhtml/gluLookAt.xml
    Vector3f F = targetPosition - cameraPosition;
    F.normalize();
    Vector3f up = cameraUpVector;
    up.normalize();
    Vector3f s = F.cross(up);
    Vector3f sNormalized = s;
    sNormalized.normalize();
    Vector3f u = sNormalized.cross(F);

    Matrix3f M;
    // First row
    M(0, 0) = s[0];
    M(0, 1) = s[1];
    M(0, 2) = s[2];
    // Second row
    M(1, 0) = u[0];
    M(1, 1) = u[1];
    M(1, 2) = u[2];
    // Third row
    M(2, 0) = -F[0];
    M(2, 1) = -F[1];
    M(2, 2) = -F[2];

    // Must calculate this somehow
    zNear = z_near;
    zFar = z_far;

    m3DViewingTransformation = Affine3f::Identity();
    m3DViewingTransformation.rotate(M);
    m3DViewingTransformation.translate(-mCameraPosition);

    mCameraSet = true;
}

void View::quit() {
    mQuit = true;
}

bool View::hasQuit() const {
    return mQuit;
}

View::~View() {
    reportInfo() << "DESTROYING view object" << Reporter::end();
    if(m_FBO != 0) {
        glDeleteFramebuffers(1, &m_FBO);
        glDeleteTextures(1, &m_textureColor);
        glDeleteTextures(1, &m_textureDepth);
    }
    quit();
}


void View::setMaximumFramerate(unsigned int framerate) {
    if(framerate == 0)
        throw Exception("Framerate cannot be 0.");

    mFramerate = framerate;
    timer->stop();
    timer->start(1000 / mFramerate); // in milliseconds
    timer->setSingleShot(false);
}

void View::execute() {
}

void View::updateRenderersInput(int executeToken) {
    for(auto renderer : getRenderers()) {
        for(int i = 0; i < renderer->getNrOfInputConnections(); ++i) {
            renderer->getInputPort(i)->getProcessObject()->update(executeToken);
        }
    }
}

void View::updateRenderers(int executeToken) {
    for(auto renderer : getRenderers()) {
        renderer->update(executeToken);
    }
}

void View::stopRenderers() {
    for(auto renderer : getRenderers()) {
        renderer->stopPipeline();
    }
}

void View::getMinMaxFromBoundingBoxes(bool transform, Vector3f &min, Vector3f &max) {
    std::vector<Renderer::pointer> renderers = getRenderers();
    // Get bounding boxes of all objects
    bool initialized = false;
    for(int i = 0; i < renderers.size(); i++) {
        // Apply transformation to all b boxes
        // Get max and min of x and y coordinates of the transformed b boxes
        try {
            DataBoundingBox box = renderers.at(i)->getBoundingBox(transform);
            MatrixXf corners = box.getCorners();
            if(!initialized) {
                Vector3f corner = box.getCorners().row(0);
                min[0] = corner[0];
                max[0] = corner[0];
                min[1] = corner[1];
                max[1] = corner[1];
                min[2] = corner[2];
                max[2] = corner[2];
                initialized = true;
            }

            //reportInfo() << box << Reporter::end();
            for(int j = 0; j < 8; j++) {
                for(uint k = 0; k < 3; k++) {
                    if(corners(j, k) < min[k])
                        min[k] = corners(j, k);

                    if(corners(j, k) > max[k])
                        max[k] = corners(j, k);
                }
            }
        } catch(Exception& e) {
            // Ignore
        }
    }
}

void View::recalculateCamera() {
    reportInfo() << "Recalculating the camera of the view" << reportEnd();
    if(mIsIn2DMode) {
        // TODO Initialize 2D
        // Initialize camera
        Vector3f min, max;
        getMinMaxFromBoundingBoxes(false, min, max);
        mBBMin = min;
        mBBMax = max;
        // Calculate area of each side of the resulting bounding box
        float area[3] = {(max[0] - min[0]) * (max[1] - min[1]), // XY plane
                         (max[1] - min[1]) * (max[2] - min[2]), // YZ plane
                         (max[2] - min[2]) * (max[0] - min[0])};
        uint maxArea = 0;
        for(uint i = 1; i < 3; i++) {
            if(area[i] > area[maxArea])
                maxArea = i;
        }
        // Find rotation needed
        float angleX, angleY;
        uint xDirection;
        uint yDirection;
        uint zDirection;
        switch(maxArea) {
            case 0:
                xDirection = 0;
                yDirection = 1;
                zDirection = 2;
                angleX = 0;
                angleY = 0;
                break;
            case 1:
                // Rotate 90 degres around Y axis
                xDirection = 2;
                yDirection = 1;
                zDirection = 0;
                angleX = 0;
                angleY = 90;
                break;
            case 2:
                // Rotate 90 degres around X axis
                xDirection = 0;
                yDirection = 2;
                zDirection = 1;
                angleX = 90;
                angleY = 0;
                break;
        }
        // Max pos - half of the size
        Vector3f centroid;
        centroid[0] = max[0] - (max[0] - min[0]) * 0.5;
        centroid[1] = max[1] - (max[1] - min[1]) * 0.5;
        centroid[2] = max[2] - (max[2] - min[2]) * 0.5;

        // Rotate object if needed
        Eigen::Quaternionf Qx;
        Qx = Eigen::AngleAxisf(angleX * M_PI / 180.0f, Vector3f::UnitX());
        Eigen::Quaternionf Qy;
        Qy = Eigen::AngleAxisf(angleY * M_PI / 180.0f, Vector3f::UnitY());
        Eigen::Quaternionf Q = Qx * Qy;

        //reportInfo() << "Centroid set to: " << centroid.x() << " " << centroid.y() << " " << centroid.z() << Reporter::end();
        // Initialize rotation point to centroid of object
        mRotationPoint = centroid;
        // Calculate initiali translation of camera
        // Move centroid to z axis
        // Note: Centroid does not change after rotation
        //mCameraPosition[1] = height()*0.5 - centroid[1];
        // Calculate z distance
        mCameraPosition[2] = -centroid[2]; // first move objects to origo
        // Move objects away from camera so that we see everything
        float z_width = (max[xDirection] - min[xDirection]);
        float z_height = (max[yDirection] - min[yDirection]);
        //reportInfo() << "asd: " << z_width << " " << z_height << Reporter::end();
        float minimumTranslationToSeeEntireObject = (
                z_width < z_height ? z_height : z_width);
        float boundingBoxDepth = std::max(max[zDirection] - min[zDirection], 0.1f);
        //reportInfo() << "minimum translation to see entire object: " << minimumTranslationToSeeEntireObject  << Reporter::end();
        //reportInfo() << "half depth of bounding box " << boundingBoxDepth*0.5 << Reporter::end();
        mCameraPosition[2] += -minimumTranslationToSeeEntireObject
                              - boundingBoxDepth * 0.5; // half of the depth of the bounding box
        //reportInfo() << "Camera pos set to: " << cameraPosition.x() << " " << cameraPosition.y() << " " << cameraPosition.z() << Reporter::end();
        zFar = 10;//(minimumTranslationToSeeEntireObject + boundingBoxDepth) * 2;
        zNear = -10;//std::min(minimumTranslationToSeeEntireObject * 0.5, 0.1);
        mCameraPosition[2] = 0;
        aspect = (float) (this->width()) / this->height();
        float orthoAspect = z_width / z_height;
        float scalingWidth = 1;
        float scalingHeight = 1;
        if(aspect > orthoAspect) {
            scalingWidth = aspect / orthoAspect;
        } else {
            scalingHeight = orthoAspect / aspect;
        }
        mLeft = (min[xDirection] / m_zoom) * scalingWidth;
        mRight = (max[xDirection] / m_zoom) * scalingWidth;
        mBottom = (min[yDirection] / m_zoom) * scalingHeight;
        mTop = (max[yDirection] / m_zoom) * scalingHeight;

        mCameraPosition[0] = mLeft + (mRight - mLeft) * 0.5f - centroid[0]; // center camera
        mCameraPosition[1] = mBottom + (mTop - mBottom) * 0.5f - centroid[1]; // center camera
        mCameraPosition[1] =
                mCameraPosition[1] - 2.0f * (mBottom + (mTop - mBottom) * 0.5f); // Compensate for Y flipping

        m3DViewingTransformation = Affine3f::Identity();
        //m3DViewingTransformation.pretranslate(-mRotationPoint); // Move to rotation point
        //m3DViewingTransformation.prerotate(Q.toRotationMatrix()); // Rotate
        //m3DViewingTransformation.pretranslate(mRotationPoint); // Move back from rotation point
        m3DViewingTransformation.scale(Vector3f(1, -1, 1)); // Flip y
        m3DViewingTransformation.translate(mCameraPosition);
        /*
        std::cout << "Centroid: " << centroid.transpose() << std::endl;
        std::cout << "Camera pos: " << mCameraPosition.transpose() << std::endl;
        std::cout << "width and height: " << this->width() << " " << this->height() << std::endl;
        std::cout << zNear << " " << zFar << std::endl;
        std::cout << min[xDirection] << " " << max[xDirection] << std::endl;
        std::cout << min[yDirection] << " " << max[yDirection] << std::endl;
        std::cout << "Ortho params: " << mLeft << " " << mRight << " " << mBottom << " " << mTop << " " << scalingWidth << " " << scalingHeight << " " << zNear << " " << zFar << std::endl;
         */
        mPerspectiveMatrix = loadOrthographicMatrix(mLeft, mRight, mBottom, mTop, zNear, zFar);
    } else {
        // 3D Mode
        aspect = (float) (this->width()) / this->height();
        fieldOfViewX = aspect * fieldOfViewY;
        // Initialize camera
        // Get bounding boxes of all objects
        Vector3f min, max;
        getMinMaxFromBoundingBoxes(true, min, max);
        mBBMin = min;
        mBBMax = max;

        // Calculate area of each side of the resulting bounding box
        float area[3] = {(max[0] - min[0]) * (max[1] - min[1]), // XY plane
                         (max[1] - min[1]) * (max[2] - min[2]), // YZ plane
                         (max[2] - min[2]) * (max[0] - min[0])};
        uint maxArea = 0;
        for(uint i = 1; i < 3; i++) {
            if(area[i] > area[maxArea])
                maxArea = i;
        }
        // Find rotation needed
        float angleX, angleY;
        uint xDirection;
        uint yDirection;
        uint zDirection;
        switch(maxArea) {
            case 0:
                xDirection = 0;
                yDirection = 1;
                zDirection = 2;
                angleX = 0;
                angleY = 0;
                break;
            case 1:
                // Rotate 90 degres around Y axis
                xDirection = 2;
                yDirection = 1;
                zDirection = 0;
                angleX = 0;
                angleY = 90;
                break;
            case 2:
                // Rotate 90 degres around X axis
                xDirection = 0;
                yDirection = 2;
                zDirection = 1;
                angleX = 90;
                angleY = 0;
                break;
        }
        // Max pos - half of the size
        Vector3f centroid;
        centroid[0] = max[0] - (max[0] - min[0]) * 0.5;
        centroid[1] = max[1] - (max[1] - min[1]) * 0.5;
        centroid[2] = max[2] - (max[2] - min[2]) * 0.5;

        // Rotate object if needed
        Eigen::Quaternionf Qx;
        Qx = Eigen::AngleAxisf(angleX * M_PI / 180.0f, Vector3f::UnitX());
        Eigen::Quaternionf Qy;
        Qy = Eigen::AngleAxisf(angleY * M_PI / 180.0f, Vector3f::UnitY());
        Eigen::Quaternionf Q = Qx * Qy;

        //reportInfo() << "Centroid set to: " << centroid.x() << " " << centroid.y() << " " << centroid.z() << Reporter::end();
        // Initialize rotation point to centroid of object
        mRotationPoint = centroid;
        // Calculate initiali translation of camera
        // Move centroid to z axis
        // Note: Centroid does not change after rotation
        mCameraPosition[0] = -centroid[0];
        mCameraPosition[1] = -centroid[1];
        // Calculate z distance
        mCameraPosition[2] = -centroid[2]; // first move objects to origo
        // Move objects away from camera so that we see everything
        float z_width = (max[xDirection] - min[xDirection]) * 0.5
                        / tan(fieldOfViewX * 0.5);
        float z_height = (max[yDirection] - min[yDirection]) * 0.5
                         / tan(fieldOfViewY * 0.5);
        //reportInfo() << "asd: " << z_width << " " << z_height << Reporter::end();
        float minimumTranslationToSeeEntireObject = (z_width < z_height ? z_height : z_width) / m_zoom;
        float boundingBoxDepth = (max[zDirection] - min[zDirection]);
        //reportInfo() << "minimum translation to see entire object: " << minimumTranslationToSeeEntireObject  << Reporter::end();
        //reportInfo() << "half depth of bounding box " << boundingBoxDepth*0.5 << Reporter::end();
        mCameraPosition[2] += -minimumTranslationToSeeEntireObject
                              - boundingBoxDepth * 0.5; // half of the depth of the bounding box
        //reportInfo() << "Camera pos set to: " << cameraPosition.x() << " " << cameraPosition.y() << " " << cameraPosition.z() << Reporter::end();
        zFar = (minimumTranslationToSeeEntireObject + boundingBoxDepth) * 2;
        zNear = std::min(minimumTranslationToSeeEntireObject * 0.5, 0.1);
        reportInfo() << "set zFar to " << zFar << Reporter::end();
        reportInfo() << "set zNear to " << zNear << Reporter::end();
        m3DViewingTransformation = Affine3f::Identity();
        m3DViewingTransformation.pretranslate(-mRotationPoint); // Move to rotation point
        m3DViewingTransformation.prerotate(Q.toRotationMatrix()); // Rotate
        m3DViewingTransformation.pretranslate(mRotationPoint); // Move back from rotation point
        m3DViewingTransformation.pretranslate(mCameraPosition);
        mCentroidZ = -centroid[2];
    }
}

void View::reinitialize() {
    m_initialized = false;
    initializeGL();
}

void View::initializeGL() {
    if(m_initialized)
        return;
    m_initialized = true;
    for(auto renderer : getRenderers())
        renderer->initializeOpenGLFunctions();
    //QGLFunctions *fun = Window::getMainGLContext()->functions();
    initializeOpenGLFunctions();
    glViewport(0, 0, this->width(), this->height());
    glEnable(GL_TEXTURE_2D);
    // Update all renderes, so that getBoundingBox works
    std::vector<Renderer::pointer> renderers = getRenderers();
    try {
        for(int i = 0; i < renderers.size(); i++) {
            renderers[i]->update(0);
        }
    } catch (ThreadStopped& e) {
        // TODO What to do in this case?
        reportInfo() << "Caught ThreadStopped in View::initializeGL" << reportEnd();
        return;
    }
    if(renderers.empty())
        return;
    if(mIsIn2DMode) {
        glDisable(GL_DEPTH_TEST);
        recalculateCamera();
    } else {
        glEnable(GL_DEPTH_TEST);

        if(m_FBO == 0 && !mVolumeRenderers.empty()) {
            // Create framebuffer to render to
            glGenFramebuffers(1, &m_FBO);

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);
            // Create textures which are to be assigned to framebuffer
            glGenTextures(1, &m_textureColor);
            glGenTextures(1, &m_textureDepth);
            glBindTexture(GL_TEXTURE_2D, m_textureColor);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width(), height(), 0, GL_RGBA, GL_FLOAT, NULL);
            glBindTexture(GL_TEXTURE_2D, m_textureDepth);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width(), height(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

            // Assign textures to FBO
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textureColor, 0);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_textureDepth, 0);

            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // 3D mode
        if(!mCameraSet && getNrOfInputConnections() == 0) {
            // If camera is not set explicitly by user, FAST has to calculate it
            recalculateCamera();
        } else {
            aspect = (float) (this->width()) / this->height();
            fieldOfViewX = aspect * fieldOfViewY;
        }
        mPerspectiveMatrix = loadPerspectiveMatrix(fieldOfViewY, aspect, zNear, zFar);
    }

    reportInfo() << "Finished initializing OpenGL" << Reporter::end();

}


void View::paintGL() {

    mRuntimeManager->startRegularTimer("paint");

    if(!mIsIn2DMode && !mVolumeRenderers.empty())
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO); // draw in our custom FBO
    glClearColor(mBackgroundColor.getRedValue(), mBackgroundColor.getGreenValue(), mBackgroundColor.getBlueValue(),
                 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if(mAutoUpdateCamera) {
        // If bounding box has changed, recalculate camera
        Vector3f min = mBBMin;
        Vector3f max = mBBMax;
        getMinMaxFromBoundingBoxes(!mIsIn2DMode, min, max);
        if(mBBMin != min || mBBMax != max)
            recalculateCamera();
    }

    if(mIsIn2DMode) {
        mRuntimeManager->startRegularTimer("draw2D");
        for(auto renderer : mNonVolumeRenderers) {
            if(!renderer->isDisabled()) {
                renderer->draw(mPerspectiveMatrix, m3DViewingTransformation.matrix(), zNear, zFar, true, width(), height());
            }
            renderer->postDraw(); // Call postDraw even though it is disabled, so that it is marked as hasRendered
        }

        if(m_showScalebar)
            drawScalebar();

        mRuntimeManager->stopRegularTimer("draw2D");
    } else {
        if(getNrOfInputConnections() > 0) {
            // Has camera input connection, get camera
            Camera::pointer camera = getInputData<Camera>(0);
            CameraAccess::pointer access = camera->getAccess(ACCESS_READ);
            mRotationPoint = access->getCameraTransformation() * access->getTargetPosition();
        }

        mRuntimeManager->startRegularTimer("draw");
        for(auto renderer : mNonVolumeRenderers) {
            if(!renderer->isDisabled()) {
                renderer->draw(mPerspectiveMatrix, m3DViewingTransformation.matrix(), zNear, zFar, false, width(), height());
            }
            renderer->postDraw(); // Call postDraw even though it is disabled, so that it is marked as hasRendered
        }

        if(!mVolumeRenderers.empty()) {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);
            for(auto renderer : mVolumeRenderers) {
                if(!renderer->isDisabled()) {
                    renderer->draw(mPerspectiveMatrix, m3DViewingTransformation.matrix(), zNear, zFar, false, width(), height());
                }
                renderer->postDraw(); // Call postDraw even though it is disabled, so that it is marked as hasRendered
            }

            // Blit/copy the framebuffer to the default framebuffer (window)
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0, 0, width(), height(), 0, 0, width(), height(),
                              GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);
        }

        mRuntimeManager->stopRegularTimer("draw");
    }

    glFinish();
    mRuntimeManager->stopRegularTimer("paint");
}

void View::resizeGL(int width, int height) {
    glViewport(0, 0, width, height);

    if(mIsIn2DMode) {
        // TODO the aspect ratio of the viewport and the orhto projection (left, right, bottom, top) has to match.
        aspect = (float) width / height;
        float orthoAspect = (mRight - mLeft) / (mTop - mBottom);
        float scalingWidth = 1;
        float scalingHeight = 1;
        if(aspect > orthoAspect) {
            scalingWidth = aspect / orthoAspect;
        } else {
            scalingHeight = orthoAspect / aspect;
        }
        mPerspectiveMatrix = loadOrthographicMatrix(mLeft * scalingWidth, mRight * scalingWidth,
                                                    mBottom * scalingHeight, mTop * scalingHeight, zNear, zFar);
    } else {

        glBindTexture(GL_TEXTURE_2D, m_textureColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glBindTexture(GL_TEXTURE_2D, m_textureDepth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

        aspect = (float) width / height;
        fieldOfViewX = aspect * fieldOfViewY;
        mPerspectiveMatrix = loadPerspectiveMatrix(fieldOfViewY, aspect, zNear, zFar);
    }
}

void View::keyPressEvent(QKeyEvent *event) {
    switch(event->key()) {
        case Qt::Key_R:
            recalculateCamera();
            break;
        case Qt::Key_Left:
            if(mIsIn2DMode) {
                // Move 10% of width
                float actualMovementX = width()*0.1 * ((mRight - mLeft) / width());
                mCameraPosition[0] += actualMovementX;
                m3DViewingTransformation.pretranslate(Vector3f(actualMovementX, 0, 0));
            }
            break;
        case Qt::Key_Right:
            if(mIsIn2DMode) {
                // Move 10% of width
                float actualMovementX = width()*0.1 * ((mRight - mLeft) / width());
                mCameraPosition[0] -= actualMovementX;
                m3DViewingTransformation.pretranslate(Vector3f(-actualMovementX, 0, 0));
            }
            break;
        case Qt::Key_Down:
            if(mIsIn2DMode) {
                // Move 10% of height
                float actualMovementY = height()*0.1 * ((mRight - mLeft) / height());
                mCameraPosition[1] = actualMovementY;
                m3DViewingTransformation.pretranslate(Vector3f(0, actualMovementY, 0));
            }
            break;
        case Qt::Key_Up:
            if(mIsIn2DMode) {
                // Move 10% of height
                float actualMovementY = height()*0.1 * ((mRight - mLeft) / height());
                mCameraPosition[1] = -actualMovementY;
                m3DViewingTransformation.pretranslate(Vector3f(0, -actualMovementY, 0));
            }
            break;
    }
}

void View::mouseMoveEvent(QMouseEvent *event) {
    if(mRightButtonIsPressed) {
        const float deltaX = event->x() - previousX;
        const float deltaY = event->y() - previousY;
        float actualMovementX, actualMovementY;
        if(mIsIn2DMode) {
            actualMovementX = deltaX * ((mRight - mLeft) / width());
            actualMovementY = deltaY * ((mTop - mBottom) / height());
        } else {
            float viewportWidth =
                    std::tan((fieldOfViewX * M_PI / 180.0f) * 0.5f) * fabs(-mCameraPosition.z() + mCentroidZ) * 2.0f;
            float viewportHeight =
                    std::tan((fieldOfViewY * M_PI / 180.0f) * 0.5f) * fabs(-mCameraPosition.z() + mCentroidZ) * 2.0f;
            actualMovementX = deltaX * viewportWidth / width();
            actualMovementY = deltaY * viewportHeight / height();
        }
        mCameraPosition[0] += actualMovementX;
        mCameraPosition[1] -= actualMovementY;
        m3DViewingTransformation.pretranslate(Vector3f(actualMovementX, -actualMovementY, 0));
        previousX = event->x();
        previousY = event->y();
    } else if(mLeftMouseButtonIsPressed && !mIsIn2DMode) {
        // 3D rotation
        int cx = width() / 2;
        int cy = height() / 2;

        if(event->x() == cx && event->y() == cy) { //The if cursor is in the middle
            return;
        }

        int diffx = event->x() - cx; //check the difference between the current x and the last x position
        int diffy = event->y() - cy; //check the difference between the current y and the last y position
        QCursor::setPos(mapToGlobal(QPoint(cx, cy)));
        Eigen::Quaternionf Qx;
        float sensitivity = 0.01;
        Qx = Eigen::AngleAxisf(sensitivity * diffx, Vector3f::UnitY());
        Eigen::Quaternionf Qy;
        Qy = Eigen::AngleAxisf(sensitivity * diffy, Vector3f::UnitX());
        Eigen::Quaternionf Q = Qx * Qy;
        Vector3f newRotationPoint = m3DViewingTransformation * mRotationPoint; // Move rotation point to new position
        m3DViewingTransformation.pretranslate(-newRotationPoint); // Move to rotation point
        m3DViewingTransformation.prerotate(Q.toRotationMatrix()); // Rotate
        m3DViewingTransformation.pretranslate(newRotationPoint); // Move back
    }
}

void View::mousePressEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton && !mIsIn2DMode) {
        mLeftMouseButtonIsPressed = true;
        // Move cursor to center of window
        int cx = width() / 2;
        int cy = height() / 2;
        QCursor::setPos(mapToGlobal(QPoint(cx, cy)));
    } else if(event->button() == Qt::RightButton) {
        previousX = event->x();
        previousY = event->y();
        mRightButtonIsPressed = true;
    }
}

void View::wheelEvent(QWheelEvent *event) {
    if(mIsIn2DMode) {
        // the aspect ratio of the viewport and the orhto projection (left, right, bottom, top) has to match.
        aspect = (float) width() / height();
        float orthoAspect = (mRight - mLeft) / (mTop - mBottom);
        float scalingWidth = 1;
        float scalingHeight = 1;
        if(aspect > orthoAspect) {
            scalingWidth = aspect / orthoAspect;
        } else {
            scalingHeight = orthoAspect / aspect;
        }
		float targetSizeX = (mRight - mLeft) * 0.33f; // should end up with a fraction of the size
		float targetSizeY = (mTop - mBottom) * 0.33f; // should end up with fraction of the size
		float currentPosX = (event->position().x()/width())*(mRight - mLeft) + mLeft;
        float currentPosY = (event->position().y()/height())*(mTop - mBottom) + mBottom;
        // First: Zoom towards center
        if(event->delta() > 0) {
            mLeft = mLeft + targetSizeX*0.5f;
            mRight = mRight - targetSizeX*0.5f;
            mBottom = mBottom + targetSizeY * 0.5f;
            mTop = mTop - targetSizeY*0.5f;
        } else if(event->delta() < 0) {
            mLeft = mLeft - targetSizeX * 0.5f;;
            mRight = mRight + targetSizeX*0.5f;
            mBottom = mBottom - targetSizeY*0.5f;
            mTop = mTop + targetSizeY*0.5f;
        }
        // Now: Keep pointer at same position while zooming in and out:
        float newPosX = (event->position().x()/width())*(mRight - mLeft) + mLeft;
        float newPosY = (event->position().y()/height())*(mTop - mBottom) + mBottom;
        float diffX = newPosX - currentPosX;
        float diffY = newPosY - currentPosY;
        mLeft -= diffX;
        mRight -= diffX;
        mBottom += diffY;
        mTop += diffY;
        mPerspectiveMatrix = loadOrthographicMatrix(mLeft * scalingWidth, mRight * scalingWidth,
                                                    mBottom * scalingHeight, mTop * scalingHeight, zNear, zFar);
    } else {
        if(event->delta() > 0) {
            mCameraPosition[2] += (zFar - zNear) * 0.05f;
            m3DViewingTransformation.pretranslate(Vector3f(0, 0, (zFar - zNear) * 0.05f));
        } else if(event->delta() < 0) {
            mCameraPosition[2] += -(zFar - zNear) * 0.05f;
            m3DViewingTransformation.pretranslate(Vector3f(0, 0, -(zFar - zNear) * 0.05f));
        }
    }
}

void View::mouseReleaseEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        mLeftMouseButtonIsPressed = false;
    } else if(event->button() == Qt::RightButton) {
        mRightButtonIsPressed = false;
    }
}

void View::set2DMode() {
    mIsIn2DMode = true;
}

void View::set3DMode() {
    mIsIn2DMode = false;
}

std::vector<Renderer::pointer> View::getRenderers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Renderer::pointer> newList = mNonVolumeRenderers;
    newList.insert(newList.cend(), mVolumeRenderers.begin(), mVolumeRenderers.end());
    return newList;
}

void View::resetRenderers() {
    for(auto renderer : getRenderers()) {
        renderer->reset();
    }
}

Vector4f View::getOrthoProjectionParameters() {
    return Vector4f(mLeft, mRight, mBottom, mTop);
}

void View::setAutoUpdateCamera(bool autoUpdate) {
    mAutoUpdateCamera = autoUpdate;
}

Matrix4f View::getViewMatrix() const {
    return m3DViewingTransformation.matrix();
}

Matrix4f View::getPerspectiveMatrix() const {
    return mPerspectiveMatrix;
}

void View::setZoom(float zoom) {
    if(zoom < 0.0f)
        throw Exception("Zoom level must be larger than 0");
    m_zoom = zoom; // This value will be used on startup/initialization of camera
    // If view is running we should also change current values:
    if(mIsIn2DMode) {
        mLeft = mLeft / zoom;
        mRight = mRight / zoom;
        mTop = mTop / zoom;
        mBottom = mBottom / zoom;
        mPerspectiveMatrix = loadOrthographicMatrix(mLeft, mRight, mBottom, mTop, zNear, zFar);
    } else {
        float diff = mCameraPosition[2] - mCameraPosition[2] / zoom;
        mCameraPosition[2] = mCameraPosition[2] / zoom;
        m3DViewingTransformation.pretranslate(Vector3f(0, 0, diff));
    }
}

bool View::eventFilter(QObject *object, QEvent *event) {
    if(event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        keyPressEvent(keyEvent);
    } else if(event->type() == QEvent::WindowStateChange) {
        changeEvent(event);
    }
    return false;
}

void View::changeEvent(QEvent *event) {
}

void View::drawScalebar() {
    Vector4f bottom_left = (mPerspectiveMatrix*m3DViewingTransformation).inverse()*Vector4f(-1,-1,0,1);
    Vector4f top_right = (mPerspectiveMatrix*m3DViewingTransformation).inverse()*Vector4f(1,1,0,1);
    const float physicalWidth = top_right.x() - bottom_left.x();
    const float physicalHeight = std::fabs(top_right.y() - bottom_left.y());
    float barWidth;
    int decimals = 0;
    if(physicalWidth*1000.0f/10.0f > 1) {
        int digits = roundToString(physicalWidth*1000/10.0f).size();
        barWidth = std::floor((physicalWidth*1000/10.0f)/(std::pow(10, digits-1)))*std::pow(10, digits-1);
    } else {
        // Count number of digits until non-zero
        float current = physicalWidth*1000/10.0f;
        int counter = 0;
        while(current < 1) {
            current *= 10;
            ++counter;
        }
        barWidth = std::floor((physicalWidth*1000/10.0f)*(std::pow(10, counter)))/std::pow(10, counter);
        decimals = counter;
    }
    bottom_left = (mPerspectiveMatrix*m3DViewingTransformation).inverse()*Vector4f(-1,-1,0,1);
    //bottom_left = (mPerspectiveMatrix*m3DViewingTransformation).inverse()*Vector4f(-0.98,-0.92,0,1);
    Vector4f right = bottom_left;
    right.x() += barWidth/1000.0f;

    // Draw line
    auto mesh = Mesh::create({MeshVertex(bottom_left.head(3)), MeshVertex(right.head(3))}, {MeshLine(0, 1)});
    m_lineRenderer->setDefaultLineWidth(1.5);
    m_lineRenderer->setDrawJoints(false);
    m_lineRenderer->connect(mesh);
    m_lineRenderer->run();
    m_lineRenderer->setView(this);
    m_lineRenderer->draw(mPerspectiveMatrix, m3DViewingTransformation.matrix(), zNear, zFar, true, width(), height());
    m_lineRenderer->postDraw();

    // Draw text
    auto text = Text::create(
            roundToString(barWidth, decimals) + " μm", //+ ": " + roundToString(physicalWidth*1000, 1) + " x " + roundToString(physicalHeight*1000, 1) + " micrometers",
            Color::Black()
            );
    m_textRenderer->connect(text);
    m_textRenderer->run();
    m_textRenderer->setView(this);
    m_textRenderer->draw(mPerspectiveMatrix, m3DViewingTransformation.matrix(), zNear, zFar, true, width(), height());
    m_textRenderer->postDraw();

}

void View::setScalebar(float enable) {
    m_showScalebar = enable;
}

} // end namespace fast
