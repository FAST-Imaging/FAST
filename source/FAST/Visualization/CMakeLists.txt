fast_add_sources(
    Plane.cpp
    Plane.hpp
)
if(FAST_MODULE_Visualization)
fast_add_python_interfaces(Renderer.hpp View.hpp Window.hpp SimpleWindow.hpp DualViewWindow.hpp MultiViewWindow.hpp SlicerWindow.hpp LabelColorRenderer.hpp ComputationThread.hpp)
fast_add_python_shared_pointers(Window SimpleWindow SimpleWindow2D SimpleWindow3D DualViewWindow DualViewWindow2D DualViewWindow3D MultiViewWindow MultiViewWindow2D MultiViewWindow3D SlicerWindow Renderer ImageRendererBase View LabelColorRenderer ComputationThread)
fast_add_process_object(ImageRenderer ImageRenderer/ImageRenderer.hpp)
fast_add_sources(
    Window.cpp
    Window.hpp
    SimpleWindow.cpp
    SimpleWindow.hpp
    DualViewWindow.cpp
    DualViewWindow.hpp
    MultiViewWindow.cpp
    MultiViewWindow.hpp
    WindowWidget.cpp
    WindowWidget.hpp
    ComputationThread.cpp
    ComputationThread.hpp
    View.cpp
    View.hpp
    Renderer.cpp
    Renderer.hpp
    LabelColorRenderer.cpp
    LabelColorRenderer.hpp
    SlicerWindow.cpp
    SlicerWindow.hpp
    Shortcuts.cpp
    Shortcuts.hpp
)
fast_add_all_subdirectories()
fast_add_test_sources()
endif()


