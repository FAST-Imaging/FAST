#include "Shortcuts.hpp"
#include "SlicerWindow.hpp"
#include <FAST/Visualization/ImageRenderer/ImageRenderer.hpp>
#include <FAST/Visualization/ImagePyramidRenderer/ImagePyramidRenderer.hpp>
#include <FAST/Visualization/SegmentationRenderer/SegmentationRenderer.hpp>
#include <FAST/Visualization/SimpleWindow.hpp>
#include <FAST/Visualization/RenderToImage/RenderToImage.hpp>
#include <FAST/Data/ImagePyramid.hpp>
#include <FAST/Visualization/VertexRenderer/VertexRenderer.hpp>
#include <FAST/Visualization/VolumeRenderer/AlphaBlendingVolumeRenderer.hpp>
#include <FAST/Visualization/VolumeRenderer/MaximumIntensityProjection.hpp>

namespace fast {

template <class T>
bool hasValue(T type) {
    return !std::holds_alternative<std::monostate>(type);
}

template <class T>
T getValueOrDefault(std::optional<T> variable, T defaultValue) {
    if(variable.has_value()) {
        return variable.value();
    } else {
        return defaultValue;
    }
}

std::variant<std::monostate, Window::pointer, Image::pointer> display2D(Display2DArgs args) {
    if(!hasValue(args.image) &&
       !hasValue(args.segmentation) &&
       !hasValue(args.imagePyramid) &&
       !hasValue(args.vertices) &&
       !hasValue(args.lines)
       ) {
        throw Exception("No data given to display2D");
    }

    std::vector<Renderer::pointer> renderers;
    if(hasValue(args.image)) {
        auto renderer = ImageRenderer::create(
                getValueOrDefault(args.intensityLevel, -1.0f),
                getValueOrDefault(args.intensityWindow, -1.0f)
        );
        if(std::holds_alternative<Image::pointer>(args.image)) {
            renderer->connect(std::get<Image::pointer>(args.image));
        } else {
            renderer->connect(std::get<ProcessObject::pointer>(args.image));
        }
        renderers.push_back(renderer);
    }

    if(hasValue(args.imagePyramid)) {
        auto renderer = ImagePyramidRenderer::create();
        if(std::holds_alternative<ImagePyramid::pointer>(args.imagePyramid)) {
            renderer->connect(std::get<ImagePyramid::pointer>(args.imagePyramid));
        } else {
            renderer->connect(std::get<ProcessObject::pointer>(args.imagePyramid));
        }
        renderers.push_back(renderer);
    }

    if(hasValue(args.segmentation)) {
        auto renderer = SegmentationRenderer::create(
                args.segmentationColors,
                args.segmentationOpacity,
                args.segmentationBorderOpacity,
                args.segmentationBorderRadius
        );
        if(std::holds_alternative<Image::pointer>(args.segmentation)) {
            renderer->connect(std::get<Image::pointer>(args.segmentation));
        } else if(std::holds_alternative<ImagePyramid::pointer>(args.segmentation)) {
            renderer->connect(std::get<ImagePyramid::pointer>(args.segmentation));
        } else {
            renderer->connect(std::get<ProcessObject::pointer>(args.segmentation));
        }
        renderers.push_back(renderer);
    }

    if(hasValue(args.vertices)) {
        auto renderer = VertexRenderer::create(args.vertexSize, args.vertexSizeIsInPixels, args.vertexMinSize, args.vertexColor, args.vertexLabelColors, args.vertexOpacity);
        if(std::holds_alternative<Mesh::pointer>(args.vertices)) {
            renderer->connect(std::get<Mesh::pointer>(args.vertices));
        } else {
            renderer->connect(std::get<ProcessObject::pointer>(args.vertices));
        }
        renderers.push_back(renderer);
    }

    if(hasValue(args.lines)) {
        auto renderer = LineRenderer::create(args.lineWidth, args.lineColor, args.lineLabelColors, args.lineOpacity);
        if(std::holds_alternative<Mesh::pointer>(args.lines)) {
            renderer->connect(std::get<Mesh::pointer>(args.lines));
        } else {
            renderer->connect(std::get<ProcessObject::pointer>(args.lines));
        }
        renderers.push_back(renderer);
    }

    if(args.renderToImage) {
        int width = args.width;
        if(width == 0)
            width = 1024;
        int height = args.height;
        if(height == 0)
            height = -1;
        auto renderToImage = RenderToImage::create(args.bgcolor, width, height)
                ->connect(renderers);
        return renderToImage->runAndGetOutputData<Image>();
    } else {
        auto window = SimpleWindow2D::create(args.bgcolor, args.width, args.height)
                ->connect(renderers);
        if(args.fullscreen)
            window->enableFullscreen();
        if(args.maximize)
            window->enableMaximized();
        if(!args.title.empty())
            window->setTitle(args.title);
        if(hasValue(args.widgets)) {
            if(std::holds_alternative<std::vector<QWidget*>>(args.widgets)) {
                window->connect(std::get<std::vector<QWidget*>>(args.widgets));
            } else {
                for(const auto& item : std::get<std::map<WidgetPosition, std::vector<QWidget*>>>(args.widgets)) {
                    window->connect(item.second, item.first);
                }
            }
        }
        if(args.timeout.has_value())
            window->setTimeout(args.timeout.value());
        if(args.returnWindow) {
            return window;
        } else {
            window->run();
            return std::monostate{};
        }
    }
}

std::variant<std::monostate, Window::pointer> display3D(Display3DArgs args) {
    if(!hasValue(args.image) &&
       !hasValue(args.segmentation)
            ) {
        throw Exception("No data given to display3D");
    }

    std::shared_ptr<Window> window;
    if(args.displayType == DisplayType::SLICER) {
        auto slicerWindow = SlicerWindow::create(args.bgcolor, args.width, args.height);
        if(args.fullscreen)
            slicerWindow->enableFullscreen();
        if(args.maximize)
            slicerWindow->enableMaximized();
        if(!args.title.empty())
            slicerWindow->setTitle(args.title);
        if(hasValue(args.image)) {
            if(std::holds_alternative<Image::pointer>(args.image)) {
                slicerWindow->connectImage(std::get<Image::pointer>(args.image), getValueOrDefault(args.intensityLevel, -1.0f),
                                      getValueOrDefault(args.intensityWindow, -1.0f));
            } else {
                slicerWindow->connectImage(std::get<ProcessObject::pointer>(args.image), getValueOrDefault(args.intensityLevel, -1.0f),
                                           getValueOrDefault(args.intensityWindow, -1.0f));
            }
        }
        if(hasValue(args.segmentation)) {
            if(std::holds_alternative<Image::pointer>(args.segmentation)) {
                slicerWindow->connectSegmentation(std::get<Image::pointer>(args.segmentation), args.segmentationColors, args.segmentationOpacity, args.segmentationBorderOpacity, args.segmentationBorderRadius);
            } else {
                slicerWindow->connectSegmentation(std::get<ProcessObject::pointer>(args.segmentation), args.segmentationColors, args.segmentationOpacity, args.segmentationBorderOpacity, args.segmentationBorderRadius);
            }
        }
        window = slicerWindow;
    } else {
        if(!hasValue(args.image))
            throw Exception("display3D must have image for volume rendering");
        auto simpleWindow = SimpleWindow3D::create(args.bgcolor, args.width, args.height);
        if(args.fullscreen)
            simpleWindow->enableFullscreen();
        if(args.maximize)
            simpleWindow->enableMaximized();
        if(!args.title.empty())
            simpleWindow->setTitle(args.title);
        if(args.displayType == DisplayType::ALPHA_BLENDING) {
            auto renderer = AlphaBlendingVolumeRenderer::create(args.transferFunction);
            if(std::holds_alternative<Image::pointer>(args.image)) {
                renderer->connect(std::get<Image::pointer>(args.image));
            } else {
                renderer->connect(std::get<ProcessObject::pointer>(args.image));
            }
            simpleWindow->addRenderer(renderer);
        } else if(args.displayType == DisplayType::MAXIMUM_INTENSITY_PROJECTION) {
            auto renderer = MaximumIntensityProjection::create();
            if(std::holds_alternative<Image::pointer>(args.image)) {
                renderer->connect(std::get<Image::pointer>(args.image));
            } else {
                renderer->connect(std::get<ProcessObject::pointer>(args.image));
            }
            simpleWindow->addRenderer(renderer);
        } else {
            throw Exception("Invalid display type");
        }
        window = simpleWindow;
    }

    if(hasValue(args.widgets)) {
        if(std::holds_alternative<std::vector<QWidget*>>(args.widgets)) {
            window->connect(std::get<std::vector<QWidget*>>(args.widgets));
        } else {
            for(const auto& item : std::get<std::map<WidgetPosition, std::vector<QWidget*>>>(args.widgets)) {
                window->connect(item.second, item.first);
            }
        }
    }
    if(args.timeout.has_value())
        window->setTimeout(args.timeout.value());
    if(args.returnWindow) {
        return window;
    } else {
        window->run();
        return std::monostate{};
    }
}

}