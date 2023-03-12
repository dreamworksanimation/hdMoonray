// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Renderer.h"

namespace moonray { namespace rndr {
class RenderContext;
class RenderOptions;
}}
#include <scene_rdl2/common/fb_util/FbTypes.h>

namespace hdMoonray {

class RndrRenderer : public Renderer
{
public:
    RndrRenderer();
    ~RndrRenderer();

    bool activate() override;
    bool deactivate() override;

    RendererImplType getImplType() const override { return RendererImplType::Rndr; }

    void beginUpdate() override;
    void endUpdate() override;
    bool isUpdateActive() const override { return mUpdateActive; }

    void pause() override;
    void resume() override;
    bool isPaused() const override { return mPaused; }

    float getProgress() const override;
    float getElapsedSeconds() const override;
    bool isFrameComplete() const override;

    bool allocate(scene_rdl2::rdl2::RenderOutput*, PixelData&, const PixelSize&) override;
    bool resolve(scene_rdl2::rdl2::RenderOutput*, PixelData&) override;
    void deallocate(scene_rdl2::rdl2::RenderOutput*, PixelData&) override;

    void applySettings(const RenderSettings&) override;
private:
    std::unique_ptr<moonray::rndr::RenderOptions> mRenderOptions;
    std::unique_ptr<moonray::rndr::RenderContext> mRenderContext;

    bool mUpdateActive = true;
    bool mPaused = false;
    mutable bool mFrameComplete = false;
    bool mFailure = false;
    bool mResized = true;
    mutable std::mutex mMutex;

    int renderOutputIndex(scene_rdl2::rdl2::RenderOutput*) const;
    scene_rdl2::fb_util::RenderBuffer renderBuffer;
    scene_rdl2::fb_util::HeatMapBuffer heatMapBuffer;
    scene_rdl2::fb_util::FloatBuffer weightBuffer;
    scene_rdl2::fb_util::RenderBuffer renderBufferOdd;

    void invalidateAllTextureResources();
    void stopFrame() const;
};

}

