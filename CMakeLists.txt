cmake_minimum_required(VERSION 3.7)
project(NexgenRedux VERSION 0.1.0)

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

include(CTest)
enable_testing()

add_library(GLAD STATIC "${PROJECT_SOURCE_DIR}/Libraries/GLAD/Source/glad.c")

add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

add_executable(${PROJECT_NAME} 
    "${PROJECT_SOURCE_DIR}/NexgenRedux/main.cpp"
)

set(INCLUDE_DIRS "${PROJECT_NAME}/NexgenRedux")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/Libraries/GLAD/Includes/")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/Libraries/GLFW/Includes/")
include_directories(${INCLUDE_DIRS})

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

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
