// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/imaging/hd/types.h>
#include <pxr/usd/sdf/path.h>

namespace hdMoonray {

void hdmLogSyncStart(const std::string& type, const pxr::SdfPath& id, pxr::HdDirtyBits *dirtyBits);
void hdmLogSyncEnd(const pxr::SdfPath& id);

void hdmLogRenderBuffer(const std::string& msg,const pxr::SdfPath& id);

void hdmLogArras(const std::string& msg);

}

