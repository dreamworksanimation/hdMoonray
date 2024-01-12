// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scene_rdl2/scene/rdl2/SceneContext.h>

#include "PixelData.h"

namespace scene_rdl2 {
    namespace rdl2 { class RenderOutput; }
    namespace fb_util { class VariablePixelBuffer; }
}
namespace hdMoonray {

class RenderSettings;

/// This encapsulates differences in how to call Moonray/Arras for
/// different render settings, such as whether to use the renderfarm or not.
/// It is created by the first RenderPass::_Execute, and may be recreated
/// (as a differnt subclass) if the settings change.
///
/// If this really works it might be better to make it part of arras,
/// perhaps by adding the extra api to RenderContext. For this reason the
/// api will avoid any use of pxr objects.

enum class RendererImplType {
    Rndr, Arras, Null
};

class Renderer
{
public:
    Renderer() {};
    virtual ~Renderer() {};

    virtual bool activate() { return true; }
    virtual bool deactivate() { return true; }

    virtual RendererImplType getImplType() const=0;

    /// Pointer to sceneContext. You must call beginUpdate() if you want to modify it
    scene_rdl2::rdl2::SceneContext& getSceneContext() const { return *mSceneContext; }

    /// Called when changes are about to be made to the sceneContext. This may be
    /// called multiple times. This stops the local render, for remote it could send a
    /// stop signal.
    virtual void beginUpdate()=0;
    /// Start rendering the new version. For remote this transmits the deltas.
    virtual void endUpdate()=0;
    /// True if beginUpdate() has been called and endUpdate() not called yet
    virtual bool isUpdateActive() const=0;

    /// Temporarily stop doing any processing. May be called multiple times.
    virtual void pause()=0;
    /// Un-pause the render if paused.
    virtual void resume()=0;
    /// True between pause and resume
    virtual bool isPaused() const=0;

    /// Range from 0..1 estimation of how much of the render is done
    virtual float getProgress() const=0;
    /// Time since progress==0
    virtual float getElapsedSeconds() const=0;
    /// True when converged
    virtual bool isFrameComplete() const=0;

    /// A null RenderOutput* indicates the beauty buffer. This function should be used to test this
    /// so it can be grepped for:
    static bool isBeauty(const scene_rdl2::rdl2::RenderOutput* ro) { return not ro; }

    /// Update the PixelData to point to readable memory of correct size. Return false if none.
    /// The expected size and channels are in "request" in case this cannot otherwise be determined.
    virtual bool allocate(scene_rdl2::rdl2::RenderOutput*, PixelData&, const PixelSize& request) = 0;

    /// Update the PixelData to point to current image. This can alter the size, type, and data
    /// pointer from the result of the previous call and/or allocate(). This can return false on
    /// error or if it is believed the old image is the same or better.
    virtual bool resolve(scene_rdl2::rdl2::RenderOutput*, PixelData&) = 0;

    /// Free memory allocated by allocate() or resolve(). Destructor should also free anything not deallocated
    virtual void deallocate(scene_rdl2::rdl2::RenderOutput*, PixelData&) = 0;

    virtual void applySettings(const RenderSettings&)=0;
    void setIsHoudini(bool v) { mIsHoudini = v; }

protected:
    scene_rdl2::rdl2::SceneContext* mSceneContext;
    bool isHoudini() const { return mIsHoudini; }
private:
    bool mIsHoudini = false;
};

}

