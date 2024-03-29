cmake_minimum_required(VERSION 3.7)
set(NEXGEN_VERSION_MAJOR 1)
set(NEXGEN_VERSION_MINOR 0)
set(NEXGEN_VERSION_PATCH 0)
project(NexgenRedux VERSION ${NEXGEN_VERSION_MAJOR}.${NEXGEN_VERSION_MINOR}.${NEXGEN_VERSION_PATCH})

set(CMAKE_CXX_STANDARD 11)

# Detect operating system
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    SET(OperatingSystem "Win")
    add_compile_definitions(NEXGEN_WIN)
    add_compile_definitions(AS_NO_THREADS)
    add_compile_definitions(AS_USE_NAMESPACE)
    add_compile_definitions(UNICODE)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(AS_NO_MEMORY_H)

    enable_language (ASM_MASM)
    add_custom_command (OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/as_callfunc_x64_msvc_asm.obj COMMAND "ml64" /Fo ${CMAKE_CURRENT_BINARY_DIR}/as_callfunc_x64_msvc_asm.obj /c ${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_x64_msvc_asm.asm DEPENDS ${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_x64_msvc_asm.asm)       
    set (ANGELSCRIPT_ASM ${CMAKE_CURRENT_BINARY_DIR}/as_callfunc_x64_msvc_asm.obj ${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_x64_msvc_asm.asm)
    add_library(ANGELLIB STATIC ${ANGELSCRIPT_ASM})
    
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    SET(OperatingSystem "Mac")
    SET(CMAKE_OSX_DEPLOYMENT_TARGE "10.10")
    add_compile_definitions(NEXGEN_MAC)
    add_compile_definitions(AS_NO_THREADS)
    add_compile_definitions(AS_USE_NAMESPACE)
    add_compile_definitions(UNICODE)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SET(OperatingSystem "Linux")
    add_compile_definitions(NEXGEN_LINUX)
    add_compile_definitions(AS_NO_THREADS)
    add_compile_definitions(AS_USE_NAMESPACE)
    add_compile_definitions(UNICODE)
else()
    message(FATAL_ERROR "Unrecognized operating system '${CMAKE_SYSTEM_NAME}'")
endif()
message(STATUS "Nexgen Redux build for ${OperatingSystem} ${CMAKE_BUILD_TYPE}...")

# Build GLAD library
add_library(GLAD STATIC "${PROJECT_SOURCE_DIR}/Libraries/GLAD/Source/glad.c")

# Ensure includes visible to project
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

# Define include folders
set(INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/NexgenRedux-PC")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/Libraries/GLAD/Includes/")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/Libraries/GLFW/Includes/")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/Libraries/")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/")
list(APPEND INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/ECS")
include_directories(${INCLUDE_DIRS})

# Define source code files
set(APP_SOURCE ${PROJECT_NAME})
# Shared
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/AngelScriptRunner.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/MathUtility.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/MeshUtility.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/Icon.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/ConfigLoader.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/AngelScriptDebugger.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/AngelScriptDebuggerCore.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/AngelScriptMethods.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/AudioManager.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/WindowManager.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/XboxOGWindowHelper.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/OpenGLWindowHelper.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/XboxOGRenderingHelper.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/OpenGLRenderingHelper.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/RenderStateManager.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/TextureManager.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/EntityEngine/FontManager.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/EntityEngine/SceneManager.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/EntityEngine/NodeManager.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/EntityEngine/Node.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/EntityEngine/OrthoCamera.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/EntityEngine/PerspectiveCamera.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/EntityEngine/Fog.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/EntityEngine/Lighting.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/EntityEngine/Sprite.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/EntityEngine/Text.cpp")
# Gensys
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/Crc32.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/DebugUtility.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/Drive.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/DriveManager.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/FileSystem.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/FtpServer.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/Memory.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/NetworkManager.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/SocketUtility.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/StringUtility.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/TimeUtility.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/TreeNode.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/UniqueIdGenerator.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/VirtualFileSystem.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/Gensys/Test.cpp")
# Angel Script
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_atomic.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_builder.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_bytecode.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_arm.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_arm64.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_mips.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_ppc.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_ppc_64.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_sh4.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_x64_gcc.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_x64_mingw.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_x64_msvc.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_x86.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_callfunc_xenon.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_compiler.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_configgroup.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_context.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_datatype.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_gc.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_generic.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_globalproperty.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_memory.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_module.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_objecttype.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_outputbuffer.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_parser.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_restore.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_scriptcode.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_scriptengine.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_scriptfunction.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_scriptnode.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_scriptobject.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_string.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_string_util.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_thread.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_tokenizer.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_typeinfo.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/as_variablescope.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/contextmgr/contextmgr.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/debugger/debugger.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/scriptany/scriptany.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/scriptarray/scriptarray.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/scriptbuilder/scriptbuilder.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/scriptdictionary/scriptdictionary.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/scriptgrid/scriptgrid.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/scripthandle/scripthandle.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/scripthelper/scripthelper.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/scriptmath/scriptmath.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/scriptmath/scriptmathcomplex.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/scriptstdstring/scriptstdstring.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/scriptstdstring/scriptstdstring_utils.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/serializer/serializer.cpp")
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/AngelScript/addons/weakref/weakref.cpp")
# STB
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/STB/stb_vorbis.c")
# DocGen
if (${OperatingSystem} MATCHES "Win")
    list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Libraries/DocGen/docgen.cpp")
endif()
# Main
list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/NexgenRedux-PC/main.cpp")
if (${OperatingSystem} MATCHES "Win")
    list(APPEND APP_SOURCE "${PROJECT_SOURCE_DIR}/Artwork/Icon.rc")
endif()
add_executable(${APP_SOURCE})

# Define libraries to link
set(LIBRARIES ${PROJECT_NAME})
list(APPEND LIBRARIES GLAD)
if (${OperatingSystem} MATCHES "Win")
    list(APPEND LIBRARIES ANGELLIB)
    list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.lib")
    list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.lib")
    list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/Libraries/GLFW/Lib/${OperatingSystem}/libglfw3.lib")
    list(APPEND LIBRARIES "${PROJECT_SOURCE_DIR}/Libraries/DWMAPI/Lib/${OperatingSystem}/dwmapi.lib")
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

set_target_properties(NexgenRedux
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# Copy dependancies to app folder
if (${OperatingSystem} MATCHES "Win")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.dll" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libEGL.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.dll" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libGLESv2.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/Media" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Media/"
        COMMAND_EXPAND_LISTS
    )
elseif (${OperatingSystem} MATCHES "Mac")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.dylib" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libEGL.dylib"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.dylib" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libGLESv2.dylib"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/Media" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Media/"
        COMMAND_EXPAND_LISTS
    )
elseif (${OperatingSystem} MATCHES "Linux")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libEGL.so" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libEGL.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Libraries/Angle/Lib/${OperatingSystem}/libGLESv2.so" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libGLESv2.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/Artwork/Icon.png" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Icon.png"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/NexgenRedux-Shared/Media" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Media/"
        COMMAND_EXPAND_LISTS
    )
endif()

# Force POST_BUILD always
add_custom_target(ALWAYS_DO_POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E touch ${PROJECT_SOURCE_DIR}/NexgenRedux-PC/main.cpp
)

# Create install package
if (${CMAKE_BUILD_TYPE} MATCHES "Release")
    if (${OperatingSystem} MATCHES "Win")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${PROJECT_SOURCE_DIR}/Tools/InnoSetup/iscc.exe ${PROJECT_SOURCE_DIR}/Tools/InnoSetup/NexgenRedux.iss /DAppRepoPath=${PROJECT_SOURCE_DIR} /DAppVersion=1.0.0
        )
    elseif (${OperatingSystem} MATCHES "Mac")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${PROJECT_SOURCE_DIR}/Tools/Packages/packagesbuild --project ${PROJECT_SOURCE_DIR}/Tools/Packages/NexgenRedux.pkgproj
        )     
    elseif (${OperatingSystem} MATCHES "Linux")    
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND cd ${PROJECT_SOURCE_DIR}/build/bin/Release && zip -r ${PROJECT_SOURCE_DIR}/build/NexgenRedux-Linux.zip Icon.png libEGL.so libGLESv2.so NexgenRedux Media
        )   
    endif()
else()
    message(NOTICE "${CMAKE_BUILD_TYPE} build skipping packaging.")
endif()

add_dependencies(${PROJECT_NAME} ALWAYS_DO_POST_BUILD)
