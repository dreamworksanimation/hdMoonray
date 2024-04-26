// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "RndrRenderer.h"

#include <hydramoonray/RenderSettings.h>

#include <scene_rdl2/common/fb_util/VariablePixelBuffer.h>
#include <scene_rdl2/scene/rdl2/RenderOutput.h>
#include <moonray/rendering/mcrt_common/ExecutionMode.h>
#include <moonray/rendering/rndr/RenderContext.h>
#include <moonray/rendering/rndr/RenderOutputDriver.h>
#include <moonray/rendering/rndr/RenderProgressEstimation.h>

#include <iostream>

//#define DEBUG_MSG

namespace hdMoonray {

using scene_rdl2::logging::Logger;

RndrRenderer::RndrRenderer(uint32_t numThreads)
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc RndrRenderer()\n";
#   endif // end DEBUG_MSG

    // std::cout << "new RndrRenderer" << std::endl;
    mRenderOptions.reset(new moonray::rndr::RenderOptions);

    // we use 1 less than the default number of threads chosen by moonray,
    // to allow the app to run...
    // may want to use tbb::this_task_arena::max_concurrency() - 1 for Houdini setting?
    if (numThreads == 0) numThreads = mRenderOptions->getThreads() - 1;
    mRenderOptions->setThreads(numThreads);
    mRenderOptions->setRenderMode(moonray::rndr::RenderMode::PROGRESSIVE); // BATCH, PROGRESS_CHECKPOINT
    //mRenderOptions->setFps(1.0f);
    //mRenderOptions->setDesiredExecutionMode(scene_rdl2::mcrt_common::ExecutionMode::AUTO);
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [this](){ moonray::rndr::initGlobalDriver(*mRenderOptions); });

    mRenderContext.reset(new moonray::rndr::RenderContext(*mRenderOptions));
    mSceneContext = &mRenderContext->getSceneContext();
}

RndrRenderer::~RndrRenderer()
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc ~RndrRenderer()\n";
#   endif // end DEBUG_MSG

    invalidateAllTextureResources(); // cause textures to reload if run again
}

int
RndrRenderer::renderOutputIndex(scene_rdl2::rdl2::RenderOutput* ro) const
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc renderOutputIndex()\n";
#   endif // end DEBUG_MSG

    const auto *rod = mRenderContext->getRenderOutputDriver();
    const int numRenderOutputs = rod->getNumberOfRenderOutputs();
    for (int index = 0; index < numRenderOutputs; ++index) {
        if (ro == rod->getRenderOutput(index))
            return index;
    }
    return -1; // result of bad lpe or other error
}


// Due to problems with Hydra, the RenderOutput may be null, in which case the request must
// be used to get the type and size of the buffer.
bool
RndrRenderer::allocate(scene_rdl2::rdl2::RenderOutput* ro, PixelData& pd, const PixelSize& request)
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc allocate()\n";
#   endif // end DEBUG_MSG

    if (pd.mData) {
        // don't resize any existing buffer
        mResized = true;
    } else if (isBeauty(ro) && request.mChannels == 4) {
        renderBuffer.init(request.mWidth, request.mHeight);
        pd.mChannels = 4;
        pd.mWidth = request.mWidth;
        pd.mHeight = request.mHeight;
        pd.mData = renderBuffer.getData();
    } else {
        pd.mChannels = request.mChannels;
        if (ro) {
            int index = renderOutputIndex(ro);
            if (index >= 0) pd.mChannels = mRenderContext->getRenderOutputDriver()->getNumberOfChannels(index);
        }
        pd.mWidth = request.mWidth;
        pd.mHeight = request.mHeight;
        pd.vpb.init(
            scene_rdl2::fb_util::VariablePixelBuffer::Format(
                scene_rdl2::fb_util::VariablePixelBuffer::FLOAT + pd.mChannels - 1),
            pd.mWidth, pd.mHeight);
        pd.mData = pd.vpb.getData();
    }
    return true;
}


// HDM-183: It appears when Houdini wants to update the scene it will not call Sync()
// (which calls beginUpdate) until it is able to start a process on every tbb thread.
// This won't happen as long as the renderer is using a thread and thus it blocks
// until the render finishes.
//
// However while stuck it does a tight loop requesting the image buffers (and not
// showing them to the user). This detects it asking for the same image buffer
// twice without doing the isFrameCompleted test, and seems to reliably indicate
// that Houdini is stuck. It then calls stopFrame so the threads exit and Houdini
// can get started again.
static bool setPrevRo = false;
static scene_rdl2::rdl2::RenderOutput* prevRo = nullptr;

// warning: multithreaded
bool
RndrRenderer::resolve(scene_rdl2::rdl2::RenderOutput* ro, PixelData& pd)
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc resolve()\n";
#   endif // end DEBUG_MSG

    //std::cout << "RndrRenderer::resolve " << (ro ? ro->getName() : "beauty") << std::endl;

    if (isHoudini() && not mUpdateActive) { // HDM-183 detect Houdini-18 hanging
        if (not setPrevRo) {
            setPrevRo = true;
            prevRo = ro;
        } else if (ro == prevRo) {
            std::lock_guard<std::mutex> guard(mMutex);
            stopFrame();
            setPrevRo = false;
        }
    }

    if (mFailure) return false;

    if (not mRenderContext->isFrameReadyForDisplay()) {
        // tiles can have garbage in them before isFrameReadyForDisplay, avoid showing it
        if (mResized) return false; // resized array is always bad
        // if render has been stopped show it anyway, so animation works:
        if (not mUpdateActive && not mPaused) return false;
    }
    // see if no change since last time
    unsigned n = mRenderContext->getFilmActivity();
    unsigned oldN = pd.filmActivity;
    if (mFrameComplete) {
        // In order to figure out the render image update condition by film activity for re-render context,
        // we have to reset pd.filmActivity parameter at beginning of re-render because the
        // filmActivity inside renderContext is automatically reset to 0 when re-render is started.
        // However, I could not find reasonable places to do so.
        // One of the alternative ideas is reset pd.filmActivity in advance if the render is completed.
        pd.filmActivity = 0;    // reset
    } else {
        pd.filmActivity = n;
    }
    if (n == oldN) {
        return false; // render image is not updated.
    }

    mResized = false;

    if (isBeauty(ro)) {
        mRenderContext->snapshotRenderBuffer(&renderBuffer, true, true);
        pd.mChannels = 4;
        pd.mWidth = renderBuffer.getWidth();
        pd.mHeight = renderBuffer.getHeight();
        pd.mData = renderBuffer.getData();
        return true;
    }

    int index = renderOutputIndex(ro);
    if (index < 0) return false;
    const auto *rod = mRenderContext->getRenderOutputDriver();

    // read other buffers needed to perform snapshot
    if (rod->requiresRenderBuffer(index)) {
        //std::cout << "RenderOutput " << ro->getName() << " needs RenderBuffer\n";
        mRenderContext->snapshotRenderBuffer(&renderBuffer, true, true);
    }
    if (rod->requiresHeatMap(index)) {
        //std::cout << "RenderOutput " << ro->getName() << " needs HeapMap\n";
        mRenderContext->snapshotHeatMapBuffer(&heatMapBuffer, true, true);
    }
    if (rod->requiresWeightBuffer(index)) {
        //std::cout << "RenderOutput " << ro->getName() << " needs WeightBuffer\n";
        mRenderContext->snapshotWeightBuffer(&weightBuffer, true, true);
    }
    if (rod->requiresRenderBufferOdd(index)) {
        //std::cout << "RenderOutput " << ro->getName() << " needs RenderBufferOdd\n";
        mRenderContext->snapshotRenderBufferOdd(&renderBufferOdd, true, true);
    }

    mRenderContext->snapshotRenderOutput(
        &pd.vpb, index,
        &renderBuffer, &heatMapBuffer, &weightBuffer, &renderBufferOdd,
        true, true);
    pd.mChannels = unsigned(pd.vpb.getFormat()) - unsigned(scene_rdl2::fb_util::VariablePixelBuffer::FLOAT) + 1;
    pd.mWidth = pd.vpb.getWidth();
    pd.mHeight = pd.vpb.getHeight();
    pd.mData = pd.vpb.getData();
    return true;
}


void
RndrRenderer::deallocate(scene_rdl2::rdl2::RenderOutput* ro, PixelData& pd)
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc deallocate()\n";
#   endif // end DEBUG_MSG

    if (isBeauty(ro)) {
        renderBuffer.cleanUp();
    } else {
        // free other buffers that may have been allocated:
        heatMapBuffer.cleanUp();
        weightBuffer.cleanUp();
        renderBufferOdd.cleanUp();
    }
}


void
RndrRenderer::applySettings(const RenderSettings& settings)
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc applySettings()\n";
#   endif // end DEBUG_MSG
    static bool oldRestartToggle = false;
    static bool oldReloadTexturesToggle = false;
    setExecMode(settings.getExecutionMode());
    if (settings.getRestartToggle() != oldRestartToggle || settings.getReloadTexturesToggle() != oldReloadTexturesToggle) {
        oldRestartToggle = settings.getRestartToggle();
        oldReloadTexturesToggle = settings.getReloadTexturesToggle();
        // replicate restarting as much as possible (fixme: should run renderprep again)
        invalidateAllTextureResources();
        mFailure = false;
    }
}

void
RndrRenderer::invalidateAllTextureResources()
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc invalidateAllTextureResources()\n";
#   endif // end DEBUG_MSG

    try {
        mRenderContext->invalidateAllTextureResources();
    } catch (const std::exception &e) {
        // ignore "No Attribute named 'positive x texture'"
        Logger::warn(e.what());
    }
}

void
RndrRenderer::restartRenderer()
{
    // currently not supported in this mode
}

static std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::time_point::min();

float
RndrRenderer::getProgress() const
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc getProgress()\n";
#   endif // end DEBUG_MSG

    return mRenderContext->getFrameProgressFraction(nullptr, nullptr);
}

float
RndrRenderer::getElapsedSeconds() const
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc getElapsedSeconds()\n";
#   endif // end DEBUG_MSG

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::chrono::duration<float> d(now - startTime);
    return d.count();
}


void
RndrRenderer::setExecMode(std::string mode)
{   
    if (mExecMode == mode) return;
    if (mode ==  "auto"){
        mRenderOptions->setDesiredExecutionMode(moonray::mcrt_common::ExecutionMode::AUTO);
        mExecMode = mode;
    }
    else if (mode == "vectorized"){
        mRenderOptions->setDesiredExecutionMode(moonray::mcrt_common::ExecutionMode::VECTORIZED);
        mExecMode = mode;
    }
    else if (mode == "scalar"){
        mRenderOptions->setDesiredExecutionMode(moonray::mcrt_common::ExecutionMode::SCALAR);
        mExecMode = mode;
    }
    else if (mode == "xpu"){
        mRenderOptions->setDesiredExecutionMode(moonray::mcrt_common::ExecutionMode::XPU);
        mExecMode = mode;
    } else {
        std::cerr<< "Invalid Execution Mode " <<std::endl;
    }
}

// This is not thread safe, caller must hold mMutex
void
RndrRenderer::stopFrame() const
{
    if (mRenderContext->isFrameRendering()) // stopFrame will abort() if this is false
        mRenderContext->stopFrame();
}

// warning: multithreaded: Almost all the methods appear to be done by the same thread,
// however beginUpdate() may be done by a different thread. A mutex must be used
// around any parts where a simultaneous beginUpdate() would be bad.
void
RndrRenderer::beginUpdate()
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc beginUpdate()\n";
#   endif // end DEBUG_MSG

    if (not mUpdateActive) {
        std::lock_guard<std::mutex> guard(mMutex);
        if (not mUpdateActive) {
            stopFrame();
            mUpdateActive = true;
            mFrameComplete = false;
        }
    }
}

void
RndrRenderer::endUpdate()
{
    if (mUpdateActive && !mPaused) {
#       ifdef DEBUG_MSG
        std::cerr << ">> RndrRenderer.cc endUpdate()\n";
#       endif // end DEBUG_MSG

        std::lock_guard<std::mutex> guard(mMutex);
        if (mUpdateActive && !mPaused) {
            // std::cout << "endUpdate\n";
            mUpdateActive = false;
            mRenderContext->setSceneUpdated();
            if (not mRenderContext->isInitialized()) {
                std::stringstream initmessages; // dummy
                mRenderContext->initialize(initmessages);
            }
            try {
#               ifdef DEBUG_MSG
                std::cerr << ">> RndrRenderer.cc     before call mRenderContext->startFrame() <<<<<<<<<<<<<<\n";
#               endif // end DEBUG_MSG
                mRenderContext->startFrame();
#               ifdef DEBUG_MSG
                std::cerr << ">> RndrRenderer.cc     after call mRenderContext->startFrame() <<<<<<<<<<<<<<<<\n";
#               endif // end DEBUG_MSG
                mFailure = false;
            } catch (const std::exception &e) {
                mFailure = true;
                Logger::error(e.what());
            }
            startTime = std::chrono::steady_clock::now();
        }
    }
}

void
RndrRenderer::pause()
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc pause()\n";
#   endif // end DEBUG_MSG

    std::lock_guard<std::mutex> guard(mMutex);
    mPaused = true;
    stopFrame();
}

void
RndrRenderer::resume()
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc resume()\n";
#   endif // end DEBUG_MSG

    std::lock_guard<std::mutex> guard(mMutex);
    mPaused = false;
    if (not mFrameComplete) mUpdateActive = true; // endUpdate() will restart the render
}

bool
RndrRenderer::isFrameComplete() const
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RndrRenderer.cc isFrameComplete()\n";

    unsigned tmpFilmActivity = mRenderContext->getFilmActivity();
    char frameCompleteFlag = mRenderContext->isFrameComplete() ? 't' : 'f';
    char frameReadyForDisplay = mRenderContext->isFrameReadyForDisplay() ? 't' : 'f';
#   endif // end DEBUG_MSG

    setPrevRo = false; // HDM-183: detect Houdini is not "stuck"
    if (not mFrameComplete && not mUpdateActive) {
        std::lock_guard<std::mutex> guard(mMutex);
        if (not mUpdateActive && mRenderContext->isFrameComplete()) {
            mFrameComplete = true;
            stopFrame();
        }
    }

#   ifdef DEBUG_MSG
    if (!frameReadyForDisplay && frameCompleteFlag) {
        std::cerr << (mFrameComplete ? 'I' : 'i') << tmpFilmActivity
                  << ": display:" << frameReadyForDisplay
                  << " complete:" << frameCompleteFlag << '\n';
    }
#   endif // end DEBUG_MSG

    return mFrameComplete;
}

}
