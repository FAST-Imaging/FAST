# Download clarius cast API
include(cmake/Externals.cmake)

ExternalProject_Add(clarius_headers
    PREFIX ${FAST_EXTERNAL_BUILD_DIR}/clarius
    URL https://github.com/clariusdev/cast/archive/f21f9de7ae7740e61eb1ca0c73c43592e5b2f766.zip #v7.30 with export header fix
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory ${FAST_EXTERNAL_BUILD_DIR}/clarius/src/clarius_headers/src/include ${FAST_EXTERNAL_INSTALL_DIR}/include/
)

if(WIN32)
    set(URL "https://github.com/clariusdev/cast/releases/download/v7.3.0/clarius-cast-v730-windows.zip")
else()
    set(URL "https://github.com/clariusdev/cast/releases/download/v7.3.0/clarius-cast-v730-linux.zip")
endif()

if(WIN32)
ExternalProject_Add(clarius
        PREFIX ${FAST_EXTERNAL_BUILD_DIR}/clarius # The folder in which the package will downloaded to
        URL ${URL}
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/clarius/src/clarius/cast.lib ${FAST_EXTERNAL_INSTALL_DIR}/lib/ COMMAND
          ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/clarius/src/clarius/cast.dll ${FAST_EXTERNAL_INSTALL_DIR}/bin/
        DEPENDS clarius_headers

)
else()
ExternalProject_Add(clarius
        PREFIX ${FAST_EXTERNAL_BUILD_DIR}/clarius # The folder in which the package will downloaded to
        URL ${URL}
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${FAST_EXTERNAL_BUILD_DIR}/clarius/src/clarius/libcast.so ${FAST_EXTERNAL_INSTALL_DIR}/lib/
        DEPENDS clarius_headers
)
endif()
list(APPEND FAST_EXTERNAL_DEPENDENCIES clarius)
