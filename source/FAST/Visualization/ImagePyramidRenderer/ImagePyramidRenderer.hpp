#pragma once

#include <FAST/Visualization/Renderer.hpp>
#include <deque>
#include <thread>

namespace fast {

class ImagePyramid;
class ImageSharpening;

/**
 * @brief Renders tiled image pyramids
 *
 * @ingroup renderer wsi
 */
class FAST_EXPORT ImagePyramidRenderer : public Renderer {
    FAST_PROCESS_OBJECT(ImagePyramidRenderer)
    public:
        FAST_CONSTRUCTOR(ImagePyramidRenderer, bool, sharpening, = false)
        void setSharpening(bool sharpening);
        bool getSharpening() const;
        void loadAttributes() override;
        ~ImagePyramidRenderer();
        void clearPyramid();
    private:
        void
        draw(Matrix4f perspectiveMatrix, Matrix4f viewingMatrix, float zNear, float zFar, bool mode2D, int viewWidth,
             int viewHeight);

        std::unordered_map<std::string, uint> mTexturesToRender;
        std::unordered_map<std::string, GLsync> m_textureFences;
        std::unordered_map<uint, std::shared_ptr<ImagePyramid>> mImageUsed;
        std::unordered_map<std::string, uint> mVAO;
        std::unordered_map<std::string, uint> mVBO;
        std::unordered_map<std::string, uint> mEBO;

        // Queue of tiles to be loaded
        std::list<std::string> m_tileQueue; // LIFO queue
        // Buffer to process queue
        std::unique_ptr<std::thread> m_bufferThread;
        // Condition variable to wait if queue is empty
        std::condition_variable m_queueEmptyCondition;
        std::mutex m_tileQueueMutex;
        bool m_stop = false;
        std::unordered_set<std::string> m_loaded;

        int m_currentLevel = -1;

        bool m_postProcessingSharpening = false;
        std::shared_ptr<ImageSharpening> m_sharpening;

        std::shared_ptr<ImagePyramid> m_input;

        void drawTextures(Matrix4f &perspectiveMatrix, Matrix4f &viewingMatrix, bool mode2D);
        int loadTileTexture(std::string tileID);
};

}