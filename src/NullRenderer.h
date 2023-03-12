// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Renderer.h"

namespace scene_rdl2 { namespace rndr {
class RenderContext;
}}
#include <scene_rdl2/common/fb_util/FbTypes.h>

namespace hdMoonray {

class NullRenderer : public Renderer
{
public:
    NullRenderer(scene_rdl2::rdl2::SceneContext* context);
    ~NullRenderer();

    RendererImplType getImplType() const override { return RendererImplType::Null; }

    void beginUpdate() override;
    void endUpdate() override;
    bool isUpdateActive() const override { return mUpdateActive; }

    void pause() override { mPaused = true; }
    void resume() override { mPaused = false; }
    bool isPaused() const override { return mPaused; }

    float getProgress() const override;
    float getElapsedSeconds() const override;
    bool isFrameComplete() const override;

    bool allocate(scene_rdl2::rdl2::RenderOutput*, PixelData&, const PixelSize&) override;
    bool resolve(scene_rdl2::rdl2::RenderOutput*, PixelData&) override;
    void deallocate(scene_rdl2::rdl2::RenderOutput*, PixelData&) override;

    void applySettings(const RenderSettings&) override;
private:
    bool mPaused = false;
    std::atomic<bool> mUpdateActive{true};
};

}

