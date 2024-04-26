# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.json
        DESTINATION plugin/pxr)

add_subdirectory(hd_moonray)
add_subdirectory(hd_moonray_debug)
add_subdirectory(houdini)
add_subdirectory(adapters)
