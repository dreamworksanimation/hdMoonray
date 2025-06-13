# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(component hdMoonrayAdapters)

set(installIncludeDir ${PACKAGE_NAME}/hdMoonray)
set(exportGroup ${PROJECT_NAME}Targets)

add_library(${component} SHARED "")
add_library(${PROJECT_NAME}::${component} ALIAS ${component})

# no "lib" prefix on the plugin lib
set_target_properties(${component} PROPERTIES PREFIX "")

# Custom DWA source file
if ("$ENV{STUDIO}" STREQUAL "GLD")
    set(DwaSpecificSources ProceduralAdapter.cc)
endif()

target_sources(${component}
    PRIVATE
        GeometryLightAdapter.cc
        MoonrayLightFilterAdapter.cc
        ${DwaSpecificSources}
)

target_include_directories(${component}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${component}
    PUBLIC
        usdImaging

# these libraries are required by USD, but we don't
# currently have a USD config with the dependencies linked
        Python::Module
        Boost::headers
        TBB::tbb

)

# If at Dreamworks add a SConscript stub file so others can use this library.
SConscript_Stub(${component})

# Set standard compile/link options
HdMoonray_cxx_compile_definitions(${component})
HdMoonray_cxx_compile_features(${component})
HdMoonray_cxx_compile_options(${component})
HdMoonray_link_options(${component})

# Configure plugInfo.json file
set(plugInfoTemplate ${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.json.in)
set(plugInfoFile ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json)
set(LIBRARY_PATH ../../${component}${CMAKE_SHARED_LIBRARY_SUFFIX})
configure_file(${plugInfoTemplate} ${plugInfoFile})

# -------------------------------------
# Install the target and the export set
# -------------------------------------
include(GNUInstallDirs)

# install the target
# .so is installed under plugin, so that it can
# use the RUNPATH ../lib64
install(TARGETS ${component}
    COMPONENT ${component}
    EXPORT ${exportGroup}
    LIBRARY
        DESTINATION plugin
        NAMELINK_SKIP
    RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${installIncludeDir}
)

# install main plugInfo
INSTALL(FILES ${plugInfoFile}
    DESTINATION plugin/pxr/${component}
)
