// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Geometry.h"

#include <pxr/imaging/hd/volume.h>
#include <pxr/imaging/hd/field.h>

namespace hdMoonray {

/// USD-style volume prim
class Volume final : public pxr::HdVolume, public Geometry
{
public:
    explicit Volume(pxr::SdfPath const& id INSTANCERID(const pxr::SdfPath& iid));
    const std::string& className(pxr::HdSceneDelegate* sceneDelegate) const override;

    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    void Sync(pxr::HdSceneDelegate* sceneDelegate,
              pxr::HdRenderParam*   renderParam,
              pxr::HdDirtyBits*     dirtyBits,
              const pxr::TfToken&   reprToken) override;

    void Finalize(pxr::HdRenderParam* renderParam) override;

protected:
    // Initialize one of the reprs, called before Sync()
    void _InitRepr(pxr::TfToken const &reprToken, pxr::HdDirtyBits *dirtyBits) override {}

    // Expand dirty bits to what Sync() actually needs
    pxr::HdDirtyBits _PropagateDirtyBits(pxr::HdDirtyBits bits) const override { return bits; }

private:
    // This class does not support copying.
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
