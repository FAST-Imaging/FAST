"""
A few shortcut functions
"""
from typing import Dict, Union
import fast

def _set_default_shorcut_value(x, default):
    return default if x is None else x

def display2D(
            image:Union[Image, ProcessObject]=None,
            segmentation:Union[Image, ImagePyramid, ProcessObject]=None,
            imagePyramid:Union[ImagePyramid, ProcessObject]=None,
            lines:Union[Mesh, ProcessObject]=None,
            vertices: Union[Mesh, ProcessObject]=None,
            intensityLevel:float=None,
            intensityWindow:float=None,
            segmentationColors:Dict[int, Color]=None,
            segmentationOpacity:float=0.5,
            segmentationBorderOpacity:float=None,
            segmentationBorderRadius:int=1,
            lineWidth:float=1,
            lineColor:Color=Color.Green(),
            vertexSize:float=10.0,
            vertexSizeIsInPixels:bool=True,
            vertexMinSize:int=1,
            vertexColor:Color=Color.Null(),
            vertexLabelColors:Dict[int, Color]=None,
            vertexOpacity:float=1.0,
            bgcolor:Color=Color.White(),
            width:int=None,
            height:int=None,
            timeout:int=None,
            renderToImage:bool=False,
            returnWindow:bool=False,
            widgets:Union[List, Dict[int, List]]=None,
            title:str='',
            fullscreen:bool=False,
            maximize:bool=False,
        ):
    """
    Shortcut for displaying image, segmentation and meshes using SimpleWindow2D

    :param image: Image to display
    :param segmentation: Segmentation to display
    :param imagePyramid: ImagePyramid to display
    :param lines: Lines to display
    :param vertices: Vertices to display
    :param intensityLevel: Intensity level for image rendering
    :param intensityWindow: Intensity window for image rendering
    :param segmentationColors: Colors to use for segmentation
    :param segmentationOpacity: Opacity of segmentation
    :param segmentationBorderOpacity: Border opacity of segmentation
    :param segmentationBorderRadius: Size of segmentation border
    :param lineWidth: Width of line
    :param lineColor: Color of line
    :param vertexSize Vertex point size (can be in pixels or millimeters, see sizeIsInPixels parameter)
    :param vertexSizeIsInPixels Whether size is given in pixels or millimeters
    :param vertexMinSize Minimum size in pixels, used when sizeInPixels = false
    :param vertexColor Override color stored for each vertex
    :param vertexLabelColors Set vertex color per label
    :param vertexOpacity Opacity of vertices: 1 = no transparency, 0 = fully transparent
    :param bgcolor: Background color
    :param width: Width of window
    :param height: Height of window
    :param timeout: If set to a number, the window will auto close after this many milliseconds
    :param renderToImage: Use RenderToImage instead of SimpleWindow and return the resulting image
    :param returnWindow: Whether to return the window object, or to run it
    :param widgets: Widgets to connect to the window
    :param title: Title to set to window, if renderToImage is false.
    :param fullscreen: Enable fullscreen window, if renderToImage is false.
    :param maximize: Maximize window, if renderToImage is false.
    :return: window if returnWindow is set to True, else None
    """

    if image is None and imagePyramid is None and segmentation is None and lines is None and vertices is None:
        raise ValueError('No data was given to display2D')

    width = _set_default_shorcut_value(width, 0)
    height = _set_default_shorcut_value(height, 0)
    intensityLevel = _set_default_shorcut_value(intensityLevel, -1)
    intensityWindow = _set_default_shorcut_value(intensityWindow, -1)
    segmentationColors = _set_default_shorcut_value(segmentationColors, LabelColors())
    segmentationBorderOpacity = _set_default_shorcut_value(segmentationBorderOpacity, -1)
    vertexLabelColors = _set_default_shorcut_value(vertexLabelColors, LabelColors())

    renderers = []

    if image is not None:
        renderer = ImageRenderer.create(
            level=intensityLevel,
            window=intensityWindow
        ).connect(image)
        renderers.append(renderer)

    if imagePyramid is not None:
        renderer = ImagePyramidRenderer.create(
        ).connect(imagePyramid)
        renderers.append(renderer)

    if segmentation is not None:
        renderer = SegmentationRenderer.create(
            colors=segmentationColors,
            opacity=segmentationOpacity,
            borderOpacity=segmentationBorderOpacity,
            borderRadius=segmentationBorderRadius
        ).connect(segmentation)
        renderers.append(renderer)

    if lines is not None:
        renderer = LineRenderer.create(
            lineWidth=lineWidth,
            color=lineColor
        ).connect(lines)
        renderers.append(renderer)

    if vertices is not None:
        renderer = VertexRenderer.create(
            size=vertexSize,
            sizeIsInPixels=vertexSizeIsInPixels,
            minSize=vertexMinSize,
            color=vertexColor,
            opacity=vertexOpacity,
            labelColors=vertexLabelColors
        ).connect(vertices)
        renderers.append(renderer)

    if renderToImage:
        render = RenderToImage.create(
            bgcolor=bgcolor,
            width=width,
            height=height
        ).connect(renderers)
        return render.runAndGetOutputData()
    else:
        window = SimpleWindow2D.create(
            bgcolor=bgcolor,
            width=width,
            height=height
        ).connect(renderers)
        if title: window.setTitle(title)
        if fullscreen: window.enableFullscreen()
        if maximize: window.enableMaximized()
        if widgets:
            if isinstance(widgets, dict):
                for pos, widget_list in widgets.items():
                    window.connect(widget_list, pos)
            else:
                window.connect(widgets)
        if timeout:
            window.setTimeout(timeout)
        if returnWindow:
            return window
        else:
            window.run()


from enum import Enum
class DisplayType(Enum):
    SLICER = 1
    ALPHA_BLENDING = 2
    MAXIMUM_INTENSITY_PROJECTION = 3

def display3D(
        image:Union[Image, ProcessObject]=None,
        segmentation:Union[Image, ProcessObject]=None,
        intensityLevel:float=None,
        intensityWindow:float=None,
        segmentationColors:Dict[int, Color]=None,
        segmentationOpacity:float=0.5,
        segmentationBorderOpacity:float=None,
        segmentationBorderRadius:int=1,
        transferFunction:TransferFunction=None,
        displayType:DisplayType=DisplayType.SLICER,
        bgcolor:Color=Color.White(),
        width:int=None,
        height:int=None,
        timeout:int=None,
        returnWindow:bool=False,
        widgets:Union[List, Dict[int, List]]=None,
        title:str='',
        fullscreen:bool=False,
        maximize:bool=False,
):
    """
    Shortcut for displaying image, segmentation and meshes using SimpleWindow2D

    TODO:
        * Surface extraction or threshold volume rendering?
        * Mesh support: Vertices, lines and triangles

    :param image: Image to display
    :param segmentation: Segmentation to display
    :param bgcolor: Background color
    :param width: Width of window
    :param height: Height of window
    :param timeout: If set to a number, the window will auto close after this many milliseconds
    :param returnWindow: Whether to return the window object, or to run it
    :param widgets: Widgets to connect to the window
    :param title: Title to set to window, if renderToImage is false.
    :param fullscreen: Enable fullscreen window, if renderToImage is false.
    :param maximize: Maximize window, if renderToImage is false.
    :return: window if returnWindow is set to True, else None
    """

    width = _set_default_shorcut_value(width, 0)
    height = _set_default_shorcut_value(height, 0)
    intensityLevel = _set_default_shorcut_value(intensityLevel, -1)
    intensityWindow = _set_default_shorcut_value(intensityWindow, -1)
    segmentationColors = _set_default_shorcut_value(segmentationColors, LabelColors())
    segmentationBorderOpacity = _set_default_shorcut_value(segmentationBorderOpacity, -1)
    transferFunction = _set_default_shorcut_value(transferFunction, TransferFunction())

    if displayType == DisplayType.SLICER:
        window = fast.SlicerWindow.create(bgcolor=bgcolor, width=width, height=height)
        if image is not None:
            window.connectImage(image, intensityLevel, intensityWindow)

        if segmentation is not None:
            window.connectSegmentation(
                segmentation,
                colors=segmentationColors,
                opacity=segmentationOpacity,
                borderOpacity=segmentationBorderOpacity,
                borderRadius=segmentationBorderRadius
            )

    else:
        window = fast.SimpleWindow3D.create(bgcolor=bgcolor, width=width, height=height)
        if displayType == DisplayType.MAXIMUM_INTENSITY_PROJECTION:
            renderer = fast.MaximumIntensityProjection.create().connect(image)
            window.connect(renderer)
        elif displayType == DisplayType.ALPHA_BLENDING:
            renderer = fast.AlphaBlendingVolumeRenderer.create(transferFunction).connect(image)
            window.connect(renderer)

    if title: window.setTitle(title)
    if fullscreen: window.enableFullscreen()
    if maximize: window.enableMaximized()
    if timeout is not None: window.setTimeout(timeout)
    if widgets:
        if isinstance(widgets, dict):
            for pos, widget_list in widgets.items():
                window.connect(widget_list, pos)
        else:
            window.connect(widgets)
    if returnWindow:
        return window
    else:
        window.run()

