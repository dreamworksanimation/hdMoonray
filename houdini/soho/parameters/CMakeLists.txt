# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

install(DIRECTORY . 
        DESTINATION houdini/soho/parameters 
        FILES_MATCHING PATTERN "*.ds"
)
