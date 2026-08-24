#pragma once

#include <FAST/Visualization/LineRenderer/LineRenderer.hpp>

namespace fast {

/**
 * @brief Renders a vector field image using lines
 *
 * @ingroup renderers
 */
class FAST_EXPORT VectorFieldRenderer : public LineRenderer {
    FAST_PROCESS_OBJECT(VectorFieldRenderer)
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
        FAST_CONSTRUCTOR(VectorFieldRenderer,
                         float, lineWidth, = 1.0f,
                         Color, color, = Color::Null(),
                         LabelColors, labelColors, = LabelColors(),
                         float, opacity, = 1.0f,
                         bool, drawOnTop, = false
                                 );
    protected:
        void execute() override;
};

}