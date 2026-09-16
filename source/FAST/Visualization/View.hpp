#pragma once

#include "FAST/Data/Color.hpp"
#include "Renderer.hpp"
#include "Plane.hpp"
#include <vector>
#include <QGLWidget>
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <FAST/Visualization/LineRenderer/LineRenderer.hpp>

namespace fast {

class TextRenderer;
class ComputationThread;

class FAST_EXPORT  View : public QGLWidget, public ProcessObject, protected QOpenGLFunctions_3_3_Core {
    //FAST_OBJECT(View)
    Q_OBJECT
    public:
        void addRenderer(Renderer::pointer renderer);
    	void removeRenderer(Renderer::pointer renderer);
        void removeAllRenderers();
        void keyPressEvent(QKeyEvent* event);
        void mouseMoveEvent(QMouseEvent* event);
        void mousePressEvent(QMouseEvent* event);
        void mouseReleaseEvent(QMouseEvent* event);
        void wheelEvent(QWheelEvent* event);
        void changeEvent(QEvent* event);
        bool eventFilter(QObject* object, QEvent* event);
        void setCameraInputConnection(DataChannel::pointer port);
        void set2DMode();
        void set3DMode();
        void setLookAt(Vector3f cameraPosition, Vector3f targetPosition, Vector3f cameraUpVector, float zNear = 0.1, float zFar = 1000);
        void quit();
        void reinitialize();
        bool hasQuit() const;
        ~View();
        void recalculateCamera();
        void setBackgroundColor(Color color);
        void setAutoUpdateCamera(bool autoUpdate);
    	Vector4f getOrthoProjectionParameters();

		std::string getNameOfClass() const {
		    return "View";
		};
        View();
		std::vector<Renderer::pointer> getRenderers();
		static QGLFormat getGLFormat();
		Matrix4f getViewMatrix() const;
		Matrix4f getPerspectiveMatrix() const;
		void loadAttributes() override;
        /**
         * Function for setting the zoom level of the view. Level 1 is no zooming. 
           Level 0.5 makes images in the view half its size. Level 2 makes images in the view double in size.
         */
        void setZoom(float zoom);
        QGLWidget* asQGLWidget() { return (QGLWidget*)this; }
        /**
         * @brief Enable or disable scalebar
         * @param enable
         */
        void setScalebar(float enable);
        void scheduleRedraw(bool now = true);
    Q_SIGNALS:
        void redraw();
    private:
        uint m_FBO = 0;
        uint m_textureColor = 0;
        uint m_textureDepth = 0;
		std::vector<Renderer::pointer> mNonVolumeRenderers;
		std::vector<Renderer::pointer> mVolumeRenderers;
		GLuint programGLSL;

        // Camera
        Affine3f m3DViewingTransformation;
		Vector3f mRotationPoint;
		Vector3f mCameraPosition;
		bool mCameraSet;

		Matrix4f mPerspectiveMatrix;

        void execute();

        Color mBackgroundColor;
        
        bool mQuit;

        std::atomic_bool m_needsToRedraw;
        std::atomic_bool m_postponedRedraw;
        std::chrono::high_resolution_clock::time_point m_lastRedrawRequest;
        
        float m_zoom = 1.0f;
		float zNear, zFar;
        float fieldOfViewX, fieldOfViewY;
        float aspect;
        bool mIsIn2DMode;
        bool mAutoUpdateCamera;
		Vector3f mBBMin, mBBMax;

        bool mLeftMouseButtonIsPressed;
        bool mRightButtonIsPressed;

        int previousX, previousY;

		float mLeft, mRight, mBottom, mTop; // Used for ortho projection
		float mCentroidZ;

        friend class ComputationThread;

        // Scalebar
        std::shared_ptr<TextRenderer> m_textRenderer;
        std::shared_ptr<LineRenderer> m_lineRenderer;
        bool m_showScalebar = false;
        void drawScalebar();
    protected:
        void getMinMaxFromBoundingBoxes(bool transform, Vector3f& min, Vector3f& max);
        void initializeGL();
        void paintGL();
        void resizeGL(int width, int height);
		void updateRenderersInput(int executeToken);
		void updateRenderers(int executeToken);
		void stopRenderers();
		void resetRenderers();

		std::mutex m_mutex;
		std::atomic_bool m_initialized = false;
};

} // end namespace fast
