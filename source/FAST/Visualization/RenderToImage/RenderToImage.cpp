#include "RenderToImage.hpp"
#include <FAST/Data/Camera.hpp>
#include <FAST/Visualization/VolumeRenderer/VolumeRenderer.hpp>
#include <FAST/Visualization/Window.hpp>
#include <FAST/Data/Image.hpp>
#include <QOpenGLContext>
#include <FAST/Algorithms/ImageFlipper/ImageFlipper.hpp>
#include <FAST/Algorithms/ImageChannelConverter/ImageChannelConverter.hpp>

namespace fast {

RenderToImage::RenderToImage(Color bgcolor, int width, int height) {
    mBackgroundColor = bgcolor;
    m_width = width;
    m_height = height;
    zNear = 0.1;
    zFar = 1000;
    fieldOfViewY = 45;
    m_zoom = 1.0;
    mIsIn2DMode = true;
    mCameraSet = false;
    mAutoUpdateCamera = false;
    createOutputPort(0);

    if(QThread::currentThread() == QApplication::instance()->thread()) { // Is main thread?
        // Main thread..
        auto context = new QOpenGLContext();
        context->setShareContext(fast::Window::getSecondaryGLContext());
        context->create();
        if(!context->isValid() || !context->areSharing(context, fast::Window::getSecondaryGLContext())) {
            throw Exception("The custom Qt GL context in fast::View is invalid!");
        }
        m_context = context;
    } else {
        // Computation thread
        auto context = new QOpenGLContext();
        context->setShareContext(fast::Window::getMainGLContext());
        context->create();
        if(!context->isValid() || !context->areSharing(context, fast::Window::getMainGLContext())) {
            throw Exception("The custom Qt GL context in fast::View is invalid!");
        }
        m_context = context;
    }
    m_context->makeCurrent(Window::getQSurface());
    initializeOpenGLFunctions();
}


std::shared_ptr<RenderToImage> RenderToImage::connect(std::shared_ptr<Renderer> renderer) {
    addRenderer(renderer);
    return std::dynamic_pointer_cast<RenderToImage>(mPtr.lock());
}
std::shared_ptr<RenderToImage> RenderToImage::connect(std::vector<std::shared_ptr<Renderer>> renderers) {
    for(auto renderer : renderers)
        addRenderer(renderer);

    return std::dynamic_pointer_cast<RenderToImage>(mPtr.lock());
}

void RenderToImage::addRenderer(Renderer::pointer renderer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    //renderer->setView(this); // TODO
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
    setModified(true);
}


std::vector<Renderer::pointer> RenderToImage::getRenderers() {
    std::vector<Renderer::pointer> newList = mNonVolumeRenderers;
    newList.insert(newList.cend(), mVolumeRenderers.begin(), mVolumeRenderers.end());
    return newList;
}

void RenderToImage::execute() {
    m_context->makeCurrent(Window::getQSurface());
    bool doContinue = true;
    for(auto renderer : getRenderers()) {
        renderer->update(m_executeToken);
        if(renderer->hasReceivedLastFrameFlag())
            doContinue = false;
    }
    ++m_executeToken;
    if(doContinue)
        setModified(true); // TODO hack in order to keep running

    // If first run: Initialize..
    if(!m_initialized) {
        recalculateCamera();
        // Create framebuffer to render to
        glGenFramebuffers(1, &m_FBO);
        glGenRenderbuffers(1, &m_renderBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, m_renderBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, m_width, m_height);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_renderBuffer);
        if(!mNonVolumeRenderers.empty() && !mVolumeRenderers.empty()) {
            glGenTextures(1, &m_textureColor);
            glGenTextures(1, &m_textureDepth);
            glBindTexture(GL_TEXTURE_2D, m_textureColor);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT, NULL);
            glBindTexture(GL_TEXTURE_2D, m_textureDepth);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

            // Assign textures to FBO
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textureColor, 0);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_textureDepth, 0);
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        initializeGL();
    }

    //std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    // Do drawing
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);
    paintGL();

    // Get framebuffer as image
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    auto data = make_uninitialized_unique<uchar[]>(m_width*m_height*4);
    glReadPixels(0,
                 0,
                 m_width,
                 m_height,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 data.get());
    auto image = Image::create(m_width, m_height, TYPE_UINT8, 4, std::move(data));
    auto convert = ImageChannelConverter::create({3})->connect(image); // Remove alpha channel
    image = ImageFlipper::create(false, true)->connect(convert)
            ->runAndGetOutputData<Image>();
    if(!doContinue)
        image->setLastFrame("RenderToImage");
    //std::chrono::duration<float, std::milli> duration = std::chrono::high_resolution_clock::now() - start;
    //std::cout << "Runtime grab frame: " << duration.count() << std::endl;
    addOutputData(0, image);
}

void RenderToImage::getMinMaxFromBoundingBoxes(bool transform, Vector3f &min, Vector3f &max) {
    std::vector<Renderer::pointer> renderers = mNonVolumeRenderers;
    renderers.insert(renderers.end(), mVolumeRenderers.begin(), mVolumeRenderers.end());
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

void RenderToImage::recalculateCamera() {
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


        float aspect = (max[yDirection] - min[yDirection])/(max[xDirection] - min[xDirection]);
        if(m_width <= 0 && m_height <= 0)
            throw Exception("Width and height in RenderToImage can't both be below 0");
        if(m_height <= 0) {
            m_height = round(m_width*(aspect));
        }
        if(m_width <= 0) {
            m_width = round(m_height*(1.0f/aspect));
        }
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
        aspect = (float)m_width / m_height;
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
                        //m3DRenderToImageingTransformation.pretranslate(-mRotationPoint); // Move to rotation point
                        //m3DRenderToImageingTransformation.prerotate(Q.toRotationMatrix()); // Rotate
                        //m3DRenderToImageingTransformation.pretranslate(mRotationPoint); // Move back from rotation point
                        m3DViewingTransformation.scale(Vector3f(1, -1, 1));
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

        //float dataAspect = (max[xDirection] - min[xDirection])/(max[yDirection] - min[yDirection]);
        if(m_width < 0 && m_height < 0)
            throw Exception("Width and height in RenderToImage can't both be below 0");
        // Not sure how to do this properly. So just copy missing
        if(m_height < 0) {
            m_height = m_width;
        }
        if(m_width < 0) {
            m_width = m_height;
        }
        aspect = (float)m_width / m_height;
        fieldOfViewX = aspect * fieldOfViewY;

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

/*
void RenderToImage::reinitialize() {
    m_initialized = false;
    initializeGL();
}*/

void RenderToImage::initializeGL() {
    if(m_initialized)
        return;
    m_initialized = true;
    for(auto renderer : getRenderers())
        renderer->initializeOpenGLFunctions();
    //QGLFunctions *fun = Window::getMainGLContext()->functions();
    initializeOpenGLFunctions();
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_TEXTURE_2D);
    if(mIsIn2DMode) {
        glDisable(GL_DEPTH_TEST);
        recalculateCamera();
    } else {
        glEnable(GL_DEPTH_TEST);

        // 3D mode
        if(!mCameraSet && getNrOfInputConnections() == 0) {
            // If camera is not set explicitly by user, FAST has to calculate it
            recalculateCamera();
        } else {
            aspect = (float)m_width / m_height;
            fieldOfViewX = aspect * fieldOfViewY;
        }
        mPerspectiveMatrix = loadPerspectiveMatrix(fieldOfViewY, aspect, zNear, zFar);
    }

    reportInfo() << "Finished initializing OpenGL" << Reporter::end();

}


void RenderToImage::paintGL() {

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
            for(auto& renderer : mNonVolumeRenderers) {
                if(!renderer->isDisabled()) {
                    renderer->draw(mPerspectiveMatrix, m3DViewingTransformation.matrix(), zNear, zFar, true, m_width, m_height);
                }
                renderer->postDraw();
            }
            mRuntimeManager->stopRegularTimer("draw2D");

        } else {
            if(getNrOfInputConnections() > 0) {
                // Has camera input connection, get camera
                Camera::pointer camera = getInputData<Camera>(0);
                CameraAccess::pointer access = camera->getAccess(ACCESS_READ);
                mRotationPoint = access->getCameraTransformation() * access->getTargetPosition();
            }

            mRuntimeManager->startRegularTimer("draw");
            for(auto& renderer : mNonVolumeRenderers) {
                if(!renderer->isDisabled()) {
                    renderer->draw(mPerspectiveMatrix, m3DViewingTransformation.matrix(), zNear, zFar, false, m_width, m_height);
                }
                renderer->postDraw();
            }

            if(!mVolumeRenderers.empty()) {
                for(auto& renderer : mVolumeRenderers) {
                    if(!renderer->isDisabled()) {
                        renderer->draw(mPerspectiveMatrix, m3DViewingTransformation.matrix(), zNear, zFar, false, m_width, m_height);
                    }
                    renderer->postDraw();
                }

                // Blit/copy the framebuffer to the default framebuffer (window)
                /*
                glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height,
                                  GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);
                 */
            }

            mRuntimeManager->stopRegularTimer("draw");
        }

        glFinish();
        mRuntimeManager->stopRegularTimer("paint");
}

void RenderToImage::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    mNonVolumeRenderers.clear();
    mVolumeRenderers.clear();
    // Cleanup GL resources
    glDeleteFramebuffers(1, &m_FBO);
    glDeleteTextures(1, &m_textureColor);
    glDeleteTextures(1, &m_textureDepth);
    glDeleteRenderbuffers(1, &m_renderBuffer);
    m_initialized = false;
}

void RenderToImage::removeAllRenderers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    mNonVolumeRenderers.clear();
    mVolumeRenderers.clear();
}

void RenderToImage::set2DMode() {
    mIsIn2DMode = true;
}

void RenderToImage::set3DMode() {
    mIsIn2DMode = false;
}

void RenderToImage::setAutoUpdateCamera(bool autoUpdate) {
    mAutoUpdateCamera = autoUpdate;
}

RenderToImage::~RenderToImage() {
    reset();
}

}
