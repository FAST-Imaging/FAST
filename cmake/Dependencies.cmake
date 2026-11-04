# Setup all dependencies for FAST, both internal (have to be installed on the system)
# and external (downloaded automatically)

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/lib/)
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/bin/)
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/include/)
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/licenses/)

## OpenCL
if(WIN32)
    fast_download_dependency(opencl
            3.0.8
            0b2d3098ed29fac09bbd7f831647e08e23324eaccbdb29c603617989e48c50b1
            OpenCL.lib
    )
elseif(APPLE)
	find_package(OpenCL REQUIRED) # Need to link with Apple OpenCL binary and not khronos..?
	list(APPEND LIBRARIES ${OpenCL_LIBRARIES})
	list(APPEND FAST_INCLUDE_DIRS ${OpenCL_INCLUDE_DIRS})
if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        fast_download_dependency(opencl
            3.0.8
            a97ebc4eb918ba0bd7ae4b1a01afdbd1097fa300854da433f6687eaa891f75fa
        )
else()
        fast_download_dependency(opencl
            3.0.8
            63d72f3af7a3362e48ea45b55c772f2717de55a27de5aaf38e55e0364a4ee45b
        )
endif()
else()
    fast_download_dependency(opencl
            3.0.8
            a9fc571dc6cb034145e1a9dcae63649762a8a4616eca617ffabc7c77a7fb1894
            libOpenCL.so
    )
endif()

find_package(OpenGL REQUIRED)
list(APPEND FAST_SYSTEM_LIBRARIES ${OPENGL_LIBRARIES})

## Qt
if(FAST_MODULE_Visualization)
    if(FAST_BUILD_QT5)
        # Use FAST build of Qt
        if(WIN32)
            fast_download_dependency(qt5
                    5.15.2
		    3d025e412d4af7d4fe669677ac71614909eb44aac8eca6b8fb29459abf621dff
                    Qt5Core.lib Qt5Gui.lib Qt5Widgets.lib Qt5OpenGL.lib Qt5Multimedia.lib Qt5MultimediaWidgets.lib Qt5Network.lib Qt5PrintSupport.lib Qt5SerialPort.lib
            )
            fast_download_dependency(openssl
                    1.1.1
                    e0f9b5d26627c70abbfa9e3b0d731a81995bdd2af2177eebf1c4b32691643c9e
            )
      	elseif(APPLE)
		if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
			fast_download_dependency(qt5
			    5.15.8
			    2c5e30b1ae862e9d47f3e59ae2346eea0cb24c2208307bbade6ac2ec8bca3463
			    libQt5Core.dylib libQt5Gui.dylib libQt5Widgets.dylib libQt5OpenGL.dylib libQt5Multimedia.dylib libQt5MultimediaWidgets.dylib libQt5Network.dylib libQt5PrintSupport.dylib libQt5SerialPort.dylib
			    )
		else()
            fast_download_dependency(qt5
                    5.15.2
		    0f5ff6ea8a9299cda1f41922f310a1f8f5bb8bf8829c46e85eb68e2638e2a840
                    libQt5Core.dylib libQt5Gui.dylib libQt5Widgets.dylib libQt5OpenGL.dylib libQt5Multimedia.dylib libQt5MultimediaWidgets.dylib libQt5Network.dylib libQt5PrintSupport.dylib libQt5SerialPort.dylib
            )
		endif()
        else()
            fast_download_dependency(qt5
                    5.15.2
		    71bf6dbae6aa24cc5e64379cb04899f6abf4fe512d87093f3f566c6b0fca4fa9
                    libQt5Core.so libQt5Gui.so libQt5Widgets.so libQt5OpenGL.so libQt5Multimedia.so libQt5MultimediaWidgets.so libQt5Network.so libQt5PrintSupport.so libQt5SerialPort.so
            )
            fast_download_dependency(openssl
                    1.1.1
                    813d09d0e4fb8c03b4470692659d8600e5d56c77708aa27c0290e9be03cc7352
            )
        endif()
        # Need to set version manually to suppress warnings
        set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                PROPERTY Qt5Core_VERSION_MAJOR "5")
        set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                PROPERTY Qt5Core_VERSION_MINOR "15")
        set(Qt5Core_VERSION_MAJOR "5")
        set(Qt5Core_VERSION_MINOR "15")
        add_executable(Qt5::moc IMPORTED)
        add_dependencies(Qt5::moc qt5)
        set(MOC_FILENAME "${PROJECT_BINARY_DIR}/bin/moc${CMAKE_EXECUTABLE_SUFFIX}" )
        set_target_properties(Qt5::moc PROPERTIES IMPORTED_LOCATION ${MOC_FILENAME})
        # HACK: Create fake moc executable to avoid errors on newer cmake
        if(NOT EXISTS ${MOC_FILENAME})
            # Copy an executable we know exist: cmake(.exe)
            file(COPY ${CMAKE_COMMAND}
                DESTINATION "${PROJECT_BINARY_DIR}/bin/"
                FOLLOW_SYMLINK_CHAIN
                FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_WRITE WORLD_EXECUTE)
            # Rename it to moc(.exe)
            file(RENAME ${PROJECT_BINARY_DIR}/bin/cmake${CMAKE_EXECUTABLE_SUFFIX} ${PROJECT_BINARY_DIR}/bin/moc${CMAKE_EXECUTABLE_SUFFIX})
        endif()
        set(QT_MODULES QtCore QtGui QtWidgets QtOpenGL QtOpenGLWidgets QtMultimedia QtMultimediaWidgets QtPrintSupport QtNetwork)
        foreach(ITEM ${QT_MODULES})
            list(APPEND FAST_INCLUDE_DIRS ${FAST_EXTERNAL_INSTALL_DIR}/include/${ITEM})
        endforeach()
    else(FAST_BUILD_QT5)
        # Use system Qt
        find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL Multimedia MultimediaWidgets PrintSupport Network)
        list(APPEND LIBRARIES
                Qt5::Core
                Qt5::Gui
                Qt5::Widgets
                Qt5::OpenGL
                Qt5::Multimedia
                Qt5::MultimediaWidgets
                Qt5::PrintSupport
                Qt5::Network
        )
        list(APPEND FAST_INCLUDE_DIRS
                ${Qt5Widgets_INCLUDE_DIRS}
                ${Qt5Core_INCLUDE_DIRS}
                ${Qt5Gui_INCLUDE_DIRS}
                ${Qt5OpenGL_INCLUDE_DIRS}
                ${Qt5Multimedia_INCLUDE_DIRS}
                ${Qt5MultimediaWidgets_INCLUDE_DIRS}
                ${Qt5PrintSupport_INCLUDE_DIRS}
                ${Qt5Network_INCLUDE_DIRS}
        )
    endif(FAST_BUILD_QT5)

    set(CMAKE_AUTOMOC ON)

    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        add_definitions("-fPIC") # Get rid of Qt error with position independent code
    endif()
    add_definitions("-DFAST_MODULE_VISUALIZATION")
endif()

## External dependencies
include(cmake/ExternalEigen.cmake)
add_definitions("-DEIGEN_MPL2_ONLY") # Avoid using LGPL code in eigen http://eigen.tuxfamily.org/index.php?title=Main_Page#License
include(cmake/ExternalZlib.cmake)
include(cmake/ExternalZip.cmake)
include(cmake/ImageCompressionDependencies.cmake)

# Optional modules
include(cmake/ModuleVTK.cmake)
include(cmake/ModuleITK.cmake)
include(cmake/ModuleOpenIGTLink.cmake)
include(cmake/ModuleKinect.cmake)
include(cmake/ModuleRealSense.cmake)
include(cmake/ModuleWholeSlideImaging.cmake)
include(cmake/ModuleClarius.cmake)
include(cmake/ModuleDICOM.cmake)
include(cmake/ModuleHDF5.cmake)
include(cmake/ModulePlotting.cmake)

# Make sure FAST can find external includes and libaries
link_directories(${FAST_EXTERNAL_INSTALL_DIR}/lib/)
list(APPEND FAST_INCLUDE_DIRS ${FAST_EXTERNAL_INSTALL_DIR}/include)
