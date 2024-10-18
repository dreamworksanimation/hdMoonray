// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "GeometryMixin.h"
#include <pxr/imaging/hd/volume.h>
#include <pxr/imaging/hd/field.h>

namespace hdMoonray {

/// USD-style volume prim
class Volume final : public pxr::HdVolume, public GeometryMixin
{
public:
    explicit Volume(pxr::SdfPath const& id);

    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    void Sync(pxr::HdSceneDelegate* sceneDelegate,
              pxr::HdRenderParam*   renderParam,
              pxr::HdDirtyBits*     dirtyBits,
              const pxr::TfToken&   reprToken) override;

    void Finalize(pxr::HdRenderParam* renderParam) override;

protected:
    
    // Hydra overrides
    void _InitRepr(pxr::TfToken const &reprToken, pxr::HdDirtyBits *dirtyBits) override {}
    pxr::HdDirtyBits _PropagateDirtyBits(pxr::HdDirtyBits bits) const override { return bits; }

    // GeometryMixin overrides
    void syncAttributes(pxr::HdSceneDelegate* sceneDelegate, RenderDelegate& renderDelegate,
                        pxr::HdDirtyBits* dirtyBits, const pxr::TfToken& reprToken) override;
    
    bool isVolume() override { return true; }
    bool supportsUserData() override { return false; }

private:
    Volume(const Volume&)             = delete;
    Volume &operator =(const Volume&) = delete;
};

/// USD field prim, these belong to the volumes
class OpenVdbAsset final : public pxr::HdField
{
public:
    OpenVdbAsset(const pxr::SdfPath& id): pxr::HdField(id) {}

    void Sync(pxr::HdSceneDelegate*, pxr::HdRenderParam*, pxr::HdDirtyBits*) override;
    
    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override { return DirtyBits::AllDirty; }
    
    const std::string& filePath() const { return mFilePath; }
    pxr::TfToken name() const { return mName; }
    int index() const { return mIndex; }

private:
    std::string mFilePath;
    pxr::TfToken mName;
    int mIndex = -1;
};

}
