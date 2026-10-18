cmake_minimum_required(VERSION 3.16)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h" _version_line REGEX "^#define PORT_VERSION")
string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" _port_version "${_version_line}")
project(AlfabetSmierci VERSION "${_port_version}" LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ALF_STATIC_THIRD_PARTY "Link the bundled static SDL3 and libopenmpt build" OFF)
set(ALF_DEPS_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/deps/linux-install-ubuntu22.04-x86_64" CACHE PATH
    "Prefix containing static SDL3 and libopenmpt for the portable Linux package")
if(ALF_STATIC_THIRD_PARTY)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "ALF_STATIC_THIRD_PARTY is currently supported only on Linux")
    endif()
    enable_language(CXX)
    find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3-static
        PATHS "${ALF_DEPS_PREFIX}/lib/cmake/SDL3" NO_DEFAULT_PATH)
    find_library(OPENMPT_STATIC_LIBRARY NAMES openmpt
        PATHS "${ALF_DEPS_PREFIX}/lib" NO_DEFAULT_PATH)
    find_path(OPENMPT_INCLUDE_DIR NAMES libopenmpt/libopenmpt.h
        PATHS "${ALF_DEPS_PREFIX}/include" NO_DEFAULT_PATH)
    if(NOT OPENMPT_STATIC_LIBRARY OR NOT OPENMPT_INCLUDE_DIR)
        message(FATAL_ERROR "Static libopenmpt was not found under ALF_DEPS_PREFIX=${ALF_DEPS_PREFIX}")
    endif()
    add_library(OpenMPT::Static UNKNOWN IMPORTED)
    set_target_properties(OpenMPT::Static PROPERTIES
        IMPORTED_LOCATION "${OPENMPT_STATIC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENMPT_INCLUDE_DIR}")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
    pkg_check_modules(OPENMPT REQUIRED IMPORTED_TARGET libopenmpt)
endif()

file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
if(APPLE)
    add_executable(alfabet_smierci MACOSX_BUNDLE ${ENGINE_SOURCES})
    set_target_properties(alfabet_smierci PROPERTIES
        OUTPUT_NAME "AlfabetSmierci"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Alfabet Smierci"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.peteroman.alfabetsmierci"
        MACOSX_BUNDLE_INFO_STRING "Alfabet Śmierci, port ${PROJECT_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "(C) 1995/1996 Arte / Seven Stars Multimedia; port (C) 2026 Piotr 'Pete' Romańczyk")
    set(_resource_dir "$<TARGET_BUNDLE_DIR:alfabet_smierci>/Contents/Resources")
else()
    add_executable(alfabet_smierci ${ENGINE_SOURCES})
    set_target_properties(alfabet_smierci PROPERTIES
        OUTPUT_NAME "AlfabetSmierci"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/AlfabetSmierci")
    set(_resource_dir "$<TARGET_FILE_DIR:alfabet_smierci>")
endif()

target_compile_options(alfabet_smierci PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_compile_definitions(alfabet_smierci PRIVATE ALF_NATIVE_BUILD=1)
if(ALF_STATIC_THIRD_PARTY)
    target_link_libraries(alfabet_smierci PRIVATE SDL3::SDL3-static OpenMPT::Static m pthread)
    set_property(TARGET alfabet_smierci PROPERTY LINKER_LANGUAGE CXX)
    target_link_options(alfabet_smierci PRIVATE -static-libstdc++ -static-libgcc)
else()
    target_link_libraries(alfabet_smierci PRIVATE PkgConfig::SDL3 PkgConfig::OPENMPT)
endif()

# Keep runtime assets with the executable on Linux and in Resources for a macOS app bundle.
add_custom_target(alfabet_assets ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/AlfabetSmierci/data" "${_resource_dir}/data"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts" "${_resource_dir}/data/scripts"
    VERBATIM)
add_dependencies(alfabet_assets alfabet_smierci)
