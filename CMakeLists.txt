cmake_minimum_required(VERSION 3.7)
project(NexgenRedux VERSION 0.1.0)

# Detect operating system
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    SET(OperatingSystem "Win")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
   SET(OperatingSystem "Mac")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SET(OperatingSystem "Linux")
else()
    message(FATAL_ERROR "Unrecognized operating system '${CMAKE_SYSTEM_NAME}'")
endif()
message(STATUS "Nexgen Redux build for ${OperatingSystem}...")

# Build GLAD library
add_library(GLAD STATIC "${PROJECT_SOURCE_DIR}/Libraries/GLAD/Source/glad.c")

# Ensure includes visible to project
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

# Define include folders
set(INCLUDE_DIRS "${PROJECT_NAME}/NexgenRedux")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/Libraries/GLAD/Includes/")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/Libraries/GLFW/Includes/")
include_directories(${INCLUDE_DIRS})

# Define source code files
set(APP_SOURCE ${PROJECT_NAME})
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux/main.cpp")
if (${OperatingSystem} MATCHES "Win")
    list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Artwork/Icon.rc")
endif()
add_executable(${APP_SOURCE})

# Defome libraries to link
set(LIBRARIES ${PROJECT_NAME})
list(APPEND LIBRARIES GLAD)
if (${OperatingSystem} MATCHES "Win")
    list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.lib")
    list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.lib")
    list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/Libraries/GLFW/Lib/${OperatingSystem}/libglfw3.lib")
elseif (${OperatingSystem} MATCHES "Mac")
    list(APPEND LIBRARIES "/System/Library/Frameworks/Cocoa.framework")
    list(APPEND LIBRARIES "/System/Library/Frameworks/IOKit.framework")
    list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/Libraries/GLFW/Lib/${OperatingSystem}/libglfw3.a")
elseif (${OperatingSystem} MATCHES "Linux")
    list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.so")
    list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.so")
    list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/Libraries/GLFW/Lib/${OperatingSystem}/libglfw3.a")
endif()
target_link_libraries(${LIBRARIES}) 

# Copy dependancies to app folder
if (${OperatingSystem} MATCHES "Win")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.dll" "${PROJECT_BINARY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.dll" "${PROJECT_BINARY_DIR}"
        COMMAND_EXPAND_LISTS
    )
elseif (${OperatingSystem} MATCHES "Mac")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.dylib" "${PROJECT_BINARY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.dylib" "${PROJECT_BINARY_DIR}"
        COMMAND_EXPAND_LISTS
    )
elseif (${OperatingSystem} MATCHES "Linux")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.so" "${PROJECT_BINARY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.so" "${PROJECT_BINARY_DIR}"
        COMMAND_EXPAND_LISTS
    )
endif()

# CPack settings
include(CPack)
set(CPACK_PACKAGE_NAME "Nexgen Redux")
set(CPACK_PACKAGE_VENDOR "Team Resurgent")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Nexgen Redux Installer")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Nexgen Redux")
set(CPACK_CREATE_DESKTOP_LINKS "Nexgen Redux")
set(CPACK_NSIS_IGNORE_LICENSE_PAGE 1)
SET(CPACK_NSIS_MODIFY_PATH ON)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
if (${OperatingSystem} MATCHES "Win")
    install(FILES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.dll" DESTINATION bin)
    install(FILES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.dll" DESTINATION bin)
    set(CPACK_GENERATOR NSIS)
    set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/Artwork\\\\Icon.ico")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Nexgen Redux.lnk' '$INSTDIR\\\\NexgenRedux.exe' ''")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\\\$START_MENU\\\\Nexgen Redux.lnk'")
elseif (${OperatingSystem} MATCHES "Mac")
    install(FILES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.dylib" DESTINATION bin)
    install(FILES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.dylib" DESTINATION bin)    
elseif (${OperatingSystem} MATCHES "Linux")
    install(FILES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.so" DESTINATION bin)
    install(FILES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.so" DESTINATION bin)
    set(CPACK_GENERATOR TGZ)
endif()

# After build run CPack
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND "${CMAKE_CPACK_COMMAND}" "-C" "Release"
    COMMENT "Packing"
    DEPENDS ${PROJECT_NAME}
)