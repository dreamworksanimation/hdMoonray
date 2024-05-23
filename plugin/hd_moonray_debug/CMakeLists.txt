# Copyright 2023 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(component hd_moonray_debug)

set(installIncludeDir ${PACKAGE_NAME}/${component})
set(exportGroup ${PROJECT_NAME}Targets)

add_library(${component} SHARED "")
add_library(${PROJECT_NAME}::${component} ALIAS ${component})

# no "lib" prefix on the plugin lib
set_target_properties(${component} PROPERTIES PREFIX "")

target_sources(${component}
    PRIVATE
        RndrRenderer.cc
        RendererPlugin.cc
)

target_include_directories(${component}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if (NOT IsDarwinPlatform)
    set(PlatformSpecificLibs atomic)
endif()

target_link_libraries(${component}
    PUBLIC
        HdMoonray::hydramoonray
        Moonray::rendering_rndr
        TBB::tbb
        hd   # pxr imaging
        usdImaging
        Python::Module
        ${PlatformSpecificLibs}
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

# install the plugin lib
# .so is installed under plugin, so that it can
# use the RUNPATH ../lib64

# install the target
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


