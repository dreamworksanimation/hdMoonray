# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(component hd_moonray)

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
        RendererPlugin.cc
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
        HdMoonray::hydramoonray
        ArrasCore::arras4_log
        ArrasCore::client_api
        ArrasCore::sdk
        ArrasCore::core_messages
        ArrasCore::message_api
        McrtDataio::client_receiver
        McrtMessages::mcrt_messages
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
            plugin/pxr/${component}
)


