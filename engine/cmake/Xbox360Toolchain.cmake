# CMake toolchain for the Xbox 360, using RXDK-360's clang. Flags follow its Visual Studio toolset
# (ClangCompile.cs). The libraries are linked into a title by RXDK-360's own MSBuild steps; see platform/xbox360.
#
#   cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=engine/cmake/Xbox360Toolchain.cmake -S engine -B out

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ppc64)
set(XBOX360 TRUE)

if(NOT RXDK360_ROOT)
  set(RXDK360_ROOT "C:/Program Files/RXDK-360")
endif()
set(RXDK360_ROOT "${RXDK360_ROOT}" CACHE PATH "RXDK-360 install folder")
set(_clang "${RXDK360_ROOT}/bin/clang")
if(NOT RXDK360_CC)
  set(RXDK360_CC "${_clang}/bin/clang.exe")
endif()
set(RXDK360_CC "${RXDK360_CC}" CACHE FILEPATH "clang to use, for example one built in the RXDK-360 repository")
if(NOT EXISTS "${RXDK360_CC}")
  message(FATAL_ERROR "RXDK-360 clang not found: ${RXDK360_CC}. Set RXDK360_ROOT or RXDK360_CC.")
endif()
# RXDK-360 releases before November 2026 crash on this target and need powerpc-unknown-xbox360.
if(NOT RXDK360_TARGET)
  set(RXDK360_TARGET powerpc64-unknown-xbox360)
endif()
set(RXDK360_TARGET "${RXDK360_TARGET}" CACHE STRING "clang target")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES RXDK360_ROOT RXDK360_CC RXDK360_TARGET)

set(CMAKE_C_COMPILER "${RXDK360_CC}")
set(CMAKE_CXX_COMPILER "${RXDK360_CC}")
set(CMAKE_AR "${_clang}/bin/llvm-ar.exe" CACHE FILEPATH "")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# llvm-ar writes the index itself, so no ranlib step (the toolset ships none).
foreach(_lang C CXX)
  set(CMAKE_${_lang}_CREATE_STATIC_LIBRARY "<CMAKE_AR> rcs <TARGET> <OBJECTS>")
  set(CMAKE_${_lang}_ARCHIVE_CREATE "<CMAKE_AR> rcs <TARGET> <OBJECTS>")
  set(CMAKE_${_lang}_ARCHIVE_APPEND "<CMAKE_AR> rs <TARGET> <OBJECTS>")
  set(CMAKE_${_lang}_ARCHIVE_FINISH "")
endforeach()

# In the flags because CMake does not pass CMAKE_<LANG>_COMPILER_TARGET on for this compiler.
set(_common "--target=${RXDK360_TARGET} -nostdlibinc -fshort-wchar -ffunction-sections -fdata-sections -D__Picolibc__ -D_GNU_SOURCE")
# A clang built in an RXDK-360 source tree (its build/llvm/bin) comes with that tree's headers and runtime, which
# the title is linked with, so those are used; otherwise the installed ones.
get_filename_component(_dev "${RXDK360_CC}/../../../.." ABSOLUTE)
if(EXISTS "${_dev}/runtime/config" AND EXISTS "${_dev}/build/llvm/libcxx/include")
  set(RXDK360_DEV "${_dev}" CACHE PATH "RXDK-360 source tree the compiler is from")
  set(_config "${_dev}/runtime/config")
  set(_picolibc "${_dev}/vendor/picolibc/libc/include")
  set(_libcxx "${_dev}/build/llvm/libcxx/include")
  set(_libcxxabi "${_dev}/build/llvm/libcxxabi/include")
else()
  set(_config "${_clang}/include/config")
  set(_picolibc "${_clang}/include/picolibc")
  set(_libcxx "${_clang}/include/libcxx")
  set(_libcxxabi "${_clang}/include/libcxxabi")
endif()
set(RXDK360_CONFIG_DIR "${_config}" CACHE INTERNAL "")
set(_c_dirs "\"-I${_config}\" -include picolibc.h \"-I${_picolibc}\"")
set(_cxx_dirs "\"-I${_libcxx}\" \"-I${_libcxxabi}\"")
set(CMAKE_C_FLAGS_INIT "${_common} ${_c_dirs}")
set(CMAKE_CXX_FLAGS_INIT "${_common} ${_cxx_dirs} -include __config_site -include rxdk_libcpp_prereq.h ${_c_dirs}")

# So third-party headers can be marked as such (no warnings from them).
set(CMAKE_INCLUDE_SYSTEM_FLAG_C "-isystem ")
set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem ")

# As the Visual Studio toolset. CMake adds none of its own for this compiler.
foreach(_lang C CXX)
  set(CMAKE_${_lang}_FLAGS_DEBUG_INIT "-O0 -gdwarf-4")
  set(CMAKE_${_lang}_FLAGS_RELEASE_INIT "-O2")
  set(CMAKE_${_lang}_FLAGS_RELWITHDEBINFO_INIT "-O2 -gdwarf-4")
  set(CMAKE_${_lang}_FLAGS_MINSIZEREL_INIT "-Os")
endforeach()
