// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vector>
#include <scene_rdl2/common/fb_util/VariablePixelBuffer.h>

namespace hdMoonray {

/// Structure shared by Renderer and RenderBuffer, used to return pointers to pixel
/// buffers from Renderer to hdMoonray.

struct PixelSize {
    unsigned mChannels = 0; // 1-4 for N floats. Other numbers are reserved for other data types
    unsigned mWidth = 1; // number of pixels wide
    unsigned mHeight = 1; // number of pixels high
};

struct PixelData: public PixelSize {
    void* mData = nullptr; // points at first byte of lower-left pixel
    unsigned filmActivity = ~0; // renderer can use this to check if image has changed
    std::vector<float> vec; // renderer can use this to store the data (but it does not have to)
    scene_rdl2::fb_util::VariablePixelBuffer vpb; // renderer can use this to store the data also
};

}


