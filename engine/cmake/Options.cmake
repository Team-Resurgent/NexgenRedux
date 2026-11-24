# Engine code: no exceptions, no RTTI, warnings are errors.
function(nx_options target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /GR- /utf-8)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX _HAS_EXCEPTIONS=0)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror $<$<CXX_COMPILER_ID:GNU>:-Wno-format-truncation> -fno-exceptions $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
  endif()
  if(XBOXOG)
    # The SDK's own headers change the packing and leave it changed for a moment.
    target_compile_options(${target} PRIVATE -Wno-pragma-pack)
  endif()
endfunction()

# Copies a media folder next to the executable on desktop. Consoles package it with the title.
function(nx_media target folder)
  if(NOT XBOXOG)
    get_target_property(_bundle ${target} MACOSX_BUNDLE)
    if(APPLE AND _bundle)
      set(_media "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/media")
    else()
      set(_media "$<TARGET_FILE_DIR:${target}>/media")
    endif()
    # Every build, as scripts change without anything being compiled.
    add_custom_target(${target}_media COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/${folder}" "${_media}" VERBATIM)
    add_dependencies(${target} ${target}_media)
  endif()
endfunction()

# For third-party code compiled as part of one of our libraries.
function(nx_no_warnings file)
  if(MSVC)
    set_source_files_properties(${file} PROPERTIES COMPILE_OPTIONS "/W0;/wd4146;/wd4701;/wd4703")
  else()
    set_source_files_properties(${file} PROPERTIES COMPILE_OPTIONS "-w")
  endif()
endfunction()
