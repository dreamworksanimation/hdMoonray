# Copyright 2023 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(component hydramoonray)

set(buildIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(installIncludeDir ${PACKAGE_NAME}/${component})
set(exportGroup ${PROJECT_NAME}Targets)

add_library(${component} SHARED "")
add_library(${PROJECT_NAME}::${component} ALIAS ${component})

target_sources(${component}
    PRIVATE
        BasisCurves.cc
        Camera.cc
        CoordSys.cc
        Geometry.cc
        HdmLog.cc
        Instancer.cc
        Light.cc
        LightFilter.cc
        Material.cc
        Mesh.cc
        Points.cc
        Procedural.cc
        RenderBuffer.cc
        RenderDelegate.cc
        RenderPass.cc
        RenderSettings.cc
        ValueConverter.cc
        Volume.cc
)

set_property(TARGET ${component}
    PROPERTY PUBLIC_HEADER
        BasisCurves.h
        Camera.h
        CoordSys.h
        Geometry.h
        HdmLog.h
        Instancer.h
        Light.h
        LightFilter.h
        Material.h
        Mesh.h
        PixelData.h
        Points.h
        Procedural.h
        RenderBuffer.h
        RenderDelegate.h
        Renderer.h
        RenderPass.h
        RenderSettings.h
        ValueConverter.h
        Volume.h
)

target_include_directories(${component}
    PUBLIC
        $<BUILD_INTERFACE:${buildIncludeDir}>
        $<INSTALL_INTERFACE:include>
)

if (NOT IsDarwinPlatform)
    set(PlatformSpecificLibs atomic)
endif()

target_link_libraries(${component}
    PUBLIC
        SceneRdl2::common_except
        SceneRdl2::render_logging
        SceneRdl2::scene_rdl2
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
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        NAMELINK_SKIP
    RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${installIncludeDir}
)



