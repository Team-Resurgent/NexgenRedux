cmake_minimum_required(VERSION 3.7)
project(Nexgen VERSION 0.1.0)

message(STATUS "NexgenRedux Build...")

add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
include(CTest)
enable_testing()

add_executable(${PROJECT_NAME} 
    main.cpp 
)

set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

set(INCLUDE_DIRS ${PROJECT_NAME})
#list(APPEND INCLUDE_DIRS ${OPENGL_INCLUDE_DIR})
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/../Libraries/Angle/Includes/")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/../Libraries/GLFW/Includes/")
include_directories(${INCLUDE_DIRS})

set(LIBRARIES ${PROJECT_NAME})
#list(APPEND LIBRARIES ${OPENGL_LIBRARIES})
list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/../Libraries/Angle/Lib/Linux/libEGL.so")
list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/../Libraries/Angle/Lib/Linux/libGLESv2.so")
list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/../Libraries/GLFW/Lib/Linux/libglfw3.a")
target_link_libraries(${LIBRARIES}) 

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
