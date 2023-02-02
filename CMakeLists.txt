cmake_minimum_required(VERSION 3.7)
set(NEXGEN_VERSION_MAJOR 1)
set(NEXGEN_VERSION_MINOR 0)
set(NEXGEN_VERSION_PATCH 0)
project(NexgenRedux VERSION ${NEXGEN_VERSION_MAJOR}.${NEXGEN_VERSION_MINOR}.${NEXGEN_VERSION_PATCH})

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
message(STATUS "Nexgen Redux build for ${OperatingSystem} ${CMAKE_BUILD_TYPE}...")

# Build GLAD library
add_library(GLAD STATIC "${PROJECT_SOURCE_DIR}/Libraries/GLAD/Source/glad.c")

# Ensure includes visible to project
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

# Define include folders
set(INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/NexgenRedux")
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

# Define libraries to link
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

# Force POST_BUILD always
add_custom_target(ALWAYS_DO_POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E touch ${PROJECT_SOURCE_DIR}/NexgenRedux/Icon.h
)

# Create install package
if (${CMAKE_BUILD_TYPE} MATCHES "Release")
    if (${OperatingSystem} MATCHES "Win")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${PROJECT_SOURCE_DIR}/Tools/InnoSetup/iscc.exe ${PROJECT_SOURCE_DIR}/Tools/InnoSetup/NexgenRedux.iss /DAppRepoPath=${PROJECT_SOURCE_DIR} /DAppVersion=1.0.0
        )
    elseif (${OperatingSystem} MATCHES "Mac")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${PROJECT_SOURCE_DIR}/Tools/Packages/packagesbuild ${PROJECT_SOURCE_DIR}/Tools/Packages/NexgenRedux.pkgproj
        )     
    elseif (${OperatingSystem} MATCHES "Linux")    

    endif()
endif()

add_dependencies(${PROJECT_NAME} ALWAYS_DO_POST_BUILD)

# CPack settings
# include(CPack)
# install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
# if (${OperatingSystem} MATCHES "Win")
#     install(FILES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.dll" DESTINATION bin)
#     install(FILES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.dll" DESTINATION bin)
#     install(FILES "${PROJECT_SOURCE_DIR}/Libraries/VisualC/VC_redist.x64.exe" DESTINATION bin)
#     set(CPACK_GENERATOR NSIS)
#     #set(CPACK_PACKAGE_INSTALL_DIRECTORY "Nexgen Redux")
#     #set(CPACK_NSIS_IGNORE_LICENSE_PAGE 1)
#     set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/Artwork\\\\Icon.ico")
#     set(CPACK_NSIS_MUI_UNIICON "${PROJECT_SOURCE_DIR}/Artwork\\\\Icon.ico")
#     set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Nexgen Redux.lnk' '$INSTDIR\\\\bin\\\\NexgenRedux.exe' ''")
#     set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\\\$START_MENU\\\\Nexgen Redux.lnk'")
#     set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "ExecWait '\\\\$INSTDIR\\\\bin\\\\VC_redist.x64.exe' ''")
#     set(CPACK_NSIS_MODIFY_PATH "ON")
#     set(CPACK_VERBATIM_VARIABLES TRUE)
#     set(CPACK_PACKAGE_FILE_NAME \"yourpackagename_${BUILD_TYPE}\")
#     #set(CPACK_SOURCE_PACKAGE_FILE_NAME "Nex")
# elseif (${OperatingSystem} MATCHES "Mac")
#     install(FILES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.dylib" DESTINATION bin)
#     install(FILES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.dylib" DESTINATION bin)    
# elseif (${OperatingSystem} MATCHES "Linux")
#     install(FILES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.so" DESTINATION bin)
#     install(FILES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.so" DESTINATION bin)
#     set(CPACK_GENERATOR TGZ)
# endif()

# # After build run CPack
# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#     COMMAND "${CMAKE_CPACK_COMMAND}" "-C" "Release"
#     COMMENT "Packing"
#     DEPENDS ${PROJECT_NAME}
# )