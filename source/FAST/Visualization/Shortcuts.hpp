#pragma once

#include <FAST/Visualization/Window.hpp>
#include <FAST/Data/Image.hpp>
#include <FAST/Visualization/VolumeRenderer/TransferFunction.hpp>
#include <variant>
#include <optional>

class QWidget;

namespace fast {

class ImagePyramid;
class Mesh;

#ifndef SWIG

/**
 * @defgroup shortcuts Shortcuts
 * Functions for reducing boiler plate code such as display2D and display3D.
 */

/**
 * @brief Arguments for display2D function.
 * @ingroup shortcuts
 */
struct Display2DArgs {
    std::variant<std::monostate, std::shared_ptr<Image>, std::shared_ptr<ProcessObject>> image; /**< Source of Image data to display (optional) **/
    std::variant<std::monostate, std::shared_ptr<ImagePyramid>, std::shared_ptr<ProcessObject>> imagePyramid;  /**< Source of ImagePyramid data to display (optional) **/
    std::variant<std::monostate, std::shared_ptr<Image>, std::shared_ptr<ImagePyramid>, std::shared_ptr<ProcessObject>> segmentation; /**< Source of segmentation Image data to display (optional) **/
    std::variant<std::monostate, std::shared_ptr<Mesh>, std::shared_ptr<ProcessObject>> vertices; /**< Source of vertices Mesh data to display (optional) **/
    std::variant<std::monostate, std::shared_ptr<Mesh>, std::shared_ptr<ProcessObject>> lines; /**< Source of lines Mesh data to display (optional) **/
    std::optional<float> intensityLevel; /**< Intensity level used by ImageRenderer **/
    std::optional<float> intensityWindow; /**< Intensity window used by ImageRenderer **/
    LabelColors segmentationColors;
    float segmentationOpacity = 0.5f;
    float segmentationBorderOpacity = -1.0f;
    int segmentationBorderRadius = 1;
    float lineWidth = 1.0f;
    Color lineColor = Color::Null();
    LabelColors lineLabelColors;
    float lineOpacity = 1.0f;
    float vertexSize = 10.0;
    bool vertexSizeIsInPixels = true;
    int vertexMinSize = 1;
    Color vertexColor = Color::Null();
    LabelColors vertexLabelColors;
    float vertexOpacity = 1.0f;
    Color bgcolor = Color::White(); /**< Background color to use for the View. **/
    int width = 0; /**< Width of window, set to 0 to use default width. **/
    int height = 0; /**< Height of window, set to 0 to use default height. **/
    std::optional<int> timeout; /**< Milliseconds until closing window automatically. If not set, the window will not automatically close. **/
    bool renderToImage = false; /**< If set to true, the data is rendered to an Image and returned, instead of displaying it using a window. **/
    bool returnWindow = false; /**< If set to true, the window is returned by the function, else display2D will call run on the window and return nothing. **/
    std::variant<std::monostate, std::vector<QWidget*>, std::map<WidgetPosition, std::vector<QWidget*>>> widgets;  /**< Widgets to attach to window. **/
    std::string title = ""; /**< Title to set to window, if renderToImage is false. **/
    bool fullscreen = false; /**< Enable fullscreen window, if renderToImage is false. **/
    bool maximize = false; /**< Maximize window, if renderToImage is false. **/
};

/**
 * @brief A function for displaying data in 2D
 *
 * Use this to reduce boiler plate code when displaying data in 2D.
 *
 * @sa Display2DArgs
 * @sa display3D
 *
 * @param args See the Display2DArgs struct
 * @return A variant which is either empty, Window or Image depending on the choice of Display2DArgs.returnWindow
 *      and Display2DArgs.renderToImage
 * @ingroup shortcuts
 */
FAST_EXPORT std::variant<std::monostate, Window::pointer, Image::pointer> display2D(Display2DArgs args);

/**
 * @brief Enum to choose how to display 3D data in display3D
 * @sa display3D
 * @ingroup shortcuts
 */
enum class DisplayType {
    SLICER = 1, /**< Display 3D image and segmentation using SlicerWindow. **/
    ALPHA_BLENDING = 2, /**< Display 3D image using AlphaBlendingVolumeRenderer. **/
    MAXIMUM_INTENSITY_PROJECTION = 3 /**< Display 3D image using MaximumIntensityProjection. **/
};

/**
 * @brief Arguments for display3D function.
 * @ingroup shortcuts
 */
struct Display3DArgs {
    std::variant<std::monostate, std::shared_ptr<Image>, std::shared_ptr<ProcessObject>> image; /**< Source of Image data to display (optional) **/
    std::variant<std::monostate, std::shared_ptr<Image>, std::shared_ptr<ProcessObject>> segmentation; /**< Source of segmentation Image data to display (optional) **/
    std::optional<float> intensityLevel; /**< Intensity level used by ImageRenderer. Only used with DisplayType::SLICER **/
    std::optional<float> intensityWindow; /**< Intensity window used by ImageRenderer. Only used with DisplayType::SLICER **/
    LabelColors segmentationColors;
    float segmentationOpacity = 0.5f;
    float segmentationBorderOpacity = -1.0f;
    int segmentationBorderRadius = 1;
    TransferFunction transferFunction; /**< Transfer function to use by AlphaBlendingVolumeRenderer when displayType == DisplayType::ALPHA_BLENDING **/
    DisplayType displayType = DisplayType::SLICER;
    Color bgcolor = Color::White(); /**< Background color to use for the View. **/
    int width = 0; /**< Width of window, set to 0 to use default width. **/
    int height = 0; /**< Height of window, set to 0 to use default height. **/
    std::optional<int> timeout;  /**< Milliseconds until closing window automatically. If not setm the window will not automatically close. **/
    bool returnWindow = false; /**< If set to true, the window is returned by the function, else display2D will call run on the window and return nothing. **/
    std::variant<std::monostate, std::vector<QWidget*>, std::map<WidgetPosition, std::vector<QWidget*>>> widgets; /**< Widgets to attach to window. **/
    std::string title = ""; /**< Title to set to window, if renderToImage is false. **/
    bool fullscreen = false; /**< Enable fullscreen window, if renderToImage is false. **/
    bool maximize = false; /**< Maximize window, if renderToImage is false. **/
};

/**
 * @brief A function for displaying data in 3D
 *
 * Use this to reduce boiler plate code when displaying data in 3D.
 *
 * TODO:
 * - Geometry support
 * - renderToImage
 *
 * @sa Display3DArgs
 * @sa display2D
 *
 * @param args See the Display3DArgs struct
 * @return A variant which is either empty or a Window depending on whether Display3DArgs.returnWindow is set to true or not.
 * @ingroup shortcuts
 */
FAST_EXPORT std::variant<std::monostate, Window::pointer> display3D(Display3DArgs args);

#endif

}