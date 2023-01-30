cmake_minimum_required(VERSION 3.7)
project(Nexgen VERSION 0.1.0)

message(STATUS "Nexgen Redux Build...")

add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
include(CTest)
enable_testing()

add_library(GLAD STATIC "${PROJECT_SOURCE_DIR}/../Libraries/GLAD/Source/glad.c")

add_executable(${PROJECT_NAME} 
    main.cpp 
)

set(INCLUDE_DIRS ${PROJECT_NAME})
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/../Libraries/GLAD/Includes/")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/../Libraries/GLFW/Includes/")
include_directories(${INCLUDE_DIRS})

set(LIBRARIES ${PROJECT_NAME})
list(APPEND LIBRARIES GLAD)
list(APPEND LIBRARIES "/System/Library/Frameworks/Cocoa.framework")
list(APPEND LIBRARIES "/System/Library/Frameworks/IOKit.framework")
list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/../Libraries/GLFW/Lib/Mac/libglfw3.a")
target_link_libraries(${LIBRARIES}) 

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/../Libraries/Angle/Lib/Mac/libEGL.dylib" "${CMAKE_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/../Libraries/Angle/Lib/Mac/libGLESv2.dylib" "${CMAKE_BINARY_DIR}"
    COMMAND_EXPAND_LISTS
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
