# The Xbox 360: code that uses the XDK's own headers, and linking a title (.xex) from the engine's libraries.
#
# The libraries are built here with RXDK-360's clang. The title is then linked by RXDK-360's MSBuild toolset ("clang",
# on the "Xbox 360" platform in Visual Studio 2022 or later), which resolves the kernel's imports and packs the .xex:
# a small project is written for it, and run by `cmake --build` like any other step.

set(RXDK360_XDK "${RXDK360_ROOT}/include/xbox" CACHE PATH "XDK headers")

# Sources that include <xtl.h>: compiled the way RXDK-360 compiles XDK code (ClangCompile.cs), on top of the
# engine's own flags.
function(nx_xdk_sources)
  set(_flags
    -fms-extensions -fms-compatibility -fdeclspec -fms-compatibility-version=1920
    -D_INTPTR_T_DEFINED -D_UINTPTR_T_DEFINED -D__gnuc_va_list=__builtin_va_list -D_HAS_CHAR16_T_LANGUAGE_SUPPORT=1
    -D__STDC_WANT_LIB_EXT1__=1 "-DDECLSPEC_UUID(x)=__declspec(uuid(x))" -D_MSC_FULL_VER=140050727
    -fno-autolink
    -Wno-invalid-token-paste -Wno-narrowing -Wno-switch -Wno-ignored-attributes -Wno-nonportable-include-path
    -Wno-microsoft-extra-qualification -Wno-ignored-pragma-intrinsic -Wno-nontrivial-memcall -Wno-delete-incomplete
    -Wno-tautological-constant-out-of-range-compare -Wno-unused-parameter -Wno-missing-field-initializers
    -Wno-unknown-pragmas -Wno-pragma-pack -Wno-microsoft-anon-tag -Wno-language-extension-token
    -D_WIN32=1 -D_M_PPCBE=1 -D_M_PPC=1 -D_XBOX=1 -D_XBOX_VER=200 -D__export= -D_SIZE_T_DEFINED -D_XM_NO_INTRINSICS_
    -D_MBCS -include stdint.h -include __stddef_max_align_t.h -include rxdk_msvcrt_compat.h
    -include rxdk_secure_overloads.h -isystem "${RXDK360_XDK}")
  set_source_files_properties(${ARGN} DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTIES COMPILE_OPTIONS "${_flags}")
endfunction()

# Visual Studio's MSBuild, which has the Xbox 360 platform once RXDK-360 is installed.
find_program(NX_MSBUILD MSBuild.exe
  PATHS "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/MSBuild/Current/Bin"
        "C:/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin"
        "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin"
        "C:/Program Files/Microsoft Visual Studio/18/Enterprise/MSBuild/Current/Bin"
        "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin"
  NO_DEFAULT_PATH)

# nx_xbox360_title(<library with main> <name> <title> <title id> <media folder>)
# Makes ${CMAKE_BINARY_DIR}/<name>/default.xex with the media beside it, as a disc has them: Xenia runs it from there.
function(nx_xbox360_title target name title id media)
  if(NOT NX_MSBUILD)
    message(WARNING "No MSBuild with the Xbox 360 platform: ${name}.xex will not be linked")
    return()
  endif()
  set(_libs $<TARGET_FILE:${target}>)
  foreach(_lib nx_screens nx_platform nx_scene nx_ui nx_input nx_net nx_tls nx_canvas nx_assets nx_audio nx_script nx_angelscript nx_gpu nx_base)
    list(APPEND _libs $<TARGET_FILE:${_lib}>)
  endforeach()
  set(_dir ${CMAKE_BINARY_DIR}/title_${name})
  set(_out ${CMAKE_BINARY_DIR}/${name})
  file(WRITE ${_dir}/start.cpp "// The title's code is all in its libraries; MSBuild wants something to compile.\n")
  file(GENERATE OUTPUT ${_dir}/default.vcxproj CONTENT "<?xml version=\"1.0\" encoding=\"utf-8\"?>
<Project DefaultTargets=\"Build\" ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">
  <ItemGroup Label=\"ProjectConfigurations\">
    <ProjectConfiguration Include=\"Release|Xbox 360\"><Configuration>Release</Configuration><Platform>Xbox 360</Platform></ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label=\"Globals\"><ProjectName>default</ProjectName><RootNamespace>default</RootNamespace></PropertyGroup>
  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />
  <PropertyGroup Label=\"Configuration\">
    <ConfigurationType>Application</ConfigurationType>
    <PlatformToolset>clang</PlatformToolset>
    <RxdkClangXdkHeaders>true</RxdkClangXdkHeaders>
    <RxdkClangLibs>$<JOIN:${_libs},;></RxdkClangLibs>
    <CharacterSet>MultiByte</CharacterSet>
    <OutDir>${_out}/</OutDir>
    <IntDir>${_dir}/obj/</IntDir>
  </PropertyGroup>
  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />
  <ItemDefinitionGroup>
    <ClCompile><PreprocessorDefinitions>NDEBUG;_XBOX;%(PreprocessorDefinitions)</PreprocessorDefinitions></ClCompile>
    <Link><AdditionalDependencies>d3d9.lib;d3dx9.lib;xgraphics.lib;xapilib.lib;xaudio2.lib;x3daudio.lib;xmcore.lib;xboxkrnl.lib;xbdm.lib;%(AdditionalDependencies)</AdditionalDependencies></Link>
    <ImageXex><TitleID>${id}</TitleID></ImageXex>
  </ItemDefinitionGroup>
  <ItemGroup><ClCompile Include=\"start.cpp\" /></ItemGroup>
  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />
  <!-- Built here, not sent to a console. -->
  <Target Name=\"Deploy\" />
</Project>
")
  add_custom_target(${target}_xex ALL
    COMMAND ${CMAKE_COMMAND} -E env RXDK360_CLANG=${RXDK360_DEV} ${NX_MSBUILD} ${_dir}/default.vcxproj -nologo -v:m -p:Configuration=Release "-p:Platform=Xbox 360"
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${media} ${_out}/media
    DEPENDS ${target} nx_screens nx_platform nx_gpu
    COMMENT "Linking ${name}.xex: ${title}"
    VERBATIM)
endfunction()
