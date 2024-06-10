# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required (VERSION 3.23.1)

include(OMR_PackageVersion)  # Sets versionString, projectString and PACKAGE_NAME
project(${projectString}
    VERSION ${versionString}
    LANGUAGES CXX C)

string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)
list(APPEND CMAKE_MESSAGE_CONTEXT ${PROJECT_NAME})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(OMR_Platform)

# ================================================
# Find dependencies
# ================================================

find_package(Python REQUIRED COMPONENTS Interpreter Development)

set(BUILD_SHARED_LIBS ON)

# the DWA installation of pxrConfig.cmake lives in
# the usd_imaging Rez package. To work correctly, it
# requires env vars REZ_USD_CORE_ROOT, REZ_USD_IMAGING_ROOT
# and PXR_INCLUDE_DIRS to be set (e.g. in CMake preset).
# A standard installation should not require these...
if(NOT pxr_FOUND)
    find_package(pxr REQUIRED)
endif()

find_package(JPEG REQUIRED)
find_package(OpenImageIO REQUIRED)

# Our USD config doesn't specify TBB or Boost dependencies,
# so we need to pull the in explicitly
find_package(TBB REQUIRED)

if (NOT DEFINED BOOST_PYTHON_COMPONENT_NAME)
    # may need to be, e.g. python36, python39
    set(BOOST_PYTHON_COMPONENT_NAME python)
endif()

set(BUILD_SHARED_LIBS ON)
find_package(Boost REQUIRED COMPONENTS "${BOOST_PYTHON_COMPONENT_NAME}")

if("${PROJECT_NAME}" STREQUAL "${CMAKE_PROJECT_NAME}")
    find_package(ArrasCore REQUIRED)
    find_package(McrtDataio REQUIRED)
    find_package(Moonshine REQUIRED)
    find_package(Moonray REQUIRED)
    find_package(SceneRdl2 REQUIRED)
endif()

# Set the RPATH for binaries in the install tree
set(CMAKE_INSTALL_RPATH ${GLOBAL_INSTALL_RPATH})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# if OPT_LEVEL is set use its value to override the CMAKE_BUILD_TYPE because the
# rez cmake plugin does not have an easy way to set the build_target.
set(opt_level $ENV{OPT_LEVEL})
if(opt_level STREQUAL opt-debug)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
elseif(opt_level STREQUAL debug)
    set(CMAKE_BUILD_TYPE Debug)
elseif(opt_level STREQUAL opt)
    set(CMAKE_BUILD_TYPE Release)
endif()

# default to Release if no build type specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Create include/${PACKAGE_NAME} link in the build directory for generated headers.
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/include)
file(CREATE_LINK ../lib ${PROJECT_BINARY_DIR}/include/${PACKAGE_NAME} SYMBOLIC)

include(${PROJECT_NAME}CompileDefinitions)
include(${PROJECT_NAME}CompileFeatures)
include(${PROJECT_NAME}CompileOptions)
include(${PROJECT_NAME}LinkOptions)
include(SConscriptStub)

# ================================================
# Add project files
# ================================================
add_subdirectory(lib)
add_subdirectory(plugin)
add_subdirectory(cmd)
add_subdirectory(sessions)

# ================================================
# Install
# ================================================
include(GNUInstallDirs)
install(EXPORT ${PROJECT_NAME}Targets
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
        NAMESPACE ${PROJECT_NAME}::)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
)

write_basic_package_version_file(
    ${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
)

install(
    DIRECTORY testSuite
    DESTINATION .
    USE_SOURCE_PERMISSIONS
)

