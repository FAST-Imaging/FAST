#include "ImagePyramidRenderer.hpp"
#include "FAST/Exception.hpp"
#include "FAST/DeviceManager.hpp"
#include "FAST/Utility.hpp"
#include "FAST/SceneGraph.hpp"
#include <FAST/Data/ImagePyramid.hpp>
#include <QGLContext>
#include <FAST/Visualization/Window.hpp>
#include <FAST/Visualization/View.hpp>
#include <FAST/Algorithms/ImageSharpening/ImageSharpening.hpp>
#ifdef WIN32
#elif defined(__APPLE__) || defined(__MACOSX)
#include <OpenGL/OpenGL.h>
#else
#include <GL/glx.h>
#endif

namespace fast {

void ImagePyramidRenderer::clearPyramid() {
    // Clear buffer. Useful when processing a new image
    // Delete all GL data
    for(auto item : mVAO) {
        glDeleteVertexArrays(1, &item.second);
    }
    mVAO.clear();
    for(auto item : mVBO) {
        glDeleteBuffers(1, &item.second);
    }
    mVBO.clear();
    for(auto item : mEBO) {
        glDeleteBuffers(1, &item.second);
    }
    mEBO.clear();
    for(auto texture : mTexturesToRender) {
        glDeleteTextures(1, &texture.second);
    }
    mTexturesToRender.clear();
    clearDataToRender();
}

ImagePyramidRenderer::~ImagePyramidRenderer() {
    reportInfo() << "Destroying ImagePyramidRenderer in THREAD: " << std::this_thread::get_id() << reportEnd();
    {
        std::unique_lock<std::mutex> lock(m_tileQueueMutex);
        m_stop = true;
    }
    m_queueEmptyCondition.notify_one();
    if(m_bufferThread) {
        m_bufferThread->join();
        reportInfo() << "Buffer thread in ImagePyramidRenderer stopped" << reportEnd();
    }
    clearPyramid(); // Free memory
    reportInfo() << "Pyramid cleared" << reportEnd();
}

ImagePyramidRenderer::ImagePyramidRenderer(bool sharpening) : Renderer() {
    createInputPort(0, "ImagePyramid", "", false);
    m_2Donly = true;
    mIsModified = true;
    m_stop = false;
    m_currentLevel = -1;
    createBooleanAttribute("sharpening", "Sharpening", "Post processing using image sharpening", m_postProcessingSharpening);
    createShaderProgram({
                                Config::getKernelSourcePath() + "/Visualization/ImagePyramidRenderer/ImagePyramidRenderer.vert",
                                Config::getKernelSourcePath() + "/Visualization/ImagePyramidRenderer/ImagePyramidRenderer.frag",
                        });

    m_sharpening = ImageSharpening::New();
    m_sharpening->setStandardDeviation(1.5f);
    setSharpening(sharpening);
}


void ImagePyramidRenderer::loadAttributes() {
    setSharpening(getBooleanAttribute("sharpening"));
}

int ImagePyramidRenderer::loadTileTexture(std::string tileID) {
    // Check if tile has been processed before
    if(mTexturesToRender.count(tileID) > 0)
        return 0;

    //std::cout << "Loading tile " << tileID << " queue size: " << m_tileQueue.size() << std::endl;

    // Create texture
    auto parts = split(tileID, "_");
    if(parts.size() != 3)
        throw Exception("incorrect tile format");

    int level = std::stoi(parts[0]);
    int tile_x = std::stoi(parts[1]);
    int tile_y = std::stoi(parts[2]);
    //std::cout << "Creating texture for tile " << tile_x << " " << tile_y << " at level " << level << std::endl;
    Image::pointer tile;
    {
        auto access = m_input->getAccess(ACCESS_READ);
        try {
            tile = access->getPatchAsImage(level, tile_x, tile_y, false);
        } catch(Exception &e) {
            //reportWarning() << "Error occured while trying to open patch " << tile_x << " " << tile_y << reportEnd();
            // Tile was missing, just skip it..
            std::lock_guard<std::mutex> lock(m_tileQueueMutex);
            mTexturesToRender[tileID] = 0;
            return 0;
        }
        if(m_postProcessingSharpening) {
            m_sharpening->connect(tile);
            tile = m_sharpening->run()->getOutput<Image>();
        }
    }
    auto tileAccess = tile->getImageAccess(ACCESS_READ);
    // Copy data from CPU to GL texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // TODO Why is this needed:
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    if(tile->getWidth()*tile->getNrOfChannels() % 4 != 0) {
        // By default OpenGL assumes that the start of each row of an image is aligned 4 bytes.
        // Thus we have to change this here
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Fix alignment issues
    }

    // WSI data from openslide is stored as ARGB, need to handle this here: BGRA and reverse
    if(m_input->isBGRA()) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA, tile->getWidth(), tile->getHeight(), 0, GL_BGRA,
                     GL_UNSIGNED_BYTE,
                     tileAccess->get());
    } else {
        if(tile->getNrOfChannels() == 3) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB, tile->getWidth(), tile->getHeight(), 0, GL_RGB,
                         GL_UNSIGNED_BYTE,
                         tileAccess->get());
        } else if(tile->getNrOfChannels() == 4) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA, tile->getWidth(), tile->getHeight(), 0, GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         tileAccess->get());
        }
    }
    GLint compressedImageSize = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &compressedImageSize);
    glBindTexture(GL_TEXTURE_2D, 0);
    GLsync uploadFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();

    bool isEmpty;
    {
        std::lock_guard<std::mutex> lock(m_tileQueueMutex);
        mTexturesToRender[tileID] = textureID;
        m_textureFences[tileID] = uploadFence;
        isEmpty = m_tileQueue.empty();
    }
    // If queue is empty, wait for sync to finish
    if(isEmpty) {
        glClientWaitSync(uploadFence, 0, 1e9);
    }
    m_view->scheduleRedraw();
    return compressedImageSize;
}

void ImagePyramidRenderer::draw(Matrix4f perspectiveMatrix, Matrix4f viewingMatrix, float zNear, float zFar, bool mode2D,
                               int viewWidth,
                               int viewHeight) {
    auto dataToRender = getDataToRender();
    if(dataToRender.empty())
        return;

    if(!m_bufferThread && m_view != nullptr) {
        // Create thread to load patches
        // Create a GL context for the thread which is sharing with the context of the view
        auto context = new QGLContext(View::getGLFormat(), m_view);
        context->create(m_view->context());

        if(!context->isValid())
            throw Exception("The custom Qt GL context is invalid!");

        if(!context->isSharing())
            throw Exception("The custom Qt GL context is not sharing!");

        context->makeCurrent();
#ifdef WIN32
        auto nativeContextHandle = wglGetCurrentContext();
        context->doneCurrent();
        m_view->context()->makeCurrent();
        auto dc = wglGetCurrentDC();
        
        m_bufferThread = std::make_unique<std::thread>([this, dc, nativeContextHandle]() {
            wglMakeCurrent(dc, nativeContextHandle);
#elif defined(__APPLE__)
        auto nativeContextHandle = CGLGetCurrentContext();
        context->doneCurrent();
        m_view->context()->makeCurrent();

        m_bufferThread = std::make_unique<std::thread>([this, nativeContextHandle]() {
            CGLSetCurrentContext(nativeContextHandle);
#else
        auto nativeContextHandle = glXGetCurrentContext();
        auto drawable = glXGetCurrentDrawable();
        auto display = glXGetCurrentDisplay();
        context->doneCurrent();
        m_view->context()->makeCurrent();

        m_bufferThread = std::make_unique<std::thread>([this, display, drawable, nativeContextHandle]() {
            glXMakeCurrent(display, drawable, nativeContextHandle);
#endif
            uint64_t memoryUsage = 0;
            while(true) {
                std::string tileID;
                {
                    std::unique_lock<std::mutex> lock(m_tileQueueMutex);
                    // If queue is empty, we wait here
                    while(m_tileQueue.empty() && !m_stop) {
                        m_queueEmptyCondition.wait(lock);
                    }
                    if(m_stop)
                        break;

                    // Get next item on queue
                    tileID = m_tileQueue.back();
                    m_tileQueue.pop_back();
                }

                memoryUsage += loadTileTexture(tileID);
                //std::cout << "Texture cache in ImagePyramidRenderer using " << (float)memoryUsage / (1024 * 1024) << " MB" << std::endl;
            }
        });
    }

    Vector4f bottom_left = (perspectiveMatrix*viewingMatrix).inverse()*Vector4f(-1,-1,0,1);
    Vector4f top_right = (perspectiveMatrix*viewingMatrix).inverse()*Vector4f(1,1,0,1);
    float width = top_right.x() - bottom_left.x();
    float height = std::fabs(top_right.y() - bottom_left.y());
    //std::cout << "Viewing coordinates:" << bottom_left.transpose() << " " <<  top_right.transpose() << std::endl;
    //std::cout << "Current Size:" << width << " " <<  height << std::endl;
    float offset_x = bottom_left.x();
    float offset_y = top_right.y();
    //std::cout << "Offset x:" << offset_x << std::endl;
    //std::cout << "Offset y:" << offset_y << std::endl;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        m_input = std::static_pointer_cast<ImagePyramid>(dataToRender[0]);
    }

    Vector3f spacing = m_input->getSpacing();
    offset_x *= 1.0f/spacing.x();
    offset_y *= 1.0f/spacing.y();
    width *= 1.0f/spacing.x();
    height *= 1.0f/spacing.y();
    // Determine which level to use
    // If nr of pixels in viewport is larger than the current width and height of view, than increase the magnification
    int fullWidth = m_input->getFullWidth();
    int fullHeight = m_input->getFullHeight();
    int levelToUse = 0;
    int level = m_input->getNrOfLevels();
    do {
        level = level - 1;
        int levelWidth = m_input->getLevelWidth(level);
        int levelHeight = m_input->getLevelHeight(level);

        // Percentage of full WSI shown currently
        float percentageShownX = (float)width / fullWidth;
        float percentageShownY = (float)height / fullHeight;
        // With current level, do we have have enough pixels to fill the view?
        if(percentageShownX * levelWidth > viewWidth && percentageShownY * levelHeight > viewHeight) {
            // If yes, stop here
            levelToUse = level;
            break;
        } else {
            // If not, increase the magnification 
            continue;
        }
    } while(level > 0);
    if(m_currentLevel != levelToUse && m_currentLevel != -1) {
        // Level change, clear queue
        std::lock_guard<std::mutex> lock(m_tileQueueMutex);
        m_tileQueue.clear();
        //std::cout << "Queue cleared!" << std::endl;
    }
    m_currentLevel = levelToUse;

    activateShader();

    // This is the actual rendering
    Affine3f transform = Affine3f::Identity();

    transform.scale(spacing);


    uint transformLoc = glGetUniformLocation(getShaderProgram(), "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, transform.data());
    transformLoc = glGetUniformLocation(getShaderProgram(), "perspectiveTransform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, perspectiveMatrix.data());
    transformLoc = glGetUniformLocation(getShaderProgram(), "viewTransform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, viewingMatrix.data());

    for(int level = m_input->getNrOfLevels()-1; level >= levelToUse; level--) {
        const int levelWidth = m_input->getLevelWidth(level);
        const int levelHeight = m_input->getLevelHeight(level);
        const int mTilesX = m_input->getLevelTilesX(level);
        const int mTilesY = m_input->getLevelTilesY(level);
        const int tileWidth = m_input->getLevelTileWidth(level);
        const int tileHeight = m_input->getLevelTileHeight(level);
        const float mCurrentTileScale = m_input->getLevelScale(level);

        for(int tile_x = 0; tile_x < mTilesX; ++tile_x) {
            for(int tile_y = 0; tile_y < mTilesY; ++tile_y) {
                const std::string tileString =
                        std::to_string(level) + "_" + std::to_string(tile_x) + "_" + std::to_string(tile_y);

                float tile_offset_x = tile_x * tileWidth;
                float tile_offset_y = tile_y * tileHeight;

                float tile_width = tileWidth;
                if(tile_x == mTilesX - 1)
                    tile_width = levelWidth - tile_offset_x;
                float tile_height = tileHeight;
                if(tile_y == mTilesY - 1)
                    tile_height = levelHeight - tile_offset_y;

                //tile_width *= spacing.x();
                //tile_height *= spacing.y();
                //tile_offset_x *= spacing.x();
                //tile_offset_y *= spacing.y();

                // Only process visible patches
                // Fully contained and partly
                if(!(
                        // Completely inside
                        (offset_x <= tile_offset_x * mCurrentTileScale &&
                         offset_x + width > tile_offset_x * mCurrentTileScale + tile_width * mCurrentTileScale)
                        ||
                        // Partially inside on left side
                        (offset_x > tile_offset_x * mCurrentTileScale &&
                         offset_x < (tile_offset_x + tile_width) * mCurrentTileScale)
                        ||
                        // Partially inside on right side
                        (offset_x + width > tile_offset_x * mCurrentTileScale &&
                         offset_x + width <= (tile_offset_x + tile_width) * mCurrentTileScale)
                ))
                    continue;
                if(!(
                        // Completely inside
                        (offset_y <= tile_offset_y * mCurrentTileScale &&
                         offset_y + height > tile_offset_y * mCurrentTileScale + tile_height * mCurrentTileScale)
                        ||
                        // Partially inside top
                        (offset_y > tile_offset_y * mCurrentTileScale &&
                         offset_y < (tile_offset_y + tile_height) * mCurrentTileScale)
                        ||
                        // Partially inside bottom
                        (offset_y + height > tile_offset_y * mCurrentTileScale &&
                         offset_y + height <= (tile_offset_y + tile_height) * mCurrentTileScale)
                ))
                    continue;

                uint textureID;
                if(m_view != nullptr) {
                    // Is patch in cache?
                    bool textureLoaded = false;
                    {
                        std::lock_guard<std::mutex> lock(m_tileQueueMutex);
                        textureLoaded = mTexturesToRender.count(tileString) > 0;
                    }
                    if(!textureLoaded) {
                        // Add to queue if not in cache
                        {
                            std::lock_guard<std::mutex> lock(m_tileQueueMutex);
                            // Remove any duplicates first
                            m_tileQueue.remove(tileString); // O(n) time complexity..
                            m_tileQueue.push_back(tileString);
                            //std::cout << "Added tile " << tileString << " to queue" << std::endl;
                        }
                        m_queueEmptyCondition.notify_one();
                        continue;
                    } else {
                        textureID = mTexturesToRender[tileString];
                    }
                } else {
                    int bytes = loadTileTexture(tileString);
                    textureID = mTexturesToRender[tileString];
                }

                // Check whether texture is ready
                {
                    std::lock_guard<std::mutex> lock(m_tileQueueMutex);
                    if(m_textureFences.count(tileString) > 0) {
                        auto waitReturn = glClientWaitSync(m_textureFences[tileString], 0, 0);
                        if(waitReturn == GL_ALREADY_SIGNALED || waitReturn == GL_CONDITION_SATISFIED) {
                            glDeleteSync(m_textureFences[tileString]);
                            m_textureFences.erase(tileString);
                        } else {
                            continue;
                        }
                    }
                }

                if(textureID == 0) // This tile was missing or something, just skip it
                    continue;

                if(mVAO.count(tileString) == 0) {
                    // Create VAO
                    uint VAO_ID;
                    glGenVertexArrays(1, &VAO_ID);
                    mVAO[tileString] = VAO_ID;
                    glBindVertexArray(VAO_ID);

                    // Create VBO
                    // Get width and height in mm
                    //std::cout << "Creating vertices for " << tile_x << " " << tile_y << std::endl;
                    //std::cout << "Tile position: " << tile_offset_x*mCurrentTileScale << " " << tile_offset_x*mCurrentTileScale + tile_width*mCurrentTileScale << std::endl;
                    //std::cout << "Tile position: " << tile_offset_y*mCurrentTileScale << " " << tile_offset_y*mCurrentTileScale + tile_height*mCurrentTileScale << std::endl;
                    float vertices[] = {
                            // vertex: x, y, z; tex coordinates: x, y
                            tile_offset_x * mCurrentTileScale, (tile_offset_y + tile_height) * mCurrentTileScale,
                            -(float)level-1,
                            0.0f, 1.0f,
                            (tile_offset_x + tile_width) * mCurrentTileScale,
                            (tile_offset_y + tile_height) * mCurrentTileScale, -(float)level-1, 1.0f, 1.0f,
                            (tile_offset_x + tile_width) * mCurrentTileScale, tile_offset_y * mCurrentTileScale, -(float)level-1,
                            1.0f,
                            0.0f,
                            tile_offset_x * mCurrentTileScale, tile_offset_y * mCurrentTileScale, -(float)level-1, 0.0f, 0.0f,
                    };
                    uint VBO;
                    glGenBuffers(1, &VBO);
                    mVBO[tileString] = VBO;
                    glBindBuffer(GL_ARRAY_BUFFER, VBO);
                    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
                    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));
                    glEnableVertexAttribArray(0);
                    glEnableVertexAttribArray(1);

                    // Create EBO
                    uint EBO;
                    glGenBuffers(1, &EBO);
                    mEBO[tileString] = EBO;
                    uint indices[] = {  // note that we start from 0!
                            0, 1, 3,   // first triangle
                            1, 2, 3    // second triangle
                    };
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
                    glBindVertexArray(0);
                }

                glBindTexture(GL_TEXTURE_2D, textureID);
                glBindVertexArray(mVAO[tileString]);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                glBindTexture(GL_TEXTURE_2D, 0);
                glBindVertexArray(0);
            }
        }
    }
    deactivateShader();
}

void ImagePyramidRenderer::drawTextures(Matrix4f &perspectiveMatrix, Matrix4f &viewingMatrix, bool mode2D) {

}

void ImagePyramidRenderer::setSharpening(bool sharpening) {
    m_postProcessingSharpening = sharpening;
}

bool ImagePyramidRenderer::getSharpening() const {
    return m_postProcessingSharpening;
}

} // end namespace fast
