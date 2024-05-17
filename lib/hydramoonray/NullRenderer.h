// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Renderer.h"

namespace hdMoonray {

class NullRenderer : public Renderer
{
public:
    NullRenderer();
    ~NullRenderer();

    void beginUpdate() override { mUpdateActive = true; }
    void endUpdate() override { mUpdateActive = false; }
    bool isUpdateActive() const override { return mUpdateActive; }

    void pause() override { mPaused = true; }
    void resume() override { mPaused = false; }
    bool isPaused() const override { return mPaused; }

    float getProgress() const override { return 1.0f; }
    float getElapsedSeconds() const override { return 0.0f; }
    bool isFrameComplete() const override { return true; }

    bool allocate(scene_rdl2::rdl2::RenderOutput*, PixelData&, const PixelSize&) override;
    bool resolve(scene_rdl2::rdl2::RenderOutput*, PixelData&) override;
    void deallocate(scene_rdl2::rdl2::RenderOutput*, PixelData&) override;

    void applySettings(const RenderSettings&) override {}

    void invalidateAllTextureResources() override {}
    void restartRenderer() override {}
    
private:
    bool mPaused = false;
    std::atomic<bool> mUpdateActive{true};
};

}
