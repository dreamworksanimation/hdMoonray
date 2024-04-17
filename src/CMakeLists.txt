# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(adapters)

set(component hdMoonray)

set(installIncludeDir ${PACKAGE_NAME}/${component})
set(exportGroup ${PROJECT_NAME}Targets)

add_library(${component} SHARED "")
add_library(${PROJECT_NAME}::${component} ALIAS ${component})

# no "lib" prefix on the plugin lib
set_target_properties(${component} PROPERTIES PREFIX "")

target_sources(${component}
    PRIVATE
        ArrasRenderer.cc
        ArrasSettings.cc
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
        NullRenderer.cc
        Points.cc
        Procedural.cc
        RenderBuffer.cc
        RenderDelegate.cc
        RendererPlugin.cc
        RenderPass.cc
        RenderSettings.cc
        RndrRenderer.cc
        ValueConverter.cc
        Volume.cc
)

target_include_directories(${component}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set(atomicLib "")
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(atomicLib atomic)
endif()

target_link_libraries(${component}
    PUBLIC
        ArrasCore::arras4_log
        ArrasCore::client_api
        ArrasCore::sdk
        ArrasCore::core_messages
        ArrasCore::message_api
        SceneRdl2::common_except
        SceneRdl2::render_logging
        SceneRdl2::scene_rdl2
        McrtDataio::client_receiver
        McrtMessages::mcrt_messages
        Moonray::rendering_rndr
        TBB::tbb
        hd   # pxr imaging
        usdImaging
        Python::Module
        ${atomicLib}
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
INSTALL(FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.json
        DESTINATION
            plugin
)

# install the export set
install(
    EXPORT ${exportGroup}
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
)


