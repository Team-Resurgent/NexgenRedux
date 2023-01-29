cmake_minimum_required(VERSION 3.7)
project(Nexgen VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(Nexgen 
    main.cpp 
)

set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)

message(STATUS "Looking ${PROJECT_SOURCE_DIR}/../Libraries/GLFW/Lib/Linux/libglfw3.a")
include_directories(${PROJECT_NAME}
    "${PROJECT_SOURCE_DIR}/../Libraries/GLFW/Includes/"
    "${PROJECT_SOURCE_DIR}/../Libraries/Angle/Includes/"
)

find_library(GLFW_LOCATION libglfw3.a HINTS "${PROJECT_SOURCE_DIR}/../Libraries/GLFW/Lib/Linux/")
#find_library(ANGLE_LOCATION libEGL.so libGLESv2.so HINTS "${PROJECT_SOURCE_DIR}/../Libraries/Angle/Lib/Linux/")
target_link_libraries(${PROJECT_NAME} 
PUBLIC ${GLFW_LOCATION} 
#PUBLIC ${ANGLE_LOCATION} 
${OPENGL_LIBRARIES}) 

# target_link_libraries(${PROJECT_NAME}
#     "${PROJECT_SOURCE_DIR}/../Libraries/GLFW/Lib/Linux/libglfw3.a"
#     SHARED "${PROJECT_SOURCE_DIR}/../Libraries/Angle/Lib/Linux/libEGL.so"
#     SHARED "${PROJECT_SOURCE_DIR}/../Libraries/Angle/Lib/Linux/libGLESv2.so"
# )

add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
