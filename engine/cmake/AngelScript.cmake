# AngelScript, built from the unmodified copy in third_party. Platform differences are settings
# here, so the upstream files never need editing.

set(_as ${CMAKE_CURRENT_SOURCE_DIR}/third_party/angelscript/sdk)
file(GLOB _as_core CONFIGURE_DEPENDS ${_as}/angelscript/source/*.cpp)
set(_as_addons
  ${_as}/add_on/scriptarray/scriptarray.cpp
  ${_as}/add_on/scriptmath/scriptmath.cpp
  ${_as}/add_on/scriptstdstring/scriptstdstring.cpp
  ${_as}/add_on/scriptstdstring/scriptstdstring_utils.cpp)

add_library(nx_angelscript STATIC ${_as_core} ${_as_addons})
target_include_directories(nx_angelscript SYSTEM PUBLIC ${_as}/angelscript/include ${_as}/add_on)
target_compile_definitions(nx_angelscript PUBLIC AS_NO_THREADS AS_NO_EXCEPTIONS)

# Windows on Arm: AngelScript's calls straight into native code aren't finished for MSVC there (as_config.h says so, and
# they crash), so scripts call the engine the portable way, as every platform can.
if(MSVC AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "ARM64")
  set(NX_SCRIPT_GENERIC ON)
endif()

if(NX_SCRIPT_GENERIC)
  target_compile_definitions(nx_angelscript PUBLIC NX_SCRIPT_GENERIC AS_MAX_PORTABILITY)
elseif(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
  enable_language(ASM_MASM)
  target_sources(nx_angelscript PRIVATE ${_as}/angelscript/source/as_callfunc_x64_msvc_asm.asm)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
  enable_language(ASM)
  if(APPLE)
    target_sources(nx_angelscript PRIVATE ${_as}/angelscript/source/as_callfunc_arm64_xcode.S)
  else()
    target_sources(nx_angelscript PRIVATE ${_as}/angelscript/source/as_callfunc_arm64_gcc.S)
  endif()
endif()

if(MSVC)
  # Not for the assembly, whose assemblers take other switches.
  target_compile_options(nx_angelscript PRIVATE "$<$<COMPILE_LANGUAGE:C,CXX>:/W0;/utf-8>")
  target_compile_definitions(nx_angelscript PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  # It casts between pointer types freely.
  target_compile_options(nx_angelscript PRIVATE -w -fno-strict-aliasing)
endif()

if(XBOXOG)
  # RXDK's clang hides __GNUC__, __MINGW32__ and _WIN32, so AngelScript recognises no platform and
  # doesn't compile. Its calling convention is MinGW's on 32-bit x86, which __CYGWIN__ selects
  # without also turning on Win32 code.
  target_compile_definitions(nx_angelscript PUBLIC __GNUC__=4 __GNUC_MINOR__=8 __CYGWIN__)
  # Its inline assembly emits unwind directives when optimised, which need unwind tables that this
  # target doesn't produce. Keeping the frame pointer makes them unnecessary.
  set_source_files_properties(${_as}/angelscript/source/as_callfunc_x86.cpp PROPERTIES
    COMPILE_OPTIONS "-U__OPTIMIZE__;-fno-omit-frame-pointer")
endif()

if(XBOX360)
  # Both are 32 bits here but not the same type (one unsigned int, one unsigned long), and AngelScript
  # takes uintptr_t and size_t to be the same one.
  target_compile_options(nx_angelscript PUBLIC -U__UINTPTR_TYPE__ -D__UINTPTR_TYPE__=__SIZE_TYPE__)
endif()
