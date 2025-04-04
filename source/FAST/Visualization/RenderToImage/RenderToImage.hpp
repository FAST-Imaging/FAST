#pragma once

#include <FAST/ProcessObject.hpp>
#include <FAST/Visualization/Renderer.hpp>
#include <FAST/Data/Color.hpp>

class QOpenGLContext;

namespace fast {

/**
 * @brief Render to an image
 *
 * Use this process object to render to an Image object instead of a window on screen.
 * Do this by connecting renderers to this object in order.
 *
 * @ingroup renderers
 */
class FAST_EXPORT RenderToImage : public ProcessObject, protected QOpenGLFunctions_3_3_Core {
    FAST_OBJECT(RenderToImage)
    public:
        /**
         * @brief Create instance
         * @param bgcolor Background color of scene
         * @param width Width of image to render to. If negative, the width will be determined using height and aspect ratio of scene.
         * @param height Height of image to render to. If negative, the height will be determined using width and aspect ratio of scene.
         * @return
         */
        FAST_CONSTRUCTOR(RenderToImage,
                     Color, bgcolor, = Color::White(),
                     int, width, = 1024,
                     int, height, = -1);
        void addRenderer(std::shared_ptr<Renderer> renderer);
        std::vector<std::shared_ptr<Renderer>> getRenderers();
        std::shared_ptr<RenderToImage> connect(std::shared_ptr<Renderer> renderer);
        std::shared_ptr<RenderToImage> connect(std::vector<std::shared_ptr<Renderer>> renderers);
        void reset();
        void removeAllRenderers();
        void set2DMode();
        void set3DMode();
        void setAutoUpdateCamera(bool autoUpdate);
        ~RenderToImage();
    private:
        void execute() override;
    private:
        uint m_FBO = 0;
        uint m_textureColor = 0;
        uint m_textureDepth = 0;
        uint m_renderBuffer = 0;
        std::vector<Renderer::pointer> mNonVolumeRenderers;
        std::vector<Renderer::pointer> mVolumeRenderers;

        // Camera
        Affine3f m3DViewingTransformation;
        Vector3f mRotationPoint;
        Vector3f mCameraPosition;
        bool mCameraSet;

        Matrix4f mPerspectiveMatrix;

        unsigned int mFramerate;

        Color mBackgroundColor;
        float mCentroidZ;

        float m_zoom = 1.0f;
        float zNear, zFar;
        float fieldOfViewX, fieldOfViewY;
        float aspect;
        bool mIsIn2DMode;
        bool mAutoUpdateCamera;
        Vector3f mBBMin, mBBMax;
        int m_width, m_height;

        float mLeft, mRight, mBottom, mTop; // Used for ortho projection

        QOpenGLContext* m_context;
    protected:
        void recalculateCamera();
        void getMinMaxFromBoundingBoxes(bool transform, Vector3f& min, Vector3f& max);
        void initializeGL();
        void paintGL();

        std::mutex m_mutex;
        std::atomic_bool m_initialized = false;
        int m_executeToken = 0;

};

}