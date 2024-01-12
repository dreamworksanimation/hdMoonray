// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file OutputFile.h

#pragma once

#include <tbb/tbb_machine.h> // fix for icc-19/tbb bug
#include <pxr/imaging/hd/renderBuffer.h>

#include <string>

namespace hd_render {

// Output pixel data using openimageio
class OutputFile
{
public:
    OutputFile(pxr::HdRenderBuffer *renderBuffer);
    int write(const std::string &filename) const;

private:
    pxr::HdRenderBuffer *mRenderBuffer;
};

} // namespace hd_render

