#pragma once

#include <FAST/Visualization/LabelColorRenderer.hpp>
#include <FAST/Data/Color.hpp>

namespace fast {

/**
 * @brief Renders lines stored in Mesh data objects.
 *
 * @ingroup renderers
 */
class FAST_EXPORT  LineRenderer : public LabelColorRenderer {
    FAST_PROCESS_OBJECT(LineRenderer)
    public:
        /**
         * @brief Create instance
         * @param lineWidth Width of line
         * @param color Global color to use for lines
         * @param labelColors Label colors
         * @param opacity Opacity of lines: 1 = no transparency, 0 = fully transparent
         * @param drawOnTop Whether to draw on top of everything else or not. This disables the depth check in OpenGL
         * @return instance
         */
        FAST_CONSTRUCTOR(LineRenderer,
                         float, lineWidth, = 1.0f,
                         Color, color, = Color::Null(),
                         LabelColors, labelColors, = LabelColors(),
                         float, opacity, = 1.0f,
                         bool, drawOnTop, = false
        );
        uint addInputConnection(DataChannel::pointer port) override;
        uint addInputConnection(DataChannel::pointer port, Color color, float width);
        void setOpacity(float opacity);
        void setDefaultColor(Color color);
        /**
         * @brief Set line width in percent (2D mode only atm.)
         * @param width
         */
        void setDefaultLineWidth(float width);
        void setDefaultDrawOnTop(bool drawOnTop);
        void setDrawOnTop(uint inputNr, bool drawOnTop);
        void setColor(uint inputNr, Color color);
        void setWidth(uint inputNr, float width);
        void setDrawJoints(bool draw);
        void
        draw(Matrix4f perspectiveMatrix, Matrix4f viewingMatrix, float zNear, float zFar, bool mode2D, int viewWidth,
             int viewHeight);
    protected:
        float m_opacity = 1.0f;
        bool m_drawJoints = true;
        float mDefaultLineWidth;
        Color mDefaultColor;
        bool mDefaultColorSet;
        bool mDefaultDrawOnTop;
        std::unordered_map<uint, float> mInputWidths;
        std::unordered_map<uint, Color> mInputColors;
        std::unordered_map<uint, bool> mInputDrawOnTop;
        std::unordered_map<uint, uint> mVAO;
};

}

