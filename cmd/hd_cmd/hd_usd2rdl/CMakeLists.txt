# Copyright 2023 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(target hd_usd2rdl)

add_executable(${target})

target_sources(${target}
    PRIVATE
        FreeCamera.cc
        hd_usd2rdl.cc
        RenderOptions.cc
        RenderSettings.cc
        SceneDelegate.cc
)

target_link_libraries(${target}
    PRIVATE
        SceneRdl2::scene_rdl2
#        IlmBase::Half
#        OpenImageIO::OpenImageIO

        # pxr libs
        gf tf trace vt work   # base
        cameraUtil hd hdx hf  # imaging
        sdf usd usdGeom       # usd
        usdImaging

        TBB::tbb
        Python::Python
        Boost::${BOOST_PYTHON_COMPONENT_NAME}
        atomic
        ${MKL}
)

# Set standard compile/link options
HdMoonray_cxx_compile_definitions(${target})
HdMoonray_cxx_compile_features(${target})
HdMoonray_cxx_compile_options(${target})
HdMoonray_link_options(${target})

install(TARGETS ${target}
    RUNTIME DESTINATION bin)
