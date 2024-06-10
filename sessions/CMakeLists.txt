# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

# Configure sessiondef files so the library names are OS appropriate
set(COMPUTATION_PROGMCRT_LIB ${CMAKE_SHARED_LIBRARY_PREFIX}computation_progmcrt${CMAKE_SHARED_LIBRARY_SUFFIX})
set(COMPUTATION_PROGMCRT_DISPATCH_LIB ${CMAKE_SHARED_LIBRARY_PREFIX}computation_progmcrt_dispatch${CMAKE_SHARED_LIBRARY_SUFFIX})
set(COMPUTATION_PROGMCRT_MERGE_LIB ${CMAKE_SHARED_LIBRARY_PREFIX}computation_progmcrt_merge${CMAKE_SHARED_LIBRARY_SUFFIX})
configure_file(hd_single.sessiondef.in hd_single.sessiondef)
configure_file(hd_multi.sessiondef.in hd_multi.sessiondef)
configure_file(dwa/hd_single.sessiondef.in dwa/hd_single.sessiondef)
configure_file(dwa/hd_multi.sessiondef.in dwa/hd_multi.sessiondef)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DESTINATION .
        FILES_MATCHING PATTERN "*.sessiondef"
)

