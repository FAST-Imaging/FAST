#pragma once

#include "FAST/ProcessObject.hpp"
#include "FAST/Data/DataBoundingBox.hpp"
#include "FAST/Data/SpatialDataObject.hpp"
#include <mutex>
#include <QOpenGLFunctions_3_3_Core>

namespace fast {

class View;
class RenderToImage;
class DataBoundingBox;

/**
 * @defgroup renderers Renderers
 * Renderers are process objects which can visualize data in a View, typically using OpenGL.
 * They should be derived from the Renderer class.
 */

/**
 * @brief Abstract base class for @ref renderers
 *
 * Renderers are process objects which can visualize data in a View, typically using OpenGL.
 * They should inherit from this class.
 *
 * @ingroup renderers
 */
class FAST_EXPORT  Renderer : public ProcessObject, protected QOpenGLFunctions_3_3_Core {
    public:
        typedef std::shared_ptr<Renderer> pointer;
        virtual void
        draw(Matrix4f perspectiveMatrix, Matrix4f viewingMatrix, float zNear, float zFar, bool mode2D, int viewWidth,
             int viewHeight) = 0;
        virtual void postDraw();
        /**
         * Adds a new input connection
         * @param port
         * @return the input nr of the new connection
         */
        virtual uint addInputConnection(DataChannel::pointer port);
        /**
         * Adds a new input connection to a specific data object
         * @param data
         * @return the input nr of the new connection
         */
        virtual uint addInputData(DataObject::pointer data);
        virtual DataBoundingBox getBoundingBox(bool transform = true);
        virtual void stopPipeline();
        virtual void reset();
        /**
         * @brief Set renderer to disabled or enabled. A disabled renderer will not draw, but it will execute.
         * @param disabled
         */
        virtual void setDisabled(bool disabled);
        /**
         * @brief Get whether this renderer is disabled or not
         * @return
         */
        virtual bool isDisabled() const;
        void setView(View* view);
        bool is2DOnly() const;
        bool is3DOnly() const;
        void loadAttributes() override;
    protected:
        Renderer();
        virtual void execute() override;
        std::unordered_map<uint, std::shared_ptr<SpatialDataObject>> getDataToRender();
        void clearDataToRender();

        /**
         * @brief Creates an OpenGL shader program. Should be used in the renderer constructor.
         * @param shaderFilenames
         * @param programName
         */
        void createShaderProgram(std::vector<std::string> shaderFilenames, std::string programName = "default");
        void attachShader(std::string filename, std::string programName = "default");
        void activateShader(std::string programName = "default");
        void deactivateShader();
        uint getShaderProgram(std::string programName = "default");
        void setShaderUniform(std::string name, Matrix4f matrix, std::string shaderProgramName = "default");
        void setShaderUniform(std::string name, Affine3f matrix, std::string shaderProgramName = "default");
        void setShaderUniform(std::string name, Vector3f vector, std::string shaderProgramName = "default");
        void setShaderUniform(std::string name, Vector4f vector, std::string shaderProgramName = "default");
        void setShaderUniform(std::string name, float value, std::string shaderProgramName = "default");
        void setShaderUniform(std::string name, bool value, std::string shaderProgramName = "default");
        void setShaderUniform(std::string name, int value, std::string shaderProgramName = "default");
        int getShaderUniformLocation(std::string name, std::string shaderProgramName = "default");

        // Locking mechanisms to ensure thread safe synchronized rendering
        bool mHasRendered = true;
        bool mStop = false;
        std::mutex mMutex;

        /**
         * @brief A disabled renderer will not draw, but it will execute
         */
        std::atomic_bool m_disabled = false;

        /**
         * Whether this renderer is only capable of 2D rendering
         */
        bool m_2Donly = false;
        /**
         * Whether this renderer is only capable of 3D rendering
         */
        bool m_3Donly = false;

        /**
         * This holds the current data to render for each input connection
         */
        std::unordered_map<uint, SpatialDataObject::pointer> mDataToRender;

        friend class View;
        friend class RenderToImage;

        View* m_view;
    private:

        /**
         * OpenGL shader IDs. Program name -> OpenGL ID
         */
        std::unordered_map<std::string, uint> mShaderProgramIDs;

};

}
