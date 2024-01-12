// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "RenderPass.h"
#include "RenderBuffer.h"
#include "RenderDelegate.h"
#include "Camera.h"

#include <pxr/imaging/hd/renderPassState.h>
#include <scene_rdl2/scene/rdl2/Utils.h>
#include <moonray/rendering/rndr/RenderStatistics.h>

#include <iostream>
#include <chrono>

//#define DEBUG_MSG

namespace {

// On Linux steady_clock is the one to use. All have same precision (1/1000000000)
// but high_resolution_clock is the same as system_clock and possibly not steady.
std::chrono::steady_clock::time_point startTime;
std::chrono::steady_clock::time_point actualStartTime; // startTime when renderer actually updated
bool startTimeSet; // makes it ignore additional calls to setStartTime
bool previousPassCompleted = true; // did previous pass finish or run for more than 10 seconds

}

namespace hdMoonray
{

using scene_rdl2::logging::Logger;

RenderPass::~RenderPass()
{
    // std::cout << "~RenderPass()" << std::endl;
}

bool
RenderPass::IsConverged() const
{
    // This oddness is to work around a Hydra bug that has been reported to pixar.
    // It does not call RenderBuffer::Resolve after IsConverged returns true, so
    // it never shows the last generated image. Fix this by requiring IsConverged()
    // to be called twice to return true and disable the _Execute call between them.
    if (mDeferIsConverged) {
        // std::cout << "RenderPass::IsConverged() -> true" << std::endl;
#       ifdef DEBUG_MSG
        std::cerr << ">> RenderPass.cc RenderPass::IsConverged():true\n";
#       endif // end DEBUG_MSG
        return true;
    } else {
        if ((mDeferIsConverged = renderDelegate.renderer().isFrameComplete())) {
            // std::cout << "RenderPass::IsConverged() -> defer" << std::endl;
            std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - actualStartTime;
            std::cout << "Hydra total time = " << moonray::rndr::timeIntervalFormat(elapsed.count()) << std::endl;
            previousPassCompleted = true;
        }
        // else std::cout << "RenderPass::IsConverged() -> false" << std::endl;
#       ifdef DEBUG_MSG
        std::cerr << ">> RenderPass.cc RenderPass::IsConverged():false\n";
#       endif // end DEBUG_MSG
        return false;
    }
}

void
RenderPass::_MarkCollectionDirty()
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RenderPass.cc RenderPass::_MarkCollectionDirty()\n";
#   endif // end DEBUG_MSG

    // std::cout << "RenderPass::_MarkCollectionDirty\n";
}

void RenderDelegate::setStartTime() const
{
    if (not startTimeSet) {
        startTimeSet = true;
        startTime = std::chrono::steady_clock::now();
    }
}

void
RenderPass::_Sync()
{
#   ifdef DEBUG_MSG
    // std::cerr << ">> RenderPass.cc RenderPass::_Sync()\n";
#   endif // end DEBUG_MSG

    renderDelegate.setStartTime();
    // std::cout << "RenderPass::_Sync\n";
}

void
RenderPass::_Execute(const pxr::HdRenderPassStateSharedPtr& renderPassState,
                     const pxr::TfTokenVector& renderTags)
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RenderPass.cc RenderPass::_Execute()\n";
#   endif // end DEBUG_MSG

    //std::cout << "RenderPass::_Execute" << std::endl;
    // std::cout << " bindings =";
    // for (const auto& i : renderPassState->GetAovBindings()) std::cout << ' ' << i.aovName; std::cout << std::endl;
    // std::cout << " renderTags ="; for (auto&& i : renderTags) std::cout << ' ' << i; std::cout << std::endl;

    // Update for any changes in render settings, may create a new renderer
    renderDelegate.getRendererApplySettings();

    // Deal with changes to "purpose"
    renderDelegate.setRenderTags(GetRenderIndex(), renderTags);

    const scene_rdl2::rdl2::SceneContext& sc(renderDelegate.sceneContext());
    const scene_rdl2::rdl2::SceneVariables& sv(sc.getSceneVariables());

    int w, h;
#if PXR_VERSION >= 2102
    if (renderPassState->GetFraming().IsValid()) {
        const pxr::GfRect2i& dw(renderPassState->GetFraming().dataWindow);
        w = dw.GetWidth();
        h = dw.GetHeight();
    } else // older clients may use viewport instead of framing
#endif
    {   const pxr::GfVec4f& vp = renderPassState->GetViewport();
        w = vp[2];
        h = vp[3];
    }

    const Camera* camera(dynamic_cast<const Camera*>(renderPassState->GetCamera()));
    if (not camera) {
#       ifdef DEBUG_MSG
        std::cerr << ">> RenderPass.cc _Execute() no camera unsupported\n";
#       endif // end DEBUG_MSG
        Logger::error("RenderPassState without camera is unsupported");
        // It could the view+proj matricies below and update a fake Camera. But
        // usdview is not using this.
        return;
    }
    const_cast<Camera*>(camera)->setAsPrimaryCamera(renderDelegate, double(w)/h);

    float frame = 0;
    scene_rdl2::rdl2::FloatVector motionSteps(2);
    auto usdDelegate = renderDelegate.usdImagingDelegate();
    if (usdDelegate) {
        pxr::UsdTimeCode currTime = usdDelegate->GetTime();
        if (currTime.IsNumeric()) {
            frame = (float)currTime.GetValue();
            pxr::GfInterval interval = usdDelegate->GetCurrentTimeSamplingInterval();
            motionSteps[0] = float(interval.GetMin()) - frame;
            motionSteps[1] = float(interval.GetMax()) - frame;
        }
    } else {
        std::pair<float, float> interval =
            const_cast<Camera*>(camera)->getTimeSamplingInterval();
        motionSteps[0] = interval.first;
        motionSteps[1] = interval.second;
    }

    const bool motionBlur = renderDelegate.getEnableMotionBlur() &&
                            motionSteps[0] < motionSteps[1];
    if (not motionBlur) { motionSteps[0] = -1; motionSteps[1] = 0; }

    if (frame != sv.get(sv.sFrameKey) ||
        motionSteps != sv.get(sv.sMotionSteps) ||
        motionBlur != sv.get(sv.sEnableMotionBlur) ||
        w != sv.get(sv.sImageWidth) ||
        h != sv.get(sv.sImageHeight))
    {
#       ifdef DEBUG_MSG
        std::cerr << ">> RenderPass.cc _Execute() sceneVariables update\n";
#       endif // end DEBUG_MSG
        scene_rdl2::rdl2::SceneVariables& wsv(renderDelegate.acquireSceneContext().getSceneVariables());
        UpdateGuard guard(wsv);
        wsv.set(sv.sFrameKey, frame);
        wsv.set(sv.sMotionSteps, motionSteps);
        wsv.set(sv.sEnableMotionBlur, motionBlur);
        wsv.set(sv.sImageWidth, w);
        wsv.set(sv.sImageHeight, h);
    }

    // const pxr::GfMatrix4d& view = renderPassState->GetWorldToViewMatrix(); // same as camera->GetViewMatrix()
    // // This matrix contains info that is *not* in the Camera, to make the pixels square in the viewport:
    // const pxr::GfMatrix4d& proj = renderPassState->GetProjectionMatrix();

    // tell renderer about any new bindings
    const pxr::HdRenderPassAovBindingVector& aovBindings = renderPassState->GetAovBindings();
    for (const pxr::HdRenderPassAovBinding& aovBinding : aovBindings) {
        RenderBuffer* buffer = reinterpret_cast<RenderBuffer*>(aovBinding.renderBuffer);
#       ifdef DEBUG_MSG
        std::cerr << ">> RenderPass.cc _Execute() before buffer->bind()\n";
#       endif // end DEBUG_MSG        
        buffer->bind(aovBinding, camera);
#       ifdef DEBUG_MSG
        std::cerr << ">> RenderPass.cc _Execute() after buffer->bind()\n";
#       endif // end DEBUG_MSG        
    }

    if (renderDelegate.renderer().isUpdateActive()) {
#       ifdef DEBUG_MSG
        std::cerr << ">> RenderPass.cc _Execute() isUpdateActive()\n";
#       endif // end DEBUG_MSG        
        mDeferIsConverged = false;
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (previousPassCompleted || std::chrono::duration<double>(now - actualStartTime).count() > 10) {
            std::chrono::duration<double> elapsed = now - startTime;
            std::cout << "Hydra setup time = " << moonray::rndr::timeIntervalFormat(elapsed.count()) << std::endl;
            previousPassCompleted = false;
        }
        actualStartTime = startTime;
    }
    renderDelegate.renderer().endUpdate();
    startTimeSet = false;

    static std::string prevRdlaOutput;
    const std::string& rdlaOutput(renderDelegate.rdlaOutput());
    if (rdlaOutput != prevRdlaOutput) {
#       ifdef DEBUG_MSG
        std::cerr << ">> RenderPass.cc _Execute() rdlaOutput\n";
#       endif // end DEBUG_MSG        

        prevRdlaOutput = rdlaOutput;
        if (not rdlaOutput.empty()) {
            scene_rdl2::rdl2::writeSceneToFile(sc,
                                          rdlaOutput,
                                          false, // deltas
                                          true); // skip defaults
        }
    }
}

}

