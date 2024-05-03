# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(target hd_render)

add_executable(${target})

target_sources(${target}
    PRIVATE
        FreeCamera.cc
        hd_render.cc
        OutputFile.cc
        RenderOptions.cc
        RenderSettings.cc
        SceneDelegate.cc
)

if (NOT IsDarwinPlatform)
    set(PlatformSpecificLibs atomic)
endif()

target_link_libraries(${target}
    PRIVATE
        SceneRdl2::scene_rdl2
        OpenImageIO::OpenImageIO
        JPEG::JPEG

        # pxr libs
        gf tf trace vt work   # base
        cameraUtil hd hdx hf  # imaging
        sdf usd usdGeom       # usd
        usdImaging

        TBB::tbb
        Python::Python
        Boost::${BOOST_PYTHON_COMPONENT_NAME}
        ${PlatformSpecificLibs}
        ${MKL}
)

# Set standard compile/link options
HdMoonray_cxx_compile_definitions(${target})
HdMoonray_cxx_compile_features(${target})
HdMoonray_cxx_compile_options(${target})
HdMoonray_link_options(${target})

install(TARGETS ${target}
    RUNTIME DESTINATION bin)
