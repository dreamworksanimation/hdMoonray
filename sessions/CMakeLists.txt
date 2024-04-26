# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

install(DIRECTORY . 
        DESTINATION sessions 
        FILES_MATCHING PATTERN "*.sessiondef"
)

install(DIRECTORY dwa
        DESTINATION sessions/dwa
        FILES_MATCHING PATTERN "*.sessiondef"
)
