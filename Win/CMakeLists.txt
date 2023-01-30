cmake_minimum_required(VERSION 3.7)
project(Nexgen VERSION 0.1.0)

message(STATUS "Nexgen Redux Build...")

include(CTest)
enable_testing()

add_library(GLAD STATIC "${PROJECT_SOURCE_DIR}/../Libraries/GLAD/Source/glad.c")

add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

add_executable(${PROJECT_NAME} 
    main.cpp 
)

#set(OpenGL_GL_PREFERENCE "GLVND")
#find_package(OpenGL REQUIRED)

set(INCLUDE_DIRS ${PROJECT_NAME})
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/../Libraries/GLAD/Includes/")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/../Libraries/GLFW/Includes/")
include_directories(${INCLUDE_DIRS})

set(LIBRARIES ${PROJECT_NAME})
list(APPEND LIBRARIES GLAD)
list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/../Libraries/Angle/Lib/Win/libEGL.lib")
list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/../Libraries/Angle/Lib/Win/libGLESv2.lib")
list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/../Libraries/GLFW/Lib/Win/glfw3dll.lib")
target_link_libraries(${LIBRARIES}) 

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
