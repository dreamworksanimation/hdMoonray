# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(soho)

install(DIRECTORY . 
        DESTINATION houdini
        FILES_MATCHING PATTERN "*.json"
)
